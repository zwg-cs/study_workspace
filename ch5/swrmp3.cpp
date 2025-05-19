#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

AVFormatContext *src_fmt_ctx = nullptr;     // 输入实例
AVCodecContext *audio_decode_ctx = nullptr; // 解码器实例
AVCodecContext *audio_encode_ctx = nullptr; // 编码器实例
int audio_index = -1;                       // 音频流索引

int open_input_file(const char *src_filename)
{
    // 打开输入文件
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Open src file occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Not find stream info %s %d.\n", src_filename, ret);
        return -1;
    }

    // 获取音频流索引
    audio_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        // 获取音频流
        AVStream *audio_stream = src_fmt_ctx->streams[audio_index];
        // 获取编码器类型
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 找到编码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec is nullptr.\n");
            return -1;
        }
        // 分配解码器实例
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_decode_ctx is nullptr.\n");
            return -1;
        }
        // 从音频流中复制参数到解码器实例
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        // 打开解码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "open audio_encode_ctx occur error %d.\n", ret);
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Not find audio steram in file %s.\n", src_filename);
    }
    return 0;
}

/**
 * 初始化编码器
 */
int init_audio_encoder(int nb_channels)
{
    // 查找编码器
    const AVCodec *audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!audio_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio encoder codec is nullptr.\n");
        return -1;
    }
    const enum AVSampleFormat *p = audio_codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) // 使用AV_SAMPLE_FMT_NONE作为结束符
    {
        av_log(NULL, AV_LOG_INFO, "audio_codec support format %d\n", *p);
        p++;
    }

    // 分配解码器
    audio_encode_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio_encode_ctx is nullptr.\n");
        return -1;
    }

    // 设置参数
    if (nb_channels == 2) // 双声道
    {
        audio_encode_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;                              // 采样格式
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_STEREO); //
    }
    else // 单声道
    {
        audio_encode_ctx->sample_fmt = AV_SAMPLE_FMT_S16P; // 采样格式
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_MONO);
    }
    audio_encode_ctx->bit_rate = 64000;    // 比特率
    audio_encode_ctx->sample_rate = 44100; // 采样率

    // 打开编码器
    int ret = avcodec_open2(audio_encode_ctx, audio_codec, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
        return -1;
    }
    return 0;
}

// 将音频帧保存到MP3文件
int save_mp3_file(FILE *fp_out, AVFrame *swr_frame)
{
    int ret = avcodec_send_frame(audio_encode_ctx, swr_frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "encode swr_frame occur error %d.\n", ret);
        return -1;
    }
    AVPacket *packet = av_packet_alloc();
    while (ret == 0)
    {
        // 获取编码后的数据包
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encoder packet occur error %d.\n", ret);
            break;
        }
        // 把编码后的mp3数据包写入文件
        fwrite(packet->data, 1, packet->size, fp_out);
    }
    av_packet_free(&packet);
    return 0;
}

/**
 * 解码视频帧
 */
