#pragma once

#include "RealsenseController.h"

#include <string>
#include <deque>

using namespace RS;

namespace EF
{
  struct EFrame
  {
    unsigned char* colorImage;
    int colorSize;
    unsigned char* depthImage;
    int depthSize;
  };

  class EncodeFrames
  {
  public:
    EncodeFrames ();
    ~EncodeFrames ();

    void Run ( RealsenseController* realsense, std::string path );
    void Stop ();
    void QueueFrame ( unsigned char * colorImage, int colorSize, unsigned char * depthImage, int depthSize );
    bool IsRunning () { return _is_running; }
    int QueueCount ();

  private:
    std::thread* _thread;
    std::mutex* _mutex;
    std::string _path;
    std::deque<EFrame> _queuedItems;
    RealsenseController* _realsense;
    bool _is_running;
    bool _is_thread_running;
    int _currentFrame;

    void ThreadRun ();
    void EmptyQueue ();
  };
}

