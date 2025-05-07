#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    const char *filename = "/root/av/fuzhou.mp4";
    const char *out_filename = "output.mp4";
    if (argc > 1)
    {
        filename = argv[1];
    }
    if (argc > 2)
    {
        out_filename = argv[2];
    }

    /* 打开输入文件 */
    AVFormatContext *in_fmt_ctx = nullptr;
    int ret = avformat_open_input(&in_fmt_ctx, filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed open input file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input file %s.\n", filename);
    /* 查找流信息 */
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }

    /* 获取视频流和音频流索引 */
    int src_video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "input file video stream index is %d.\n", src_video_index);
    AVStream *src_video_stream = nullptr;
    if (src_video_index >= 0)
    {
        src_video_stream = in_fmt_ctx->streams[src_video_index];
    }

    int src_audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "input file audio stream index is %d.\n", src_audio_index);
    AVStream *src_audio_stream = nullptr;
    if (src_audio_index >= 0)
    {
        src_audio_stream = in_fmt_ctx->streams[src_audio_index];
    }

    /* 输出文件实例 */
    AVFormatContext *out_fmt_ctx = nullptr;
    /* 分配输出文件实例 */
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, out_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed alloc output ctx.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success alloc output file.\n");
    /* 打开输入流 */
    ret = avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed open output file %s.\n", out_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open output file %s.\n", out_filename);

    /*如果输入有视频流，给输出文件添加视频流*/
    if (src_video_index >= 0)
    {
        AVStream *dst_video_stream = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        /*把文件的视频参数原样复制过来*/
        avcodec_parameters_copy(dst_video_stream->codecpar, src_video_stream->codecpar);
        dst_video_stream->time_base = src_video_stream->time_base;
        dst_video_stream->codecpar->codec_tag = 0;
    }

    /* 如果输入有音频流，给输出文件添加音频流 */
    if (src_audio_index >= 0)
    {
        AVStream *dst_audio_stream = avformat_new_stream(out_fmt_ctx, nullptr);
        /*把文件的音频参数原样复制过来*/
        avcodec_parameters_copy(dst_audio_stream->codecpar, src_audio_stream->codecpar);
        dst_audio_stream->codecpar->codec_tag = 0;
    }

    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写入文件头
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed write file header.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");

    /* 写入输出文件 */
    AVPacket *pkt = av_packet_alloc(); // 分配一个数据包
    /* 轮询数据集 */
    while (av_read_frame(in_fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == src_video_index)
        {
            pkt->stream_index = 0;
            ret = av_write_frame(out_fmt_ctx, pkt);
        }
        else
        {
            pkt->stream_index = 1;
            ret = av_write_frame(out_fmt_ctx, pkt);
        }
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(pkt); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写入文件尾
    av_log(nullptr, AV_LOG_INFO, "Success copy file.\n");

    av_packet_free(&pkt);               // 释放资源包
    avio_close(out_fmt_ctx->pb);        // 关闭输出流
    avformat_close_input(&in_fmt_ctx);  // 关闭输入文件
    avformat_free_context(out_fmt_ctx); // 关闭封装器实例

    return 0;
}