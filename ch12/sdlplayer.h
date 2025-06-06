#ifndef SDLPLAYER_H
#define SDLPLAYER_H


#include <SDL.h>
#undef main
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

class SdlPlayer
{
public:
    SdlPlayer(const char* audio_path);
    ~SdlPlayer();

    void start();  // 开始播放
    void stop();   // 停止播放
    void pause();  // 暂停播放
    void resume(); // 恢复播放


private:
    const char* m_audio_path; // 音频文件的路径
    bool is_stop = false;     // 是否停止播放
    bool is_pause = false;    // 是否暂停播放

    int playAudio(); // 播放音频
    void static fill_audio(void* para, uint8_t* stream, int len);// 回调函数，在获取音频数据后调用
};

#endif // SDLPLAYER_H
