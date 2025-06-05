#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory>


#define VideoFramePtr std::shared_ptr<VideoFrame>

class VideoFrame
{
public:
    // 构造函数
    VideoFrame();
    // 析构函数
    ~VideoFrame();

    // 初始化图像缓存
    void initBuffer(const int width, const int hegith);
    // 设置YUV图像缓存
    void setYUVBuffer(const uint8_t *buf);
    // 设置RGB图像缓存
    void setRGBBuffer(const uint8_t *buf);

    int width()
    {
        return mWidth;
    }

    int height()
    {
        return mHeight;
    }

    uint8_t* yuvBuffer()
    {
        return mYuv420Buffer;
    }

    uint8_t* rgbBuffer()
    {
        return mRgbBuffer;
    }

protected:
    uint8_t* mYuv420Buffer = nullptr;  // YUV缓存数据的地址
    uint8_t* mRgbBuffer = nullptr;  // RGB缓存数据的地址
    int mWidth;  // 图像的宽
    int mHeight;  // 图像的高

};

#endif // VIDEOFRAME_H
