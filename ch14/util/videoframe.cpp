#include "videoframe.h"

// 构造函数，两个buffer指针为空
VideoFrame::VideoFrame()
{
    mYuv420Buffer = nullptr;
    mRgbBuffer = nullptr;
}

// 析构函数，释放buffer指向的空间
VideoFrame::~VideoFrame()
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
}


// 初始化图像缓存, 先让buffer指向为空，在分配内存
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

    // 设置帧的宽和高 并给对应buffer分配空间
    mWidth = width;
    mHeight = height;
    mYuv420Buffer = (uint8_t*)malloc(mWidth * mHeight * 3 / 2);  // 使用malloc分配mWidth * mHeight * 3 / 2个字节的空间
    mRgbBuffer = (uint8_t*)malloc(mWidth * mHeight * 3);
}


// 设置YUV缓存数据,将buf指向的数据复制到mYuv420Buffer
void VideoFrame::setYUVBuffer(const uint8_t* buf)
{
    int Ysize = mWidth * mHeight;
    memcpy(mYuv420Buffer, buf, Ysize * 3 / 2);
}

// 设置RGB缓存数据
void VideoFrame::setRGBBuffer(const uint8_t* buf)
{
    memcpy(mRgbBuffer, buf, mWidth * mHeight * 3);
}
