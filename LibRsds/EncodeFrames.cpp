#include "EncodeFrames.h"
#include "Helpers.h"
#include "pngio.h"

#include <pcl/io/ply_io.h>
#include <pcl/io/pcd_io.h>

#include <filesystem>
#include <mutex>

using namespace common;
using namespace EF;

namespace fs = std::experimental::filesystem;

EncodeFrames::EncodeFrames ()
  : _thread(nullptr)
  , _mutex(nullptr)
  , _realsense(nullptr)
  , _is_thread_running(false)
  , _is_running (false)
{
}


EncodeFrames::~EncodeFrames ()
{
  Stop ();
}

void EncodeFrames::Run ( RS::RealsenseController * realsense, std::string path ) try
{
  Stop ();

  _is_running = false;  
  _path = path;
  _realsense = realsense;

  _mutex = new std::mutex ();

  _is_thread_running = true;
  _currentFrame = 0;

  _thread = new std::thread ( [&]()
  {
    ThreadRun ();
  } );
}
catch (const std::exception & e)
{
  DebugOut ( "EncodeFrames::Run exp: %s", e.what () );
  return;
}

void EncodeFrames::Stop ()
{
  if (_thread)
  {
    _is_running = false;
    _is_thread_running = false;

    _thread->join ();

    DEL ( _thread );
  }

  EmptyQueue ();

  DEL ( _mutex );
}

void EncodeFrames::QueueFrame ( 
  unsigned char * colorImage, int colorSize, 
  unsigned char * depthImage, int depthSize, 
  pcl::PointCloud<pcl::PointXYZRGB>* pcd )
{
  if (!_mutex || !_is_running || !_is_thread_running || !colorImage || !depthImage)
    return;

  EFrame frame;

  frame.colorImage = new unsigned char[colorSize];
  frame.colorSize = colorSize;
  memcpy ( frame.colorImage, colorImage, colorSize );

  frame.depthImage = new unsigned char[depthSize];
  frame.depthSize = depthSize;
  memcpy ( frame.depthImage, depthImage, depthSize );

  frame.pcd = new pcl::PointCloud<pcl::PointXYZRGB> ( *pcd );

  {   
    std::lock_guard<std::mutex> guard ( *_mutex );

    _queuedItems.push_back ( frame );
  }
}

int EncodeFrames::QueueCount ()
{
  std::lock_guard<std::mutex> guard ( *_mutex ); 
  
  return _queuedItems.size (); 
}

void EncodeFrames::ThreadRun ()
{
  EFrame item;

  _is_running = true;

  while (_is_thread_running)
  {
    while (_is_running)
    {
      {
        std::lock_guard<std::mutex> guard ( *_mutex );

        if (_queuedItems.size () == 0)
          continue;

        item = _queuedItems[0];

        _queuedItems.pop_front ();

        DebugOut ( "Saving %d with %d queuedItems remaining", _currentFrame, _queuedItems.size () );
      }

      // make sure the data is valid
      if (
        _realsense->GetColorWidth () == 0 ||
        _realsense->GetColorHeight () == 0 ||
        _realsense->GetDepthWidth () == 0 ||
        _realsense->GetDepthHeight () == 0 ||
        !item.colorImage || !item.depthImage ||
        !item.pcd || item.pcd->width == 0 || item.pcd->height == 0
        )
      {
        DebugOut ( "Invalid frame found" );
      }
      else
      {
        fs::path path = _path;
        fs::path colorFilename = path / Format ( "rgb\\%06d.png", _currentFrame );
        fs::path depthFilename = path / Format ( "depth\\%06d.png", _currentFrame );
        fs::path pcdFilename = path / Format ( "pcd\\%06d.ply", _currentFrame );

        _currentFrame++;

        pngio pngColor ( _realsense->GetColorWidth (), _realsense->GetColorHeight (), png_color_type::RGB );
        pngColor.WriteBlockAt ( 0, 0, _realsense->GetColorWidth (), _realsense->GetColorHeight (), item.colorImage );
        pngColor.Save ( colorFilename.string ().c_str () );

        pngio pngDepth ( _realsense->GetDepthWidth (), _realsense->GetDepthHeight (), png_color_type::GRAY );
        pngDepth.WriteBlockAt ( 0, 0, _realsense->GetDepthWidth (), _realsense->GetDepthHeight (), item.depthImage );
        pngDepth.Save ( depthFilename.string ().c_str () );

        pcl::io::savePLYFile < pcl::PointXYZRGB > ( pcdFilename.string (), *item.pcd, false );
        //pcl::io::savePCDFile < pcl::PointXYZRGB > ( pcdFilename.string (), *item.pcd, false );
      }      

      DEL ( item.colorImage );
      DEL ( item.depthImage );
      DEL ( item.pcd );
    }

    if (_is_thread_running)
      std::this_thread::sleep_for ( std::chrono::milliseconds ( 500 ) );
  }
}

void EncodeFrames::EmptyQueue ()
{
  if (!_mutex)
    return;

  {
    std::lock_guard<std::mutex> guard ( *_mutex );

    while (_queuedItems.size () > 0)
    {
      auto item = _queuedItems[0];

      DEL ( item.colorImage );
      DEL ( item.depthImage );
      DEL ( item.pcd );

      _queuedItems.pop_front ();
    }

  }
}
