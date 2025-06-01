#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QPushButton>
#include "sdlplayer.h"
#include "sinkplayer.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    enum PlayType : quint16
    {
        Unknown,
        Play_SDL,
        Play_SINK
    };
    QPushButton* btn_sdl;
    QPushButton* btn_sink;
    QPushButton* btn_pause;

    PlayType m_play_type = Unknown;
    char m_audio_path[128];
    bool is_stop = true;
    bool is_pause = false;

    SdlPlayer *sdl_player = nullptr;
    SinkPlayer* sink_player = nullptr;
    QFile file;// 音频文件
    QAudioSink *audio = nullptr;  // 音频槽

    void playAudio(PlayType playtype); // 开始播放音频
    void stopAudio(); // 停止播放音频
    void pauseAudio(); // 暂停播放音频/恢复播放音频
    void playPcmFile(); // 播放PCM文件
    void handleStateChanged(QAudio::State newState);

};
#endif // MAINWINDOW_H
