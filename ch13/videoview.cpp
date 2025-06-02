#include "videoview.h"
#include <QPainter>

VideoView::VideoView(QWidget *parent)
    : QWidget{parent}
{
    // 设置背景色
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background:balck");
}


/**
 * @brief VideoView::~VideoView 析构方法
 *
 */
VideoView::~VideoView()
{
    freeImage();
}

// 视频帧已经被解码，准备展示到界面上
void VideoView::onFrameDecoded(VideoFramePtr videoFrame)
{
    FunctionTransfer::runInMainThread([=](){
        mVideoFrame.reset();
        mVideoFrame = videoFrame;
        VideoFrame* frame = mVideoFrame.get();
        printf("onFrameDecoded currentThreadId=%d, count=%d\n", QThread::currentThreadId(), count);
        // 释放之前的图片
        freeImage();
        if(videoFrame != nullptr && frame->rgbBuffer() != nullptr)
        {
            _image = new QImage((uchar *)frame->rgbBuffer(),frame->width(), frame->height(), QImage::Format_RGB888);

            // 计算组件的最终尺寸
            int w = width();
            int h = height();

            // 计算矩形框的四周
            int dx = 0;
            int dy = 0;
            int dw = frame->width();
            int dh = frame->height();

            // 计算目标尺寸
            if(dw > w || dh > h)  // 如果图像尺寸超过组件的大小，就缩小图像
            {
                if(dw * h > w * dh)  // 视频的宽高比 > 播放器的宽高比
                {
                    dh = w * dh / dw;
                    dw = w;
                }else
                {
                    dw = h * dw / dw;
                    dh = h;
                }
            }

            // 居中显示
            dx = (w - dw) >> 1;
            dy = (h - dh) >> 1;

            _rect = QRect(dx, dy, dw, dh);
        }
        // 触发paintEvent方法
        update();
        count++;
    });
}



// 将图片绘制到当前组件
void VideoView::paintEvent(QPaintEvent* envet)
{
    if(!_image)
    {
        return;
    }
    // 将图片绘制到当前组件
    QPainter(this).drawImage(_rect, *_image);
}


// 释放图像资源
void VideoView::freeImage()
{
    if(_image)
    {
        delete _image;
        _image = nullptr;
    }
}
