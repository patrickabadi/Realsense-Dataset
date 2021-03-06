#include "stdafx.h"
#include "Helpers.h"
#include "RealsenseController.h"
#include "EncodeFrames.h"

#include "LibRsds.h"

#include <filesystem>
#include <tuple>

using namespace EF;
using namespace RSDS;
using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;
using namespace System::Windows::Threading;

namespace fs = std::experimental::filesystem;

std::chrono::time_point<std::chrono::steady_clock> frameTime;

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
  , _encode_frames(nullptr)
  , _colorImage(nullptr)
  , _depthImage(nullptr)
  , State(StateType::Stopped)
  , _target_fps(2.0f)
  , _lastQueueRemaining(0)
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

  if (_encode_frames)
    _encode_frames->Stop ();

  DEL ( _realsense );
  DEL ( _encode_frames );
  DEL ( _colorImage );
  DEL ( _depthImage );

}
void RsDsController::Start ( String ^ folderPath, float targetFPS )
{ 
  _currentFrame = 0; 
  _target_fps = targetFPS;

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

  if (!_encode_frames)
  {
    _encode_frames = new EncodeFrames ();
  }

  _encode_frames->Run ( _realsense, GetFullPath () );

  _lastQueueRemaining = 0;

  State = StateType::Started;
  InvokeStatus ( Status::Started, "RsDsController::Started" );

  frameTime = std::chrono::high_resolution_clock::now ();
}
void RsDsController::Stop ()
{
  State = StateType::Stopping;
  InvokeStatus ( Status::Finalizing, "RsDsController::Stop" );
  
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

bool RsDsController::ProcessFrame ()
{
  if (State == StateType::Stopping)
  {
    int count = _encode_frames->QueueCount ();
    if (count == 0)
    {
      _encode_frames->Stop ();
      State = StateType::Stopped;
      InvokeStatus ( Status::Stopped, "RsDsController::Stop" );
    }
    else if(_lastQueueRemaining != count)
    {
      _lastQueueRemaining = count;
      InvokeStatus ( Format("Queued frames remaining: %d...", count) );
    }
    return false;
  }

  if (State != StateType::Started)
    return false;

  if (!_encode_frames->IsRunning ())
    return false;

  auto now = std::chrono::high_resolution_clock::now ();
  auto ms = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(now - frameTime).count ()) / 1000.0f;

  auto fps = 1.0f / ms;

  if (fps > _target_fps)
  {
    return false;
  }

  frameTime = now;

  int colorSize = _realsense->GetColorHeight () * _realsense->GetColorWidth () * 3;
  int depthSize = _realsense->GetDepthHeight () * _realsense->GetDepthWidth () * 2;

  if (!_colorImage)
  {    
    _colorImage = new unsigned char[colorSize];
  }

  if (!_depthImage)
  {
    _depthImage = new unsigned char[depthSize];
  }

  bool success;

  success = _realsense->EncodeFrame ( _colorImage, _depthImage );
  _framesAquired = _realsense->GetFramesAcquired ();
  _framesEncoded = _realsense->GetFramesEncoded ();

  if (!success)
  {
    DebugOut ( "realsense Encode  failed" );
    return false;
  }

  DebugOut ( "Aquired: %d Encoded: %d", _framesAquired, _framesEncoded );

  _encode_frames->QueueFrame ( _colorImage, colorSize, _depthImage, depthSize );
  
  return true;
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



