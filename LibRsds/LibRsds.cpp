#include "stdafx.h"
#include "Helpers.h"
#include "RealsenseController.h"

#include "LibRsds.h"

using namespace RSDS;
using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;
using namespace System::Windows::Threading;

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
  , State(StateType::Stopped)
{
  _instance = this;
}

RsDsController::~RsDsController ()
{
  DEL ( _realsense );
}

void RsDsController::Reset ()
{

}
void RsDsController::Start ()
{
  State = StateType::Initializing;
  InvokeStatus ( Status::Initializing, "RsDsController::Start" );

  if (!_realsense)
  {
    _realsense = new RS::RealsenseController ();
    _realsense->StateCallback = RSStateCallback;
  }

  _realsense->Start ();
}
void RsDsController::Stop ()
{
  if (!_realsense)
    return;

  State = StateType::Stopping;
  InvokeStatus ( Status::Finalizing, "RsDsController::Stop" );

  _realsense->Stop ();
}
void RsDsController::Initialize ( String ^ folderPath, PFNStatusCallback ^ callback )
{
  StatusCallback = callback;

  _folderPath =  folderPath;

  auto path = ToString ( _folderPath );
  RemoveDirectoryA ( path.c_str () );
  CreateDirectoryA ( path.c_str (), nullptr );
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
    State = StateType::Started;
    InvokeStatus ( Status::RS_Started );
    break;
  case RS::RSState::Stopped:
    State = StateType::Stopped;
    InvokeStatus ( Status::RS_Stopped );
    break;
  }
}

std::string RsDsController::ToString ( String ^ str )
{
  char* basePath = (char*)Marshal::StringToHGlobalAnsi ( str ).ToPointer ();

  std::string converted = basePath;

  Marshal::FreeHGlobal ( (IntPtr)basePath );

  return converted;

}


