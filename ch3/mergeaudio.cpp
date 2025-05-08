#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    const char *src_video_file = "/root/av/fuzhous.mp4";
    const char *src_audio_file = "/root/av/fuzhous.aac";
    const char *dst_file = "output_mergeaudio.mp4";

    if (argc > 1)
    {
        src_video_file = argv[1];
    }
    if (argc > 2)
    {
        src_audio_file = argv[2];
    }
    if (argc > 3)
    {
        dst_file = argv[3];
    }

    /* 1. 打开源视频找到视频流 */
    AVFormatContext *src_video_fmt_ctx = nullptr;
    int ret = avformat_open_input(&src_video_fmt_ctx, src_video_file, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src video file %s.\n", src_video_file);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open src video file %s.\n", src_video_file);
    ret = avformat_find_stream_info(src_video_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_video_file);
    }

    int src_video_stream_index = av_find_best_stream(src_video_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream *src_video_stream = nullptr;
    if (src_video_stream_index >= 0)
    {
        src_video_stream = src_video_fmt_ctx->streams[src_video_stream_index];
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video stream in file %s.\n", src_video_file);
        return -1;
    }

    /* 2. 打开源音频找到音频流*/
    AVFormatContext *src_audio_fmt_ctx = nullptr;
    ret = avformat_open_input(&src_audio_fmt_ctx, src_audio_file, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src audio file %s.\n", src_audio_file);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open src audio file %s.\n", src_audio_file);
    ret = avformat_find_stream_info(src_audio_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_audio_file);
        return -1;
    }
    AVStream *src_audio_stream = nullptr;
    int src_audio_stream_index = av_find_best_stream(src_audio_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (src_audio_stream_index >= 0)
    {
        src_audio_stream = src_audio_fmt_ctx->streams[src_audio_stream_index];
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find audio stream in file %s.\n", src_audio_file);
        return -1;
    }

    /* 3.打开输出文件 */
    AVFormatContext *dst_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_file);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc dst file %s.\n", dst_file);
        return -1;
    }

    ret = avio_open(&dst_fmt_ctx->pb, dst_file, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open dst file %s.\n", dst_file);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open dst file %s.\n", dst_file);

    /* 4.给目标视频创建视频流和音频流 */
    AVStream *dst_video_stream = nullptr;
    if (src_video_stream_index >= 0)
    {
        dst_video_stream = avformat_new_stream(dst_fmt_ctx, nullptr);
        avcodec_parameters_copy(dst_video_stream->codecpar, src_video_stream->codecpar);
        dst_video_stream->codecpar->codec_tag = 0;
    }
    AVStream *dst_audio_stream = nullptr;
    if (src_audio_stream_index >= 0)
    {
        dst_audio_stream = avformat_new_stream(dst_fmt_ctx, nullptr);
        avcodec_parameters_copy(dst_audio_stream->codecpar, src_audio_stream->codecpar);
        dst_audio_stream->codecpar->codec_tag = 0;
    }

    /* 5.写入文件 */
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Write file header occur error.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");

    AVPacket *pkt = av_packet_alloc(); // 数据包
    int64_t last_video_pts = 0;        // 上次的视频时间戳
    int64_t last_audio_pts = 0;        // 上次的音频时间戳

    while (1)
    {
        // 如果是第一个数据包或者视频的时间戳小于等于音频的时间戳，处理视频
        if (last_video_pts == 0 || av_compare_ts(last_video_pts, dst_video_stream->time_base,
                                                 last_audio_pts, dst_audio_stream->time_base) <= 0)
        {
            while ((ret = av_read_frame(src_video_fmt_ctx, pkt)) >= 0) // 轮询视频包 这里使用循环，因为里面原来还有音频包
            {
                if (pkt->stream_index == src_video_stream_index) // 找到第一个视频包
                {
                    break;
                }
            }
            if (ret == 0)
            {
                av_packet_rescale_ts(pkt, src_video_stream->time_base, dst_video_stream->time_base); // 把数据包的时间戳从一个时间基转换到另一个时间基
                pkt->stream_index = 0;                                                               // 视频流索引
                last_video_pts = pkt->pts;                                                           // 保存当前视频时间戳，下一次使用
            }
            else
            {
                av_log(nullptr, AV_LOG_INFO, "End video file.\n");
                break;
            }
        }
        else
        {
            while ((ret = av_read_frame(src_audio_fmt_ctx, pkt)) >= 0) // 轮询音频包
            {
                if (pkt->stream_index == src_audio_stream_index)
                {
                    break;
                }
            }
            if (ret == 0)
            {
                av_packet_rescale_ts(pkt, src_audio_stream->time_base, dst_audio_stream->time_base);
                pkt->stream_index = 1;
                last_audio_pts = pkt->pts;
            }
            else
            {
                av_log(nullptr, AV_LOG_INFO, "End audio file.\n");
                break;
            }
        }

        ret = av_write_frame(dst_fmt_ctx, pkt);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(pkt);
    }
    av_write_trailer(dst_fmt_ctx); // 写入文件尾
    av_log(nullptr, AV_LOG_INFO, "Success merge video and audio file.\n");

    /* 6.释放资源 */
    av_packet_free(&pkt);
    avio_close(dst_fmt_ctx->pb);
    avformat_free_context(dst_fmt_ctx);
    avformat_close_input(&src_video_fmt_ctx);
    avformat_close_input(&src_audio_fmt_ctx);

    return 0;
}