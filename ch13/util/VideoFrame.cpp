#include "VideoFrame.h"

// 构造函数
VideoFrame::VideoFrame()
{
    mYuv420Buffer = nullptr;
    mRgbBuffer = nullptr;
}

// 析构函数
VideoFrame::~VideoFrame()
{
    if(!mYuv420Buffer)
    {
        free(mYuv420Buffer);
        mYuv420Buffer = nullptr;
    }

    if(!mRgbBuffer)
    {
        free(mRgbBuffer);
        mRgbBuffer = nullptr;
    }
}

// 初始化图像缓存
void VideoFrame::initBuffer(const int width, const int height)
{
    if(mYuv420Buffer != nullptr)
    {
        free(mYuv420Buffer);
        mYuv420Buffer = nullptr;
    }
    if(mRgbBuffer != nullptr)
    {
        free(mRgbBuffer);
        mRgbBuffer = nullptr;
    }

    mWidth = width;
    mHeight = height;

    mYuv420Buffer = (uint8_t*)malloc(width * height * 3 / 2);
    mRgbBuffer = (uint8_t*)malloc(width * height * 3);
}


// 设置YUV缓存数据
void VideoFrame::setYUVbuf(const uint8_t* buf)
{
    int Ysize = mWidth * mHeight;
    memcpy(mYuv420Buffer, buf, Ysize * 3 / 2);
}

// 设置RGB缓存数据
void VideoFrame::setRGBbuf(const uint8_t* buf)
{
    memcpy(mRgbBuffer, buf, mWidth * mHeight * 3);
}
