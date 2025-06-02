#ifndef VIDEOCALLBACK_H
#define VIDEOCALLBACK_H

#include "VideoFrame.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class VideoCallBack
{
public:
    VideoCallBack();

    ~VideoCallBack();

    // 播放函数，此函数不宜做耗时操作，否则会影响播放的流畅性
    virtual void onDisplayVideo(VideoFramePtr videoframe) = 0;
    // 停止播放，通知桌面修改按钮
    virtual void onStopPlay() = 0;
};

#endif // VIDEOCALLBACK_H