int decode_audio(AVPacket *packet, AVFrame *frame, FILE *fp_out, SwrContext *swr_ctx, AVFrame *swr_frame)
{
    int ret = avcodec_send_packet(audio_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet to decoder occur error %d.\n", ret);
        return -1;
    }
    while (1)
    {
        ret = avcodec_receive_frame(audio_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur errpr %d.\n", ret);
            break;
        }
        // 重采样, 将输入的音频数据根据指定的采样规格转换为新的音频数据输出
        ret = swr_convert(swr_ctx,                                         // 音频采样器实例
                          swr_frame->data, swr_frame->nb_samples,          // 输出数据内容和采样数据大小
                          (const uint8_t **)frame->data, frame->nb_samples // 输入数据内容和采样数据大小
        );

        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "swr_convert frame occur error %d.\n", ret);
            return -1;
        }
        // 把音频帧保持到MP3文件
        save_mp3_file(fp_out, swr_frame);
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";

    if (argc < 1)
    {
        src_filename = argv[1];
    }

    const char *mp3_filename = "swrmp3.mp3";
    // 打开输入文件
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }

    // 初始化MP3编码器
    if (init_audio_encoder(audio_decode_ctx->ch_layout.nb_channels) < 0)
    {
        return -1;
    }

    av_log(nullptr, AV_LOG_INFO, "audio_decode_ctx frame_size = %d, sample_fmt = %d, sample_rate = %d, nb_channels = %d.\n",
           audio_decode_ctx->frame_size,
           audio_decode_ctx->sample_fmt,
           audio_decode_ctx->sample_rate,
           audio_decode_ctx->ch_layout.nb_channels);

    av_log(nullptr, AV_LOG_INFO, "audio_encode_ctx frame_size = %d, sample_fmt = %d, sample_rate = %d, nb_channels = %d.\n",
           audio_encode_ctx->frame_size,
           audio_encode_ctx->sample_fmt,
           audio_encode_ctx->sample_rate,
           audio_encode_ctx->ch_layout.nb_channels);

    // 以读写打开输出文件
    FILE *fp_out = fopen(mp3_filename, "wb");
    if (!fp_out)
    {
        av_log(NULL, AV_LOG_ERROR, "open mp3 file %s fail.\n", mp3_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "target audio file is %s.\n", mp3_filename);

    int ret = -1;
    // 初始化音频采样器实例
    SwrContext *swr_ctx = nullptr; // 音频采样器实例
    // 分配音频采样器实例
    ret = swr_alloc_set_opts2(&swr_ctx,                      // 音频采样器实例
                              &audio_encode_ctx->ch_layout,  // 输出的声道布局
                              audio_encode_ctx->sample_fmt,  // 输出采样格式
                              audio_encode_ctx->sample_rate, // 输出采样频率
                              &audio_decode_ctx->ch_layout,  // 输入声道布局
                              audio_decode_ctx->sample_fmt,  // 输入采样格式
                              audio_decode_ctx->sample_rate, // 输入采样频率
                              0, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "swr init error %d.\n", ret);
        return -1;
    }
    // 初始化音频采样器实例
    ret = swr_init(swr_ctx);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "swr_init occur error %d.\n", ret);
        return -1;
    }
    // 分配一个数据帧
    AVFrame *swr_frame = av_frame_alloc();
    // 每帧的采样数量，这里要和原来的音频保持一致。aac = 1024, mp3 = 1152
    // swr_frame->nb_samples = audio_decode_ctx->frame_size;
    swr_frame->nb_samples = audio_encode_ctx->frame_size;

    if (swr_frame->nb_samples <= 0)
    {
        swr_frame->nb_samples = 512;
    }
    swr_frame->format = audio_encode_ctx->sample_fmt;   // 数据帧格式
    swr_frame->ch_layout = audio_encode_ctx->ch_layout; // 通道布局
    ret = av_frame_get_buffer(swr_frame, 0);            // 为数据帧分配新的缓冲区
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "get frame buffer error %d.\n", ret);
        return -1;
    } // 结束初始化音频采样器的实例

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == audio_index)
        {
            // 对视频帧解码
            decode_audio(packet, frame, fp_out, swr_ctx, swr_frame);
        }
        av_packet_unref(packet);
    }

    // 冲走采样的缓冲
    while (1)
    {
        ret = swr_convert(swr_ctx, swr_frame->data, swr_frame->nb_samples, nullptr, 0);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "swr_convert frame occur error %d.\n", ret);
            return -1;
        }
        else if (ret == 0)
        {
            break;
        }
        save_mp3_file(fp_out, swr_frame);
    }
    // 传入一个空帧，冲走编码器缓存
    save_mp3_file(fp_out, nullptr);
    av_log(nullptr, AV_LOG_INFO, "Success resample audio frame as mp3 file.\n");
    fclose(fp_out);

    // 释放资源
    avcodec_close(audio_decode_ctx);         // 关闭解码器
    avcodec_free_context(&audio_decode_ctx); // 释放解码器
    avformat_close_input(&src_fmt_ctx);      // 关闭输入文件
    avcodec_close(audio_encode_ctx);         // 关闭解码器
    avcodec_free_context(&audio_encode_ctx); // 释放编码器
    swr_free(&swr_ctx);                      // 释放音频采样器实例
    av_packet_free(&packet);                 // 释放数据包
    av_frame_free(&frame);                   // 释放数据帧
    av_frame_free(&swr_frame);               // 释放数据帧

    return 0;
}