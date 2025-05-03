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
    av_log(nullptr, AV_LOG_INFO, "video_codec_id=%d.\n", fmt_ctx->video_codec_id);
    av_log(nullptr, AV_LOG_INFO, "audio_codec_id=%d.\n", fmt_ctx->audio_codec_id);

    // 查找视频流
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "\nvideo index = %d.\n", video_index);
    if (video_index >= 0)
    {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        av_log(nullptr, AV_LOG_INFO, "video stream codec id = %d.\n", video_codec_id);
        // 查找视频解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video codec not find.\n");
            return -1;
        }
        // 输出video codec 的一些信息
        av_log(nullptr, AV_LOG_INFO, "video_codec id = %d.\n", video_codec->id);
        av_log(nullptr, AV_LOG_INFO, "video_codec name = %s.\n", video_codec->name);
        av_log(nullptr, AV_LOG_INFO, "video_codec long name = %s.\n", video_codec->long_name);
        // 0表示video 1表示audio
        av_log(nullptr, AV_LOG_INFO, "video_codec type = %d.\n", video_codec->type);

        // 视频解码器实例
        AVCodecContext *video_codec_ctx = nullptr;
        // 分配解码器实例
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (!video_codec_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "alloc video codev ctx failed.\n");
            return -1;
        }
        // 把视频流的参数复制个解码器实例
        ret = avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "copy param from stream to codec ctx failed.\n");
            return -1;
        }

        av_log(nullptr, AV_LOG_INFO, "Success copy video param to ctx.\n");
        av_log(nullptr, AV_LOG_INFO, "video_decoder_ctx width=%d.\n", video_codec_ctx->width);
        av_log(nullptr, AV_LOG_INFO, "video_decoder_ctx height=%d.\n", video_codec_ctx->height);

        // 打开解码器实例
        ret = avcodec_open2(video_codec_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "failed open video decoder ctx.\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "Success open video decoder ctx.\n");
        av_log(nullptr, AV_LOG_INFO, "video decoder ctx profile=%d\n", video_codec_ctx->profile);
        // 关闭实例
        avcodec_close(video_codec_ctx);
        //
        avcodec_free_context(&video_codec_ctx);
    }

    // 获取音频流
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "\naudio index=%d\n", audio_index);
    if (audio_index >= 0)
    {
        // 音频流
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        av_log(nullptr, AV_LOG_INFO, "audio codec id = %d.\n", audio_codec_id);
        // 音频解码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find audio decoder.\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "audio_codec name = %s.\n", audio_codec->name);
        av_log(nullptr, AV_LOG_INFO, "audio_codec long name = %s.\n", audio_codec->long_name);
        // type=0 video, type=1 audio
        av_log(nullptr, AV_LOG_INFO, "audio_codec type = %d.\n", audio_codec->type);

        // 音频编码器实例
        AVCodecContext *audio_decoder_ctx = nullptr;
        // 分配编码器实例
        audio_decoder_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decoder_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "failed alloc audio deocoder ctx.\n");
            return -1;
        }
        // 将流中的参数复制到编码器
        ret = avcodec_parameters_to_context(audio_decoder_ctx, audio_stream->codecpar);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Failed copy param to audio decoder ctx\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "Success copy audio param to ctx.\n");
        av_log(nullptr, AV_LOG_INFO, "audio_decoder_ctx profile=%d.\n", audio_decoder_ctx->profile);
        av_log(nullptr, AV_LOG_INFO, "audio_decoder_ctx nb_streams=%d.\n", audio_decoder_ctx->ch_layout.nb_channels);

        // 打开编码器
        ret = avcodec_open2(audio_decoder_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Failed open audio decoder ctx.\n");
            return -1;
        }
        av_log(nullptr, AV_LOG_INFO, "Success open audio decoder ctx.\n");

        // 关闭音频解码器
        avcodec_close(audio_decoder_ctx);
        // 释放音频解码器实例
        avcodec_free_context(&audio_decoder_ctx);
    }

end:

    if (fmt_ctx)
    {
        avformat_close_input(&fmt_ctx);
    }
    return -1;
}