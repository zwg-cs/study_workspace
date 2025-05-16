#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

AVCodecContext *audio_decode_ctx = nullptr; // 解码器实例
AVCodecContext *audio_encode_ctx = nullptr; // 编码器实例
AVFormatContext *src_fmt_ctx = nullptr;     // 输入上下文实例
int audio_index = -1;                       // 音频流索引

int open_input_file(const char *src_filename)
{
    // 分配输入实例
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 获取音频流索引
    audio_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        AVStream *audio_stream = src_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    return 0;
}

int init_audio_encoder(void)
{
    // 查找编码器
    const AVCodec *audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!audio_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
        return -1;
    }
    const enum AVSampleFormat *p = audio_codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE)
    {
        av_log(nullptr, AV_LOG_INFO, "audio_codec support format %d.\n", *p);
        ++p;
    }
    audio_encode_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio_encode_ctx is null\n");
        return -1;
    }
    // 设置解码器参数
    audio_encode_ctx->sample_fmt = audio_decode_ctx->sample_fmt;   // 采样格式
    audio_encode_ctx->ch_layout = audio_decode_ctx->ch_layout;     // 声道布局
    audio_encode_ctx->bit_rate = audio_decode_ctx->bit_rate;       // 比特率
    audio_encode_ctx->sample_rate = audio_decode_ctx->sample_rate; // 采样率
    audio_encode_ctx->frame_size = 1024;                           // 关键修改：设置MP3帧大小
    // 打开编码器
    int ret = avcodec_open2(audio_encode_ctx, audio_codec, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
        return -1;
    }
    return 0;
}

int save_mp3_file(FILE *fp_out, AVFrame *frame)
{
    // 分配数据包
    AVPacket *packet = av_packet_alloc();
    int ret = avcodec_send_frame(audio_encode_ctx, frame);
    while (ret == 0)
    {
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        fwrite(packet->data, 1, packet->size, fp_out);
    }
    av_packet_free(&packet);
    return 0;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    int save_index = 0;
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    const char *mp3_name = "output_savemp3.mp3";

    // 打开输入文件
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }
    // 初始化MP3编码器
    if (init_audio_encoder() < 0)
    {
        return -1;
    }
    // 打开输出文件啊
    FILE *fp_out = fopen(mp3_name, "wb");
    if (!fp_out)
    {
        av_log(nullptr, AV_LOG_ERROR, "open mp3 file %s fail.\n", mp3_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "target audio file is %s\n", mp3_name);

    int ret = -1;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    // 轮询数据包
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == audio_index)
        {
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0)
            {
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                save_mp3_file(fp_out, frame);
            }
            else
            {
                av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet);
    }
    save_mp3_file(fp_out, nullptr);
    av_log(nullptr, AV_LOG_INFO, "Success save audio frame as mp3 file.\n");
    fclose(fp_out);

    // 释放资源
    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&packet);                 // 释放数据包资源
    avcodec_close(audio_decode_ctx);         // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avcodec_close(audio_encode_ctx);         // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    avformat_close_input(&src_fmt_ctx);      // 关闭音视频文件

    return 0;
}