#include "sinkplayer.h"
#include <QDebug>
#include <thread>

#define MAX_AUDIO_FRAME_SIZE 8096  // 一帧音频最大长度
int out_sample_rate = 44100;  // 输出的采样率
AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;  // 输出的采样格式
enum QAudioFormat::SampleFormat q_sample_fmt = QAudioFormat::Int16;

/**
 * @brief 构造函数
 * @param parent
 */
SinkPlayer::SinkPlayer(QObject *parent)
    : QObject{parent}
{
    QAudioFormat format;
    format.setSampleRate(out_sample_rate);
    format.setChannelCount(out_ch_layout.nb_channels);
    format.setSampleFormat(q_sample_fmt);

    qInfo("sampleRate: %d, channelCount: %d, sampleFormat: %d", format.sampleRate(), format.channelCount(), format.sampleFormat());

    QAudioDevice device(QMediaDevices::defaultAudioOutput());
    if(!device.isFormatSupported(format))
    {
        qWarning() << "Raw audio format not supported by backend, cannot play audio->";
    }
    else
    {
        qInfo() << "Raw audio format is supported.";
    }
    sink = new QAudioSink(device, format);
}

/**
 * @brief SinkPlayer::~SinkPlayer
 */
SinkPlayer::~SinkPlayer()
{
    sink->stop();  // 停止播放
}

void SinkPlayer::setFileName(const char *file_path)
{
    qInfo() << "SinkPlayer::setFileName" << file_path << '\n';
    m_audio_path = file_path;
}


// 开始播放
void SinkPlayer::start()
{
    is_stop = false;
    qInfo() << "Play audio: " << m_audio_path << '\n';
    io = sink->start();  // 开始播放
    // 开启分线程播放音频，detach表示分离该线程
    std::thread([this](){
        int ret = playAudio(); // 播放音频
        qInfo() << "play result: " << ret << '\n';
    }).detach();
}

/**
 * @brief SinkPlayer::stop
 */
void SinkPlayer::stop()
{
    is_stop = true;
    sink->stop();
}

// 暂停播放
void SinkPlayer::pause()
{
    is_pause = true;
    sink->suspend(); // 暂停播放
}

// 恢复播放
void SinkPlayer::resume()
{
    is_pause = false;
    sink->resume();
}

// 播放音频
int SinkPlayer::playAudio()
{
    qInfo() << "playAudio " << m_audio_path << '\n';
    AVFormatContext* in_fmt_ctx = nullptr;
    // 打开输入文件
    int ret = avformat_open_input(&in_fmt_ctx, m_audio_path, nullptr, nullptr);
    if (ret < 0) {
        qCritical() << "Can't open file " << m_audio_path << '\n';
        return -1;
    }
    qInfo() << "Success open input_file " << m_audio_path << '\n';
    // 查找流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }

    AVCodecContext* audio_decode_ctx = nullptr;
    AVStream* src_audio = nullptr;

    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if(audio_index>=0)
    {
        src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 找到解码器
        const AVCodec* audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            qCritical() << "audio_codec not found" << '\n';
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx) {
            qCritical() << "audio_decode_ctx is null" << '\n';
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        // 打开编码器
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

    qInfo() << "begin swr_init" << '\n';
    int out_nb_samples = audio_decode_ctx->frame_size; // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels;  // output number channels
    SwrContext* swr_ctx = nullptr;
    ret = swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, out_sample_fmt, out_sample_rate,
                              &audio_decode_ctx->ch_layout, audio_decode_ctx->sample_fmt, audio_decode_ctx->sample_rate,
                                0, nullptr
                              );
    if (ret < 0) {
        qCritical() << "swr_alloc_set_opts2 error " << ret << '\n';
        return -1;
    }
    ret = swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        qCritical() << "swr_init error " << ret << '\n';
        return -1;
    }

    // 计算输出的缓冲区大小
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    qInfo() << "out_buffer_size=" << out_buffer_size << '\n';
    // 分配输出缓冲区的空间
    unsigned char *out_buff = (unsigned char *) av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);
    qInfo() << "begin play audio" << '\n';

    AVPacket *packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while(av_read_frame(in_fmt_ctx, packet) >= 0)
    {
        if(packet->stream_index == audio_index)
        {
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
                // 重采样
                swr_convert(swr_ctx, &out_buff, MAX_AUDIO_FRAME_SIZE,
                            (const uint8_t **)frame->data, frame->nb_samples);
                // 往扬声器写入音频
                io->write((const char*)(char*)out_buff, out_buffer_size);
                qInfo() << "swr_frame->nb_samples=" << frame->nb_samples;
                int delay = 1000 * frame->nb_samples / out_sample_rate;
                qInfo() << "delay=" << delay;
                _sleep(delay); // 休眠若干时间，单位毫秒
            }else
            {
                qCritical() << "send packet occur error " << ret << '\n';
            }
            while(is_pause)
            {
                _sleep(20);
                if(is_stop)
                {
                    break;
                }
            }
            if(is_stop)
            {
                break;
            }

        }
        av_packet_unref(packet);
    }
    qInfo() << "Success play audio file" << '\n';
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    swr_free(&swr_ctx); // 释放音频采样器的实例
    qInfo() << "Quit Play" << '\n';
    return 0;
}
