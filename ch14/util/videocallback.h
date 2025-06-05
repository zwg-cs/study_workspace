#ifndef VIDEOCALLBACK_H
#define VIDEOCALLBACK_H
#include "videoframe.h"

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

    // 播放视频，此函数不宜做耗时操作，否则会影响播放的流程性
    virtual void onDisplayVideo(VideoFramePtr videoFrame) = 0;  // 纯虚函数，这是一个接口函数，必须由子类实现

    // 暂停播放。通知界面修改按钮状态
    virtual void onStopVideo() = 0;

    // 虚函数是指在基类中使用 virtual 关键字声明的函数，
    // 目的是允许子类通过**重写（override）**来实现自己的版本，
    // 并且在运行时能够通过基类指针或引用正确调用子类的函数。

    // 抽象类
};

#endif // VIDEOCALLBACK_H
