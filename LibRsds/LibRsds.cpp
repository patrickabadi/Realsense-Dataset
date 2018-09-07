#include "stdafx.h"
#include "Helpers.h"
#include "RealsenseController.h"

#include "LibRsds.h"

#include <filesystem>
#include <tuple>

using namespace RSDS;
using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;
using namespace System::Windows::Threading;

namespace fs = std::experimental::filesystem;

delegate void MyCallback ( RsDsController::Status status, String^ description, RsDsController^ pThis );

void TryMyCallback ( RsDsController::Status status, String^ description, RsDsController^ pThis )
{
  if (pThis->StatusCallback)
    pThis->StatusCallback ( status, description );
}

void RSStateCallback ( RS::RSState state )
{
  DebugOut ( "StateCallback %d", state );

  RsDsController::Instance ()->RealsenseCallback ( state );
}

RsDsController::RsDsController ()
  : _realsense(nullptr)
  , _colorImage(nullptr)
  , _depthImage(nullptr)
  , _pngColor (nullptr)
  , _pngDepth (nullptr)
  , State(StateType::Stopped)
{
  _instance = this;
}

RsDsController::~RsDsController ()
{
  Reset ();
}

void RsDsController::Reset ()
{
  if(_realsense)
    _realsense->Stop ();

  DEL ( _realsense );
  DEL ( _colorImage );
  DEL ( _depthImage );
  DEL ( _pngColor );
  DEL ( _pngDepth );
}
void RsDsController::Start ( String ^ folderPath )
{
  

  _currentFrame = 0;  

  _color_bitmap = gcnew WriteableBitmap (
    _realsense->GetColorWidth (), _realsense->GetColorHeight (), 96.0, 96.0,
    System::Windows::Media::PixelFormats::Rgb24,
    nullptr );

  _depth_bitmap = gcnew WriteableBitmap ( 
    _realsense->GetDepthWidth(), _realsense->GetDepthHeight(), 96.0, 96.0,
    System::Windows::Media::PixelFormats::Rgb24,
    nullptr );

  try
  {
    _folderPath = folderPath;

    fs::path path = GetFullPath ();

    InvokeStatus ( Format ( "RsDsController::Initialize with path: %s", path.string ().c_str () ) );

    fs::remove_all ( path );
    fs::create_directory ( path );

    fs::path colorPath = path / "rgb";
    fs::path depthPath = path / "depth";

    fs::create_directory ( colorPath );
    fs::create_directory ( depthPath );
  }
  catch (const std::exception & e)
  {
    DebugOut ( "RsDsController::Start exp: %s", e.what () );
  }

  State = StateType::Started;
  InvokeStatus ( Status::Started, "RsDsController::Started" );
}
void RsDsController::Stop ()
{
  State = StateType::Stopped;
  InvokeStatus ( Status::Stopped, "RsDsController::Stop" );
  
}

void RsDsController::Initialize ( PFNStatusCallback ^ callback )
{

  StatusCallback = callback;

  State = StateType::Initializing;
  InvokeStatus ( Status::Initializing, "RsDsController::Start" );  

  if (!_realsense)
  {
    _realsense = new RS::RealsenseController ();
    _realsense->StateCallback = RSStateCallback;
  }

  _realsense->Start ();

}

void RsDsController::InvokeStatus ( String^ description )
{
  InvokeStatus ( Status::Unknown, description );
}

void RsDsController::InvokeStatus ( Status status )
{
  InvokeStatus ( status, "" );
}

void RsDsController::InvokeStatus ( Status status, String^ description )
{
  MyCallback^ c = gcnew MyCallback ( TryMyCallback );

  Application::Current->Dispatcher->BeginInvoke ( c, gcnew array<System::Object^>{status, description, this} );
}

void RSDS::RsDsController::InvokeStatus ( std::string description )
{
  InvokeStatus ( gcnew String ( description.c_str () ) );
}

void RsDsController::RealsenseCallback ( RS::RSState state )
{
  DebugOut ( "Realsense StateCallback %d", state );

  switch (state)
  {
  case RS::RSState::ErrorUnplugged:
    State = StateType::Stopped;
    InvokeStatus ( Status::ErrorCameraUnplugged );
    break;
  case RS::RSState::Started:
    State = StateType::Initialized;
    InvokeStatus ( Status::Completed );
    break;
  case RS::RSState::Stopped:
    State = StateType::Stopped;
    break;
  }
}

void RsDsController::ProcessFrame ()
{
  if (State != StateType::Started)
    return;

  if (!_colorImage)
  {
    int size = _realsense->GetColorHeight () * _realsense->GetColorWidth () * 3;
    _colorImage = new unsigned char[size];
  }

  if (!_depthImage)
  {
    int size = _realsense->GetDepthHeight () * _realsense->GetDepthWidth () * 2;
    _depthImage = new unsigned char[size];
  }

  bool success;

  success = _realsense->EncodeFrame ( _colorImage, _depthImage );
  _framesAquired = _realsense->GetFramesAcquired ();
  _framesEncoded = _realsense->GetFramesEncoded ();

  if (!success)
  {
    DebugOut ( "realsense Encode  failed" );
    return;
  }

  DebugOut ( "Aquired: %d Encoded: %d", _framesAquired, _framesEncoded );

  fs::path path = GetFullPath ();
  fs::path colorFilename = path / Format ( "rgb\\%06d.png", _currentFrame );
  fs::path depthFilename = path / Format ( "depth\\%06d.png", _currentFrame );

  _currentFrame++;

  pngio pngColor ( _realsense->GetColorWidth (), _realsense->GetColorHeight (), png_color_type::RGB );
  pngColor.WriteBlockAt ( 0, 0, _realsense->GetColorWidth (), _realsense->GetColorHeight (), _colorImage );
  pngColor.Save ( colorFilename.string ().c_str() );

  pngio pngDepth ( _realsense->GetDepthWidth (), _realsense->GetDepthHeight (), png_color_type::GRAY );
  pngDepth.WriteBlockAt ( 0, 0, _realsense->GetDepthWidth (), _realsense->GetDepthHeight (), _depthImage );
  pngDepth.Save ( depthFilename.string ().c_str () );

  
}

void RsDsController::ProcessBitmapImage ()
{
  _color_bitmap->Lock ();
  unsigned char* ptr = reinterpret_cast<unsigned char*>((_color_bitmap->BackBuffer).ToPointer ());
  _realsense->FillColorBitmap ( ptr );
  _color_bitmap->AddDirtyRect ( Int32Rect ( 0, 0, _realsense->GetColorWidth (), _realsense->GetColorHeight () ) );
  _color_bitmap->Unlock ();

  _depth_bitmap->Lock ();
  ptr = reinterpret_cast<unsigned char*>((_depth_bitmap->BackBuffer).ToPointer ());
  _realsense->FillDepthBitmap ( ptr, true );
  _depth_bitmap->AddDirtyRect ( Int32Rect ( 0, 0, _realsense->GetDepthWidth (), _realsense->GetDepthHeight () ) );
  _depth_bitmap->Unlock ();
}

std::string RsDsController::ToString ( String ^ str )
{
  char* basePath = (char*)Marshal::StringToHGlobalAnsi ( str ).ToPointer ();

  std::string converted = basePath;

  Marshal::FreeHGlobal ( (IntPtr)basePath );

  return converted;

}

std::string RsDsController::GetFullPath ()
{
  fs::path path = fs::current_path();
  
  path /= ToString ( _folderPath );

  return path.string();
}


