
#define NOMINMAX
#include "RealsenseController.h"
#include "Helpers.h"

#include <librealsense2/rs.hpp>

using namespace RS;


float get_depth_scale (rs2::device dev);
rs2_stream find_stream_to_align (const std::vector<rs2::stream_profile>& streams);
bool profile_changed (const std::vector<rs2::stream_profile>& current, const std::vector<rs2::stream_profile>& prev);

RealsenseController::RealsenseController ()
  : _depth_width (1280)
  , _depth_height (720)
  , _color_width (1280)
  , _color_height (720)
  , _target_fps (30)
  , _is_running (false)
  , _is_thread_running (false)
  , _thread (nullptr)
  , _mutex (nullptr)
  , _frame_aquired_count (0)
  , _frame_encoded_count (0)
  , _color_frame_queue (nullptr)
  , _depth_frame_queue (nullptr)
  , _color_frame (nullptr)
  , _depth_frame (nullptr)
  , _colorizer(nullptr)
  , _pc (nullptr)
  , StateCallback(nullptr)
  , _deviceType (DeviceType::Unknown)
{

}

RealsenseController::~RealsenseController ()
{
  StateCallback = nullptr;
  Stop (true);
}

bool RealsenseController::Start () try
{
  if (_is_thread_running && _is_running)
  {
    InvokeState ( RSState::Started );
    return true;
  }

  _is_running = true;

  if (_is_thread_running)
  {
    InvokeState ( RSState::Started );
    return true;
  }

  _mutex = new std::mutex ();
  _color_frame_queue = new rs2::frame_queue ();
  _depth_frame_queue = new rs2::frame_queue ();
  _color_frame = new rs2::frame ();
  _depth_frame = new rs2::frame ();
  _colorizer = new rs2::colorizer ();  
  _pc = new rs2::pointcloud ();

  _frame_aquired_count = 0;
  _frame_encoded_count = 0;

  _is_thread_running = true;  

  _thread = new std::thread ([&]()
  {
    ThreadRun ();
  });

  return _is_running;
}
catch (const rs2::error & e)
{
  DebugOut ("RealsenseController::Start realsense exp: %s", e.what());
  return false;
}
catch (const std::exception & e)
{
  DebugOut ("RealsenseController::Start exp: %s", e.what());
  return false;
}

