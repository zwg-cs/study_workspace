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
    VideoFrame();
    ~VideoFrame();

    // 初始化图像缓存
    void initBuffer(const int width, const int height);

    // 设置YUV缓存数据
    void setYUVbuf(const uint8_t* buf);

    // 设置RGB缓存数据
    void setRGBbuf(const uint8_t* bug);

    // 返回画面的宽度
    int width()
    {
        return mWidth;
    }

    // 返回画面的高度
    int height()
    {
        return mHeight;
    }

    // 返回yuv缓存数据地址
    uint8_t* yuvBuffer()
    {
    return mYuv420Buffer;
    }

    // 返回rgb缓存数据地址
    uint8_t* rgbBuffer()
    {
        return mRgbBuffer;
    }


protected:
    uint8_t *mYuv420Buffer = nullptr; // YUV缓存数据地址
    uint8_t* mRgbBuffer = nullptr; // RGB缓存数据的地址
    int mWidth; // 画面宽度
    int mHeight; // 画面高度
};







#endif
