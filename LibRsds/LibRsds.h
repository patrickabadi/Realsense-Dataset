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
      RS_Started,
      RS_Stopped,
      ErrorCameraUnplugged,
      Unknown,
    };

    enum class StateType
    {
      Initializing,
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
    void Start ();
    void Stop ();
    void Initialize ( String ^ folderPath, PFNStatusCallback ^ callback );
    void InvokeStatus ( String^ description );
    void InvokeStatus ( Status status );
    void InvokeStatus ( Status status, String^ description);
    void RealsenseCallback ( RS::RSState state );

  private:
    
    RS::RealsenseController * _realsense;
    static RsDsController^ _instance;
    String^ _folderPath;

    std::string RsDsController::ToString ( String ^ str );
    
  };
}
