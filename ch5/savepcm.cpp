#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *dst_filename = "savepcm.pcm";

    // 输入文件封装实例
    AVFormatContext *src_fmt_ctx = nullptr;

    // 打开音视频
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 音频解码器实例
    AVCodecContext *audio_decode_ctx = nullptr;
    int audio_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        AVStream *audio_stream = src_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 查找音频解码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 复制参数到解码器
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        av_log(nullptr, AV_LOG_INFO, "sample_fmt = %d, nb_samples = %d, nb_channels = %d.\n",
               audio_decode_ctx->sample_fmt, audio_decode_ctx->frame_size, audio_decode_ctx->ch_layout.nb_channels);
        av_log(nullptr, AV_LOG_INFO, "format_name = %s, is_planae = %d, data_size = %d.\n",
               av_get_sample_fmt_name(audio_decode_ctx->sample_fmt),
               av_sample_fmt_is_planar(audio_decode_ctx->sample_fmt),
               av_get_bytes_per_sample(audio_decode_ctx->sample_fmt)

        );
        // 打开解码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }

    // 以写的方式打开文件
    FILE *fp_out = fopen(dst_filename, "wb");
    if (!fp_out)
    {
        av_log(NULL, AV_LOG_ERROR, "open file %s fail.\n", dst_filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", dst_filename);

    int i = 0;
    int j = 0;
    int data_size = 0;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    // 轮询数据包
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        // 处理音频流
        if (packet->stream_index == audio_index)
        {
            // 数据包送入解码器
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0)
            {
                // 从解码器获取解码后的数据帧
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                // 将数据帧保存到PCM文件
                if (av_sample_fmt_is_planar((enum AVSampleFormat)frame->format))
                {
                    // 平面模式的音频在存储时要改为交错模式
                    data_size = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
                    i = 0;
                    while (i < frame->nb_samples)
                    {
                        j = 0;
                        while (j < frame->ch_layout.nb_channels)
                        {
                            fwrite(frame->data[j] + data_size * i, 1, data_size, fp_out);
                            j++;
                        }
                        i++;
                    }
                }
                else
                {
                    // 非平面模式，直接写入文件
                    fwrite(frame->extended_data[0], 1, frame->linesize[0], fp_out);
                }
                av_frame_unref(frame); // 清楚帧数据
            }
            else
            {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet);
    }
    av_log(NULL, AV_LOG_INFO, "Success save audio frame as pcm file.\n");

    // 关闭文件释放资源
    fclose(fp_out); // 关闭文件
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_close(audio_decode_ctx);
    avcodec_free_context(&audio_decode_ctx);
    avformat_free_context(src_fmt_ctx);
    return 0;
}