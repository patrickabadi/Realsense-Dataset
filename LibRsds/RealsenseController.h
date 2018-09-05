#pragma once

namespace rs2
{
  class pipeline;
  class align;
  class pipeline_profile;
  class frame;
  class frame_queue;
  class colorizer;  
  class pointcloud;
  class points;
  class device;
}

namespace std
{
  class thread;
  class mutex;
  
}

struct rs_intrinsics
{
  int           width;     /**< Width of the image in pixels */
  int           height;    /**< Height of the image in pixels */
  float         ppx;       /**< Horizontal coordinate of the principal point of the image, as a pixel offset from the left edge */
  float         ppy;       /**< Vertical coordinate of the principal point of the image, as a pixel offset from the top edge */
  float         fx;        /**< Focal length of the image plane, as a multiple of pixel width */
  float         fy;        /**< Focal length of the image plane, as a multiple of pixel height */
};

struct rs_extrinsics
{
  float rotation[9];    /**< Column-major 3x3 rotation matrix */
  float translation[3]; /**< Three-element translation vector, in meters */
};

enum rs2_stream : int;

struct volume_bounds
{
  volume_bounds ()
    : width (1000)
    , height (1000)
    , depth (1000)
    , z_translate (950)
  {  }
  float width;
  float height;
  float depth;
  float z_translate;
};


namespace RS
{
  enum RSState
  {
    Started,
    Stopped,
    ErrorUnplugged,
  };

  enum DeviceType
  {
    D435,
    D415,
    Unknown,
  };

  class RealsenseController
  {
  public:      


    typedef void (*StateCallbackFn)(RSState);

    StateCallbackFn StateCallback;

    RealsenseController ();
    ~RealsenseController ();

    bool Start ();
    void Stop ( bool fullStop = false);
    bool ProcessFrame ();

    rs_intrinsics GetColorIntrinsics () { return _color_intrinsics; }
    int GetColorWidth () { return _color_width; }
    int GetColorHeight () { return _color_height; }
    void FillColorBitmap (unsigned char* pImage);

    void SetVolume (volume_bounds volume) { _volume = volume; }
    volume_bounds GetVolume () { return _volume; }

    rs_intrinsics GetDepthIntrinsics () { return _depth_intrinsics; }
    rs_extrinsics GetExtrinsics () { return _extrinsics; }
    int GetDepthWidth () { return _depth_width; }
    int GetDepthHeight () { return _depth_height; }
    bool FillDepthBitmap (unsigned char* pImage, bool colorize);

  protected:
    void ThreadRun ();
    void InvokeState (RSState state);
    DeviceType GetDeviceType (const rs2::device& dev );

  private:
    int _depth_width;
    int _depth_height;
    int _color_width;
    int _color_height;
    int _target_fps;
    bool _is_running;
    bool _is_thread_running;
    float _depth_scale;
    bool _controls_set;
    volume_bounds _volume;

    std::thread* _thread;
    std::mutex* _mutex;

    rs2::frame_queue* _color_frame_queue;
    rs2::frame_queue* _depth_frame_queue;

    rs2::frame* _color_frame;
    rs2::frame* _depth_frame;
    rs2::colorizer* _colorizer;
    rs2::pointcloud* _pc;

    DeviceType _deviceType;

    rs_intrinsics _depth_intrinsics;
    rs_intrinsics _color_intrinsics;
    rs_extrinsics _extrinsics;

  };
}



