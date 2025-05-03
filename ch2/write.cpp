#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char **argv)
{
    const char *filename = "output.mp4";
    if (argc > 1)
    {
        filename = argv[1];
    }

    // 输出文件的封装器实例
    AVFormatContext *out_fmt_ctx = nullptr;
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed alloc output context.\n");
        return -1;
    }

    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed open output file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open output file %s.\n", filename);

    const AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "Failed find AV_CODEC_ID_H264.\n");
        return -1;
    }

    AVCodecContext *video_codec_ctx = nullptr;
    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (!video_codec_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "alloc video_codec_ctx failed.\n");
        return -1;
    }
    // 设置视频的宽高
    video_codec_ctx->width = 320;
    video_codec_ctx->height = 240;

    // 创建数据流编码器
    AVStream *video_stream = avformat_new_stream(out_fmt_ctx, video_codec);

    // 将编码器实例中的参数复制给流
    avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx);
    video_stream->codecpar->codec_tag = 0;
    // 写入文件头
    ret = avformat_write_header(out_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "write file_header failed.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file_header.\n");
    // 写入文件尾
    av_write_trailer(out_fmt_ctx);
    // 关闭输出流
    avio_close(out_fmt_ctx->pb);
    // 释放实例
    avformat_free_context(out_fmt_ctx);
    return 0;
}
