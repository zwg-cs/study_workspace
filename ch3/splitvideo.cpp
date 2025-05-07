#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <inttypes.h>
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *dst_filenaem = "split_video.mp4";

    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        dst_filenaem = argv[2];
    }

    /* 源文件实例 */
    AVFormatContext *src_fmt_ctx = nullptr;
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
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info %s.\n", src_filename);
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
        av_log(nullptr, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }

    /* 目标文件的实例 */
    AVFormatContext *dst_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_filenaem);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc dst file %s.\n", dst_filenaem);
        return -1;
    }

    /* 打开目标文件流 */
    ret = avio_open(&dst_fmt_ctx->pb, dst_filenaem, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open dst file %s.\n", dst_filenaem);
        return -1;
    }

    av_log(nullptr, AV_LOG_INFO, "Success open dst file %s.\n", dst_filenaem);

    /* 复制src流参数到dst流参数 */
    AVStream *dst_video_stream = nullptr;
    if (src_video_stream_index >= 0)
    {
        dst_video_stream = avformat_new_stream(dst_fmt_ctx, nullptr); // 创建数据流
        /* 复制参数 */
        avcodec_parameters_copy(dst_video_stream->codecpar, src_video_stream->codecpar);
        dst_video_stream->time_base = src_video_stream->time_base;
        dst_video_stream->codecpar->codec_tag = 0;
    }

    // 写入文件头
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Write file header failed!.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");

    /* 切割开始时间和切割结束时间，秒 */
    double begin_time = 5.0;
    double end_time = 15.0;
    /* 计算开始切割位置的时间戳和结束切割位置的时间戳 */
    int64_t begin_time_pts = begin_time / av_q2d(src_video_stream->time_base);
    int64_t end_time_pts = end_time / av_q2d(src_video_stream->time_base);
    av_log(nullptr, AV_LOG_INFO, "begin_video_pts = %" PRId64 ", end_video_pts = %" PRId64 ".\n", begin_time_pts, end_time_pts);

    /* 寻找指定时间戳的关键帧，并非并非begin_video_pts所处的精确位置，而是离begin_video_pts最近的关键帧 */
    ret = av_seek_frame(src_fmt_ctx, src_video_stream_index, begin_time_pts, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "seek video frame occur error %d.\n", ret);
        return -1;
    }

    int64_t key_frame_pts = -1; // 关键帧的播放时间戳
    AVPacket *pkt = av_packet_alloc();
    /* 循环 */
    while (av_read_frame(src_fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == src_video_stream_index)
        {
            pkt->stream_index = 0;
            // 保存最靠经begin_video_pts的关键帧的时间戳
            if (key_frame_pts == -1)
            {
                key_frame_pts = pkt->pts;
            }
            // 比切割的结束时间大，结束切割
            if (pkt->pts > key_frame_pts + end_time_pts - begin_time_pts)
            {
                break;
            }
            /* 调整视频包的播放时间和解码时间 */
            pkt->pts = pkt->pts - key_frame_pts;
            pkt->dts = pkt->dts - key_frame_pts;
            /* 写入 */
            ret = av_write_frame(dst_fmt_ctx, pkt);
            if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(pkt);
    }

    // 写入文件尾
    av_write_trailer(dst_fmt_ctx);
    av_log(nullptr, AV_LOG_INFO, "Success split video.\n");

    av_packet_free(&pkt);
    avio_close(dst_fmt_ctx->pb);
    avformat_free_context(dst_fmt_ctx);
    avformat_close_input(&src_fmt_ctx);
    return 0;
}