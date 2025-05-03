#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavutil/opt.h>
}
// 输入滤镜实例
AVFilterContext *buffersrc_ctx = nullptr;
// 输出滤镜实例
AVFilterContext *buffersink_ctx = nullptr;
// 滤镜图
AVFilterGraph *filter_graph = nullptr;

/**
 * @brief 初始化滤镜
 * @param video_stream 视频流
 * @param video_decode_ctx 解码器实例
 * @param filter_desc 滤镜字符串
 * @return 0表示success
 */
int init_filter(AVStream *video_stream, AVCodecContext *video_decode_ctx, const char *filter_desc)
{

    int ret = 0;
    // 获取输入滤镜
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    // 获取输出滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    // 分配滤镜的输入输出参数
    AVFilterInOut *inputs = avfilter_inout_alloc();
    // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc();
    // timebase 和 pix_fmt
    AVRational time_base = video_stream->time_base;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    // 分配滤镜图
    filter_graph = avfilter_graph_alloc();
    if (!inputs || !outputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    // 存放信息的字符串
    char args[512];
    // 创建输入滤镜实例，添加到滤镜图
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
             time_base.num, time_base.den,
             video_decode_ctx->sample_aspect_ratio.num, video_decode_ctx->sample_aspect_ratio.den);
    av_log(nullptr, AV_LOG_INFO, "args : %s.\n", args);
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source.\n");
        return ret;
    }
    // 创建输出滤镜实例，添加到滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink");
        return ret;
    }
    // 设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output pixel format.\n");
    }
    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;
    // 把过滤器字符串描述的图形添加到滤镜图中
    ret = avfilter_graph_parse_ptr(filter_graph, filter_desc, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "cannot prase graph string.\n");
        return ret;
    }
    // 检查过滤器字符串的有效性，并配置滤镜图中所有前后链接和图像格式
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph.\n");
        return ret;
    }
    // 释放滤镜的输入输出参数
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    av_log(nullptr, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
}

int main(int argc, char **argv)
{
    const char *filename = "/root/av/fuzhou.mp4";
    if (argc > 1)
    {
        filename = argv[1];
    }
    AVFormatContext *fmt_ctx = NULL;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", filename);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success find stream information.\n");
    // 找到视频流索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    av_log(NULL, AV_LOG_INFO, "video_index=%d\n", video_index);

    if (video_index >= 0)
    {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        av_log(NULL, AV_LOG_INFO, "video_stream codec_id=%d\n", video_codec_id);
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_INFO, "video_codec not found\n");
            return -1;
        }
        av_log(NULL, AV_LOG_INFO, "video_codec name=%s\n", video_codec->name);
        av_log(NULL, AV_LOG_INFO, "video_codec long_name=%s\n", video_codec->long_name);
        // 下面的type字段来自AVMediaType定义，为0表示AVMEDIA_TYPE_VIDEO，为1表示AVMEDIA_TYPE_AUDIO
        av_log(NULL, AV_LOG_INFO, "video_codec type=%d\n", video_codec->type);

        AVCodecContext *video_decode_ctx = NULL;                // 视频解码器的实例
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx)
        {
            av_log(NULL, AV_LOG_INFO, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        av_log(NULL, AV_LOG_INFO, "Success copy video parameters_to_context.\n");
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        av_log(NULL, AV_LOG_INFO, "Success open video codec.\n");
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
        // 初始化滤镜
        init_filter(video_stream, video_decode_ctx, "fps=25");
        avcodec_close(video_decode_ctx);         // 关闭解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放解码器的实例
        avfilter_free(buffersrc_ctx);            // 释放输入滤镜的实例
        avfilter_free(buffersink_ctx);           // 释放输出滤镜的实例
        avfilter_graph_free(&filter_graph);      // 释放滤镜图资源
    }
    else
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    avformat_close_input(&fmt_ctx);
    return 0;
}