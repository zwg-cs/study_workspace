#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>

#include "util/functiontransfer.h"
#include "util/videocallback.h"
#include "util/videoframe.h"
#include "openglview.h"
#include "videoplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public VideoCallBack
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QPushButton* btn_play;  // 播放按钮
    QPushButton* btn_pause;  // 暂停按钮
    OpenglView* opengl_view;  // 播放视图

    char m_video_path[128]= {NULL};  // 媒体文件地址
    bool is_stop = true;
    bool is_pause = false;

    VideoPlayer *video_player = nullptr;

    void playVideo();
    void pauseVideo();

protected:
    // 显示视频画面，此函数不宜做耗时操作，否则会影响播放的流畅性。
    void onDisplayVideo(VideoFramePtr videoFrame);
    // 停止播放。通知界面修改按钮状态
    void onStopVideo();

};
#endif // MAINWINDOW_H