void RealsenseController::ThreadRun () try
{
  rs2::config cfg;
  rs2::colorizer colorizer;

  cfg.enable_stream (RS2_STREAM_COLOR, _color_width, _color_height, RS2_FORMAT_RGB8, _target_fps);
  cfg.enable_stream (RS2_STREAM_DEPTH, _depth_width, _depth_height, RS2_FORMAT_Z16, _target_fps);
  cfg.enable_stream (RS2_STREAM_INFRARED, _depth_width, _depth_height, RS2_FORMAT_Y8, _target_fps);

  rs2::pipeline pipe;

  auto profile = pipe.start (cfg);

  auto intrinsics = profile.get_stream (RS2_STREAM_DEPTH).as<rs2::video_stream_profile> ().get_intrinsics ();
  _depth_intrinsics.fx = intrinsics.fx;
  _depth_intrinsics.fy = intrinsics.fy;
  _depth_intrinsics.height = intrinsics.height;
  _depth_intrinsics.width = intrinsics.width;
  _depth_intrinsics.ppx = intrinsics.ppx;
  _depth_intrinsics.ppy = intrinsics.ppy;

  intrinsics = profile.get_stream (RS2_STREAM_COLOR).as<rs2::video_stream_profile> ().get_intrinsics ();
  _color_intrinsics.fx = intrinsics.fx;
  _color_intrinsics.fy = intrinsics.fy;
  _color_intrinsics.height = intrinsics.height;
  _color_intrinsics.width = intrinsics.width;
  _color_intrinsics.ppx = intrinsics.ppx;
  _color_intrinsics.ppy = intrinsics.ppy;

  auto depth_stream = profile.get_stream (RS2_STREAM_DEPTH);
  auto color_stream = profile.get_stream (RS2_STREAM_COLOR);
  auto ext = depth_stream.get_extrinsics_to (color_stream);

  for (int i = 0; i < 9; i++)
    _extrinsics.rotation[i] = ext.rotation[i];

  for (int i = 0; i < 3; i++)
    _extrinsics.translation[i] = ext.translation[i];

  // Each depth camera might have different units for depth pixels, so we get it here
  // Using the pipeline's profile, we can retrieve the device that the pipeline uses
  _depth_scale = get_depth_scale (profile.get_device ());

  // figure out the device type (D435, D415)
  _deviceType = GetDeviceType ( profile.get_device () );

  //Pipeline could choose a device that does not have a color stream
  //If there is no color stream, choose to align depth to another stream
  auto align_to = find_stream_to_align (profile.get_streams ());

  // Create a rs2::align object.
  // rs2::align allows us to perform alignment of depth frames to others frames
  //The "align_to" is the stream type to which we plan to align depth frames.
  auto align = rs2::align (align_to);

  InvokeState (RSState::Started);

  while (_is_thread_running)
  {
    while (_is_running)
    {
      // Using the align object, we block the application until a frameset is available
      rs2::frameset frameset = pipe.wait_for_frames ();

      // synchronise so both queues are in step
      {
        std::lock_guard<std::mutex> guard ( *_mutex );

        auto color_frame = frameset.get_color_frame ();
        auto depth_frame = frameset.get_depth_frame ();

        _color_frame_queue->enqueue ( color_frame );
        _depth_frame_queue->enqueue ( depth_frame );

        _frame_aquired_count++;
      }      
    }

    if (_is_thread_running)
      std::this_thread::sleep_for ( std::chrono::milliseconds ( 500 ) );
  }
  

  pipe.stop ();
  cfg.disable_all_streams ();

  InvokeState (RSState::Stopped);
}
catch (const rs2::error & e)
{
  DebugOut ("RealsenseController::ThreadRun realsense exp: %s", e.what ());

  InvokeState (RSState::ErrorUnplugged);
  
}
catch (const std::exception & e)
{
  DebugOut ("RealsenseController::ThreadRun exp: %s", e.what ());

  InvokeState (RSState::ErrorUnplugged);
  
}

void RealsenseController::Stop (bool fullStop)
{
  if (!_thread || !_is_thread_running)
    return;

  _is_running = false;

  // this is used to keep the polling thread running but stop the polling.  Originally we were trying to prevent the exception: WinRT originate error - 0xC00D36B3 : 'The stream number provided was invalid.'.
  // turns out it's a system exception we can ignore
  // I'm leaving it in because maybe in the future we'll decide to keep the device open
  /*if (!fullStop)
    return;*/

  _is_thread_running = false;

  _thread->join ();

  delete _thread;
  _thread = nullptr;

  DEL (_mutex);
  DEL (_color_frame_queue);
  DEL (_depth_frame_queue);
  DEL (_color_frame);
  DEL (_depth_frame);
  DEL (_colorizer);
  DEL (_pc);
}

bool RealsenseController::ProcessFrame () try
{
  if (!_is_running || !_color_frame_queue || !_depth_frame_queue || !_thread || !_mutex)
    return false;

  bool ret = false;

  {
    std::lock_guard<std::mutex> guard (*_mutex);

    if (_color_frame_queue->poll_for_frame (_color_frame) && _depth_frame_queue->poll_for_frame(_depth_frame))
    {
      ret = true;
    }
  }

  return ret;

}
catch (const rs2::error & e)
{
  DebugOut ("RealsenseController::ProcessFrame realsense exp: %s", e.what ());
  return false;
}
catch (const std::exception & e)
{
  DebugOut ("RealsenseController::ProcessFrame exp: %s", e.what ());
  return false;
}

void RealsenseController::FillColorBitmap (unsigned char* pImage)
{
  if (!pImage || !_color_frame)
    return;

  rs2::video_frame* vf = reinterpret_cast<rs2::video_frame*>(_color_frame);

  unsigned char* pColorFrame = reinterpret_cast<unsigned char*>(const_cast<void*>(vf->get_data ()));

  int size = vf->get_bytes_per_pixel () * vf->get_width () * vf->get_height ();

  memcpy (pImage, pColorFrame, size);
}

