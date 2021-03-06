#pragma once



#define NOMINMAX
#include <windows.h>
#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>

using namespace System;
using namespace System::Windows;
using namespace System::Windows::Interop;
using namespace System::Windows::Input;
using namespace System::Runtime::InteropServices;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;
using namespace System::Windows::Media::Imaging;




namespace RS
{
  class RealsenseController;
  enum RSState : int;
}

namespace EF
{
  class EncodeFrames;
}

namespace RSDS
{
  public ref class RsDsController
  {
  public:
    enum class Status
    {
      Initializing,
      Finalizing,
      Completed,
      Started,
      Stopped,
      ErrorCameraUnplugged,
      Unknown,
    };

    enum class StateType
    {
      Initializing,
      Initialized,
      Started,
      Stopping,
      Stopped,
    };

    delegate void PFNStatusCallback ( Status status, String^ description );

    PFNStatusCallback^ StatusCallback;

    StateType State;

    RsDsController ();
    ~RsDsController ();

    static RsDsController^ Instance () { return _instance; }

    void Reset ();
    void Start ( String ^ folderPath, float targetFPS);
    void Stop ();
    void Initialize ( PFNStatusCallback ^ callback );
    void InvokeStatus ( String^ description );
    void InvokeStatus ( Status status );
    void InvokeStatus ( Status status, String^ description);
    void InvokeStatus ( std::string description );
    void RealsenseCallback ( RS::RSState state );
    bool ProcessFrame ();
    void ProcessBitmapImage ();

    WriteableBitmap^ GetColorBitmap () { return _color_bitmap; }
    WriteableBitmap^ GetDepthBitmap () { return _depth_bitmap; }

  private:
    
    RS::RealsenseController * _realsense;
    EF::EncodeFrames* _encode_frames;
    static RsDsController^ _instance;
    String^ _folderPath;
    unsigned char* _colorImage;
    unsigned char* _depthImage;

    int _framesAquired;
    int _framesEncoded;
    int _currentFrame;
    int _lastQueueRemaining;
    float _target_fps;
   
    WriteableBitmap^ _color_bitmap;
    WriteableBitmap^ _depth_bitmap;

    std::string ToString ( String ^ str );
    std::string GetFullPath ();
  
  };
}
