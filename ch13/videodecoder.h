#ifndef VIDEODECODER_H
#define VIDEODECODER_H
#include "util/VideoFrame.h"
#include "util/videocallback.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
class VideoDecoder
{
public:
    // 构造函数
    VideoDecoder(int play_type, const char* video_path, VideoCallBack* callback);
    // 析构函数
    ~VideoDecoder();

    void start();  // 开始解码
    void stop();   // 停止解码
    void pause();  // 暂停解码
    void resum();  // 恢复解码


private:
    int m_play_type;  // 播放类型，0为QImage播放。1为OpenGL播放
    const char* m_video_path;  // 视频文件的路径
    VideoCallBack* m_callback;  // 视频回调接口
    bool is_stop = false;  // 是否停止解码
    bool is_pause = false;  // 是否暂停解码
    uint8_t *m_rgb_buffer = nullptr; // RGB缓存数据
    uint8_t *m_yuv_buffer = nullptr; // yuv缓存数据

    int playVideo();  // 播放视频
    void displayImage(int width, int height); // 显示图像
};

#endif // VIDEODECODER_H
