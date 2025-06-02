#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H
#include <QImage>
#include <QWidget>

#include "util/VideoFrame.h"
#include "util/FunctionTransfer.h"
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil//avutil.h>
#include <libswscale/swscale.h>
}
class VideoView : public QWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent = nullptr);
    ~VideoView();

public slots:
    // 视频帧已被解码，准备展示到界面
    void onFrameDecoded(VideoFramePtr videeFrame);

private:
    QImage* _image = nullptr; // 图像控件
    QRect _rect;  // 矩形框
    int count = 0;
    VideoFramePtr mVideoFrame; // 视频帧的指针
    void paintEvent(QPaintEvent* event) override;
    void freeImage();  // 释放图像资源


signals:
};

#endif // VIDEOVIEW_H
