#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <inttypes.h>
}

int main(int argc, char **argv)
{
    const char *filename = "/root/av/fuzhou.mp4";
    AVFormatContext *fmt_ctx = nullptr;

    // 打开文件
    int ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", filename);

    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success find stream info.\n");

    av_dump_format(fmt_ctx, 0, filename, 0);

    av_log(nullptr, AV_LOG_INFO, "\n播放时间(微秒)duration=%" PRId64 ".\n", fmt_ctx->duration);
    av_log(nullptr, AV_LOG_INFO, "播放时间(秒)duration=%.2f.\n", (double)fmt_ctx->duration / AV_TIME_BASE);
    av_log(nullptr, AV_LOG_INFO, "比特率bit_rate=%" PRId64 " bit/s = %.4f Kbit/s = %.4f Mbit/s = %.4f KB/s = %.4f MB/s.\n",
           fmt_ctx->bit_rate,
           double(fmt_ctx->bit_rate) / 1000,
           double(fmt_ctx->bit_rate) / (1000 * 1000),
           double(fmt_ctx->bit_rate) / (8 * 1000),
           double(fmt_ctx->bit_rate) / (8 * 1000 * 1000));

    av_log(nullptr, AV_LOG_INFO, "数据流个数nb_streams=%d.\n", fmt_ctx->nb_streams);
    av_log(nullptr, AV_LOG_INFO, "能拥有的最大流数量max_streams=%d.\n", fmt_ctx->max_streams);

    // 视频流
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, -1);
    av_log(nullptr, AV_LOG_INFO, "\nVideo stream index = %d.\n", video_index);
    if (video_index >= 0)
    {
        const AVStream *video_stream = fmt_ctx->streams[video_index];
        av_log(nullptr, AV_LOG_INFO, "video_stream index = %d.\n", video_stream->index);
        av_log(nullptr, AV_LOG_INFO, "video_stream start_time = %" PRId64 ".\n", video_stream->start_time);
        av_log(nullptr, AV_LOG_INFO, "video_stream nb_frames = %" PRId64 ".\n", video_stream->nb_frames);
        av_log(nullptr, AV_LOG_INFO, "video_stream duration = %" PRId64 ".\n", video_stream->duration);
    }

    // 音频流
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, -1);
    av_log(nullptr, AV_LOG_INFO, "\nAudio stream index = %d.\n", audio_index);
    if (audio_index >= 0)
    {
        const AVStream *audio_stream = fmt_ctx->streams[audio_index];
        av_log(nullptr, AV_LOG_INFO, "audio_stream index = %d.\n", audio_stream->index);
        av_log(nullptr, AV_LOG_INFO, "audio_stream start_time = %" PRId64 ".\n", audio_stream->start_time);
        av_log(nullptr, AV_LOG_INFO, "audio_stream nb_frames = %" PRId64 ".\n", audio_stream->nb_frames);
        av_log(nullptr, AV_LOG_INFO, "audio_stream duration = %" PRId64 ".\n", audio_stream->duration);
    }
    avformat_close_input(&fmt_ctx);
    return 0;
}