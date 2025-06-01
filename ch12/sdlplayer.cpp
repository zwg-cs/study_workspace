#include "sdlplayer.h"
#include <QDebug>
#include <thread>

#define MAX_AUDIO_FRAME_SIZE 8096  // 一帧音频的最大长度
int audio_len = 0; // 一帧PCM音频的数据长度
unsigned char* audio_pos = nullptr; // 当前读取的位置

// 构造函数
SdlPlayer::SdlPlayer(const char* audio_path)
{
    m_audio_path = audio_path;
}

// 析构函数
SdlPlayer::~SdlPlayer()
{

}


// 回调函数
void SdlPlayer::fill_audio(void* para, uint8_t* stream, int len)
{
    // 将缓冲区清零
    SDL_memset(stream, 0, len);
    if(audio_len == 0)
    {
        return;
    }
    len = (len > audio_len ? audio_len : len);
    // 将音频数据混合到缓冲区
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

// 开始播放
void SdlPlayer::start()
{
    is_stop = false;
    // 开启分线程播放视频
    std::thread([this](){
        int ret = playAudio(); // 播放音频
        qInfo() << "Play result: " << ret << '\n';
    }).detach();
}

// 暂停播放
void SdlPlayer::stop()
{
    is_stop = true;
}

// 停止播放
void SdlPlayer::pause()
{
    is_pause = true;
}

// 恢复播放
void SdlPlayer::resume()
{
    is_pause = false;
}

// 播放音频
int SdlPlayer::playAudio()
{
    qInfo() << "playaudio " << m_audio_path << '\n';
    // 输入文件实例
    AVFormatContext* in_fmt_ctx = nullptr;
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, m_audio_path, nullptr, nullptr);
    if(ret < 0)
    {
        qCritical() << "Can't open file " << m_audio_path << '\n';
        return -1;
    }
    // 查找流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if(ret < 0)
    {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }

    // 音频解码器
    AVCodecContext* audio_decode_ctx = nullptr;
    // 音频流
    AVStream* src_audio = nullptr;
    // 查找音频流
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if(audio_index >=0)
    {
        src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        const AVCodec* audio_codec = avcodec_find_decoder(audio_codec_id);
        if(!audio_codec)
        {
            qCritical() << "audio_codec not found" << '\n';
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            qCritical() << "audio_decode_ctx is null" << '\n';
            return -1;
        }
        // 将音频流中的参数复制到解码器
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        // 打开解码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0) {
            qCritical() << "Can't open audio_decode_ctx" << '\n';
            return -1;
        }
    }else
    {
        qCritical() << "Can't find audio stream" << '\n';
        return -1;
    }

    qInfo() << "begin ser_init" << '\n';

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; //输出声道布局
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出采样格式
    int out_sample_rate = 44100; // 输出采样率
    int out_nb_sample = audio_decode_ctx->frame_size; // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels; // 输出的通道数量
    SwrContext *swr_ctx = nullptr;  // 音频采样器实例
    ret = swr_alloc_set_opts2(&swr_ctx,
                              &out_ch_layout,out_sample_fmt,out_sample_rate,
                              &audio_decode_ctx->ch_layout,audio_decode_ctx->sample_fmt, audio_decode_ctx->sample_rate,
                              0,nullptr);
    if (ret < 0)
    {
        qCritical() << "swr_alloc_set_opts2 error " << ret << '\n';
        return -1;
    }
    ret = swr_init(swr_ctx); // 初始化
    if(ret < 0)
    {
        qCritical() << "swr_init error" << '\n';
    }

    // 计算缓冲区的大小
    int out_buffer_size = av_samples_get_buffer_size(nullptr, out_channels, out_nb_sample, out_sample_fmt, 1);
    qInfo() << "out_buffer_size = " << out_buffer_size << '\n';
    // 分配缓冲区的空间
    unsigned char* out_buff = (unsigned char*)av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);

    qInfo() << "begin SDL_Init" << '\n';
    // 初始化SDL
    SDL_AudioSpec audio_spec; // 什么SDL音频参数
    audio_spec.freq = out_sample_rate; // 采样频率
    audio_spec.format = AUDIO_S16SYS; // 采样格式
    audio_spec.samples = out_nb_sample; // 采样数量
    audio_spec.channels = out_channels; // 声道数量
    audio_spec.silence = 0; // 是否静音
    audio_spec.callback = fill_audio; // 回调函数
    audio_spec.userdata = nullptr; // 回调函数的额外信息
    if(SDL_OpenAudio(&audio_spec, nullptr) < 0)  // 打开扬声器
    {
        qCritical() << "Open audio occur error" << '\n';
        return -1;
    }
    // 开始播放
    SDL_PauseAudio(0); //参数为0表示播放，为1表示暂停
    qInfo() << "begin paly audio" << '\n';

    AVPacket *packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while(av_read_frame(in_fmt_ctx, packet) <= 0)
    {
        if(packet->stream_index == audio_index)
        {
            // 把为解压的数据包发给解码器实例
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if(ret == 0)
            {
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }else if(ret < 0)
                {
                    qCritical() << "decode frame occur error " << ret << '\n';
                    continue;
                }
                // 对音频从采样
                swr_convert(swr_ctx, &out_buff, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)frame->data, frame->nb_samples);
                while(audio_len > 0) // 如果没有播放完
                {
                    SDL_Delay(1); // 延迟若干时间，单位毫秒
                }
                audio_pos = (unsigned char*)out_buff;// 把音频数据同步到缓冲区位置
                audio_len = out_buffer_size; // 缓冲区大小
            }
            else
            {
                qCritical() << "send packet occur error " << ret << '\n';
            }
            while(is_pause) // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
            {
                SDL_Delay(20);  // 休眠20毫秒
                if(is_stop)  // 暂停期间如果停止播放，就结束暂停
                {
                    break;
                }
            }
            if(is_stop)  // 如果停止播放，就跳出循环结束解码
            {
                break;
            }

        }
        av_packet_unref(packet); // 清除数据包
    }
    qInfo() << "Success play audio file" <<  '\n';
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_close(audio_decode_ctx);
    avcodec_free_context(&audio_decode_ctx);
    avformat_close_input(&in_fmt_ctx);
    swr_free(&swr_ctx);

    SDL_CloseAudio();  // 关闭扬声器
    SDL_Quit(); // 退出SDL
    qInfo() << "Quit SDL" << '\n';
    return 0;

}
