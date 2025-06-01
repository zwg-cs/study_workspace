#ifndef SINKPLAYER_H
#define SINKPLAYER_H
#include <QAudioSink>
#include <QMediaDevices>
#include <qaudiodecoder>


extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}
#include <QObject>

class SinkPlayer : public QObject
{
    Q_OBJECT
public:
    explicit SinkPlayer(QObject *parent = nullptr);
    ~SinkPlayer();

    void setFileName(const char* file_path);

    void start();  // 开始播放视频
    void stop();  // 停止播放视频
    void pause();  // 暂停播放视频
    void resume();  // 恢复播放

private:
    const char* m_audio_path;  // 音频播放的路径
    bool is_stop = false;  // 是否停止播放
    bool is_pause = false; // 是否暂停播放

    QAudioSink* sink = nullptr; // 音频槽
    QIODevice *io;  // 输出音频设备

    int playAudio(); // 播放音频

signals:
};

#endif // SINKPLAYER_H
