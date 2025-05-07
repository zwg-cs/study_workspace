#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *dst_filename = "onlyvideo.mp4";

    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        dst_filename = argv[2];
    }

    AVFormatContext *src_fmt_ctx = nullptr; // 输入实例
    /* 打开文件查找流 */
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open src file %s.\n", src_filename);

    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }
    /* 获取视频流 */
    AVStream *src_video_stream = nullptr;
    int src_video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (src_video_stream_index >= 0)
    {
        src_video_stream = src_fmt_ctx->streams[src_video_stream_index];
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find src video stream.\n");
        return -1;
    }

    /* 输出文件的一些准备操作 */
    AVFormatContext *dst_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc dst file %s.\n", dst_filename);
        return -1;
    }
    /* 打开输出流 */
    ret = avio_open(&dst_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open dst file %s.\n", dst_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open dst file %s.\n", dst_filename);

    /* 复制输入视频流参数到输出视频流参数 */
    if (src_video_stream_index >= 0)
    {
        AVStream *dst_video_stream = avformat_new_stream(dst_fmt_ctx, nullptr); // 目标文件创建流
        avcodec_parameters_copy(dst_video_stream->codecpar, src_video_stream->codecpar);
        dst_video_stream->time_base = src_video_stream->time_base;
        dst_video_stream->codecpar->codec_tag = 0;
    }

    /* 写入文件头 */
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");

    AVPacket *pkt = av_packet_alloc();
    /* 轮询写入文件 */
    while (av_read_frame(src_fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == src_video_stream_index)
        {
            pkt->stream_index = 0; // 视频流默认第一路
            ret = av_write_frame(dst_fmt_ctx, pkt);
            if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(pkt); // 清楚数据包
    }
    ret = av_write_trailer(dst_fmt_ctx);
    av_log(NULL, AV_LOG_INFO, "Success peel audio.\n");

    av_packet_free(&pkt);               // 释放packet资源
    avio_close(dst_fmt_ctx->pb);        // 关闭文件
    avformat_free_context(dst_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&src_fmt_ctx); // 关闭音视频文件

    return 0;
}