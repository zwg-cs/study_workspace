#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
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
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", filename);

    // 查找流
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success find stream info.\n");

    // 找到视频索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, -1);
    if (video_index >= 0)
    {
        // 获取视频流
        AVStream *video_stream = fmt_ctx->streams[video_index];
        // 获取视频解码器id
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);

        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find video codec.\n");
            return -1;
        }

        av_log(nullptr, AV_LOG_INFO, "video_codec id = %d.\n", video_codec->id);
        av_log(nullptr, AV_LOG_INFO, "video_codec name = %s.\n", video_codec->name);
        av_log(nullptr, AV_LOG_INFO, "video_codec long name = %s.\n", video_codec->long_name);
        av_log(nullptr, AV_LOG_INFO, "video_codec type = %d.\n", video_codec->type);
    }

    // 查找音频流
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, -1);
    if (audio_index >= 0)
    {
        // 获取音频流
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        // 获取音频解码器id
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 音频解码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);

        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find audio codec.\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "audio_codec id = %d.\n", audio_codec->id);
        av_log(nullptr, AV_LOG_INFO, "audio_codec name = %s.\n", audio_codec->name);
        av_log(nullptr, AV_LOG_INFO, "audio_codec long name = %s.\n", audio_codec->long_name);
        av_log(nullptr, AV_LOG_INFO, "audio_codec type = %d.\n", audio_codec->type);
    }

    avformat_close_input(&fmt_ctx);
    return 0;
}