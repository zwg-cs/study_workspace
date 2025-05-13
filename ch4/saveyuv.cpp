#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

AVCodecContext *video_decode_ctx;
int packet_index = -1; // 数据包的索引序号

/**
 * 把视频帧保存为YUV图像。save_index表示要把第几个视频帧保存为图片
 */
int save_yuv_file(AVFrame *frame, int save_index)
{
    av_log(nullptr, AV_LOG_INFO, "format = %d, width = %d, height = %d\n", frame->format, frame->width, frame->height);
    char yuv_name[20] = {0};
    sprintf(yuv_name, "output_%03d.yuv", save_index);
    av_log(nullptr, AV_LOG_INFO, "target image file is %s.\n", yuv_name);
    FILE *fp = fopen(yuv_name, "wb"); // 以写打开文件
    if (!fp)
    {
        av_log(nullptr, AV_LOG_ERROR, "open file %s fail.\n", yuv_name);
        return -1;
    }

    // 把yuv数据依次写入文件
    int i = -1;
    while (++i < frame->height) // 写入Y分量
    {
        fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
    }
    i = -1;
    while (++i < frame->height / 2) // 写入U分量
    {
        fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
    }
    i = -1;
    while (++i < frame->height / 2) // 写入V分量
    {
        fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
    }
    fclose(fp); // 关闭文件
    return 0;
}

/**
 *
 * 解码数据包
 */
int decode_video(AVPacket *packet, AVFrame *frame, int save_index)
{
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet to decode occur error.\n");
        return -1;
    }
    while (1)
    {
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "receive frame occur error.\n");
            break;
        }
        ++packet_index;
        if (packet_index < save_index) // 还没找到对应序号的帧
        {
            return AVERROR(EAGAIN);
        }
        save_yuv_file(frame, save_index); // 把视频帧保存为YUV图像
        break;
    }

    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhous.mp4";
    int save_index = 0;
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        save_index = atoi(argv[2]);
    }

    // 打开输入文件
    AVFormatContext *src_fmt_ctx = nullptr;
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s %d.\n", src_filename, ret);
        return -1;
    }
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_filename);
        return -1;
    }
    // 找到视频流的索引
    int src_video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (src_video_stream_index >= 0)
    {
        AVStream *video_stream = src_fmt_ctx->streams[src_video_stream_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video codec is nullptr.\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_decode_ctx is nullptr.\n");
            return -1;
        }
        // 复制参数到解码器
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video decode ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video stream.\n");
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(src_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        if (packet->stream_index == src_video_stream_index) // 对视频流解码
        {
            ret = decode_video(packet, frame, save_index);
            if (ret == 0)
            {
                break; // 只保存一帧
            }
        }
        av_packet_unref(packet);
    }
    av_log(NULL, AV_LOG_INFO, "Success save %d_index frame as yuv file.\n", save_index);
    // 释放资源
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(video_decode_ctx);
    avcodec_free_context(&video_decode_ctx);
    avformat_close_input(&src_fmt_ctx);
    return 0;
}