bool RealsenseController::FillDepthBitmap (unsigned char* pImage, bool colorize)
{
  if (!pImage || !_depth_frame)
    return true;  

  bool validDepth = true;
  

  if (colorize)
  {
    rs2::video_frame f = _colorizer->colorize (*_depth_frame);

    auto pDepthFrame = reinterpret_cast<unsigned char*>(const_cast<void*>(f.get_data ()));

    auto size = f.get_bytes_per_pixel () * f.get_width () * f.get_height ();

    memcpy (pImage, pDepthFrame, size);
  }
  else
  {
    rs2::depth_frame* vf = reinterpret_cast<rs2::depth_frame*>(_depth_frame);

    auto pDepthFrame = reinterpret_cast<unsigned short*>(const_cast<void*>(vf->get_data ()));

    auto size = vf->get_bytes_per_pixel () * vf->get_width () * vf->get_height ();

    memcpy ( pImage, pDepthFrame, size );

  }     

  return validDepth;
}

bool RealsenseController::EncodeFrame ( unsigned char* pColor, unsigned char* pDepth)
{
  if (!_is_running || !_color_frame_queue || !_depth_frame_queue || !_thread || !_mutex)
    return false;

  bool ret = false;

  {
    std::lock_guard<std::mutex> guard ( *_mutex );

    if (_color_frame_queue->poll_for_frame ( _color_frame ) && _depth_frame_queue->poll_for_frame ( _depth_frame ))
    {
      _frame_encoded_count++;
      ret = true;
    }
  }

  if(!ret)
    return false;

  FillColorBitmap ( pColor );
  FillDepthBitmap ( pDepth, false );

  return true;
}

void RealsenseController::InvokeState (RSState state)
{
  if (StateCallback)
    StateCallback (state);
}

DeviceType RealsenseController::GetDeviceType (const rs2::device& dev )
{
  auto info = dev.get_info ( rs2_camera_info::RS2_CAMERA_INFO_NAME );
  if (!info || strlen ( info ) == 0)
    return DeviceType::Unknown;

  if (strstr ( info, "D435" ) != nullptr)
  {
    return DeviceType::D435;
  }
  else if (strstr ( info, "D415" ) != nullptr)
  {
    return DeviceType::D415;
  }
  else
  {
    return DeviceType::Unknown;
  }

}


float get_depth_scale (rs2::device dev)
{ 
  // Go over the device's sensors
  for (rs2::sensor& sensor : dev.query_sensors ())
  {
    // Check if the sensor is on
    if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor> ())
    {
      return 10000*dpt.get_depth_scale ();      
    }
  }
  throw std::runtime_error ("Device does not have a depth sensor");
}

rs2_stream find_stream_to_align (const std::vector<rs2::stream_profile>& streams)
{
  //Given a vector of streams, we try to find a depth stream and another stream to align depth with.
  //We prioritize color streams to make the view look better.
  //If color is not available, we take another stream that (other than depth)
  rs2_stream align_to = RS2_STREAM_ANY;
  bool depth_stream_found = false;
  bool color_stream_found = false;
  for (rs2::stream_profile sp : streams)
  {
    rs2_stream profile_stream = sp.stream_type ();
    if (profile_stream != RS2_STREAM_DEPTH)
    {
      if (!color_stream_found)         //Prefer color
        align_to = profile_stream;

      if (profile_stream == RS2_STREAM_COLOR)
      {
        color_stream_found = true;
      }
    }
    else
    {
      depth_stream_found = true;
    }
  }

  if (!depth_stream_found)
    throw std::runtime_error ("No Depth stream available");

  if (align_to == RS2_STREAM_ANY)
    throw std::runtime_error ("No stream found to align with Depth");

  return align_to;
}

bool profile_changed (const std::vector<rs2::stream_profile>& current, const std::vector<rs2::stream_profile>& prev)
{
  for (auto&& sp : prev)
  {
    //If previous profile is in current (maybe just added another)
    auto itr = std::find_if (std::begin (current), std::end (current), [&sp](const rs2::stream_profile& current_sp) { return sp.unique_id () == current_sp.unique_id (); });
    if (itr == std::end (current)) //If it previous stream wasn't found in current
    {
      return true;
    }
  }
  return false;
}
