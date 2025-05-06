/**
 * 打印fps等基本信息
 */
#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <inttypes.h>
}

int main(int argc, char **argv)
{
    const char *filename = "/root/av/fuzhou.mp4";
    if (argc > 1)
    {
        filename = argv[1];
    }

    AVFormatContext *fmt_ctx = nullptr;

    // 打开输入文件
    int ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed open file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", filename);

    // 查找流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }

    // 打印fmt_ctx中的一些信息
    av_log(nullptr, AV_LOG_INFO, "\nduration=%" PRId64 ".\n", fmt_ctx->duration);
    av_log(nullptr, AV_LOG_INFO, "nb_streams=%d.\n", fmt_ctx->nb_streams);
    av_log(nullptr, AV_LOG_INFO, "max_streams=%d.\n", fmt_ctx->max_streams);
    av_log(nullptr, AV_LOG_INFO, "video_codec_id = %d\n", fmt_ctx->video_codec_id);
    av_log(nullptr, AV_LOG_INFO, "audio_codec_id = %d\n", fmt_ctx->audio_codec_id);

    // 查找视频流索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "\nvideo_index = %d.\n", video_index);
    if (video_index >= 0)
    {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频编码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "video_codec_name = %s.\n", video_codec->name);
        AVCodecParameters *video_codecpar = video_stream->codecpar;
        // 计算帧率
        int fps = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;
        av_log(nullptr, AV_LOG_INFO, "video_codecpar bit_rate = %" PRId64 ".\n", video_codecpar->bit_rate);
        av_log(nullptr, AV_LOG_INFO, "video_codecpar width = %d.\n", video_codecpar->width);
        av_log(nullptr, AV_LOG_INFO, "video_codecpar height = %d.\n", video_codecpar->height);
        av_log(nullptr, AV_LOG_INFO, "fps = %d.\n", fps);
        /* 计算每个视频帧的持续时间 */
        float per_video = 1000.0 / fps;
        av_log(nullptr, AV_LOG_INFO, "one video frame duration is %.2fms.\n", per_video);
        /* 获取视频的时间基准 */
        av_log(nullptr, AV_LOG_INFO, "video_stream time_base.num = %d.\n", video_stream->time_base.num);
        av_log(nullptr, AV_LOG_INFO, "video_stream time_base.den = %d.\n", video_stream->time_base.den);
        /* 计算视频帧的时间戳增量 */
        int avdio_frame_timestamp_increment = 1 * video_stream->time_base.den / fps;
        av_log(nullptr, AV_LOG_INFO, "video frame timestamp increment = %d.\n", avdio_frame_timestamp_increment);
    }

    /* 查看频道流索引 */
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "\naudio_index = %d.\n", audio_index);
    if (audio_index >= 0)
    {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        /* 查找音频解码器 */
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "audio_codec name = %s.\n", audio_codec->name);
        AVCodecParameters *audio_codecpar = audio_stream->codecpar;
        av_log(nullptr, AV_LOG_INFO, "audio_codecpar bit_rate = %" PRId64 ".\n", audio_codecpar->bit_rate);
        av_log(nullptr, AV_LOG_INFO, "audio_codecpar frame_size = %d.\n", audio_codecpar->frame_size);
        av_log(nullptr, AV_LOG_INFO, "audio_codecpar sample_rate = %d.\n", audio_codecpar->sample_rate);
        av_log(nullptr, AV_LOG_INFO, "audio_codecpar nb_channals = %d.\n", audio_codecpar->ch_layout.nb_channels);
        /* 计算音频帧的持续时间，frame_size 是每个音频帧的采样数量 sample_rate 是每个音频帧的采样率 */
        float per_audio = 1000.0 * audio_codecpar->frame_size / audio_codecpar->sample_rate;
        av_log(nullptr, AV_LOG_INFO, "one audio frame's duration is %.2fms.\n", per_audio);
        /* 音频帧的时间基准 */
        av_log(nullptr, AV_LOG_INFO, "audio_stream time_base.num = %d.\n", audio_stream->time_base.num);
        av_log(nullptr, AV_LOG_INFO, "audio_stream time_base.den = %d.\n", audio_stream->time_base.den);
        /* 音频帧时间戳的增量 */
        int audio_frame_timestamp_increment = 1 * audio_codecpar->frame_size * (audio_stream->time_base.den / audio_codecpar->sample_rate);
        av_log(nullptr, AV_LOG_INFO, "audio frame timestamp increment = %d.\n", audio_frame_timestamp_increment);
    }
    /* 关闭文件 */
    avformat_close_input(&fmt_ctx);
    return 0;
}
