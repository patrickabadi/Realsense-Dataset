#pragma once

using namespace System;

namespace RSDS
{
  public ref class RsDsController
  {
    delegate void PFNStatusCallback ( String^ description );

    RsDsController ();
    ~RsDsController ();

    void Reset ();
    void Start ();
    void Stop ();
    void Initialize ( String ^ folderPath, PFNStatusCallback ^ callback );
  };
}
