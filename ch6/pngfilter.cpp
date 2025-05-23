#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

AVFormatContext *in_fmt_ctx = nullptr;      // 输入文件的封装器实例
AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
int video_index = -1;                       // 视频流的索引
AVStream *src_video = nullptr;              // 源文件的视频流
AVStream *dest_video = nullptr;             // 目标文件的视频流
AVFormatContext *out_fmt_ctx;               // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = nullptr; // 视频编码器的实例

AVFilterContext *buffersrc_ctx = nullptr;  // 输入滤镜的实例
AVFilterContext *buffersink_ctx = nullptr; // 输出滤镜的实例
AVFilterGraph *filter_graph = nullptr;     // 滤镜图

// 提取滤镜的名称
char *get_filter_name(const char *filters_desc)
{
    char *ptr = nullptr;
    int len_desc = strlen(filters_desc);
    char temp[len_desc + 1];
    sprintf(temp, "%s", filters_desc);
    char *value = strtok(temp, "=");
    av_log(nullptr, AV_LOG_INFO, "find filter name: %s\n", value);
    if (value)
    {
        size_t len = strlen(value) + 1;
        ptr = (char *)av_realloc(nullptr, len);
        if (ptr)
            memcpy(ptr, value, len);
    }
    return ptr;
}

// 替换字符串中的特定字符串
char *strrpl(char *s, const char *s1, const char *s2)
{
    char *ptr;
    while (ptr = strstr(s, s1))
    { // 如果在s中找到s1
        memmove(ptr + strlen(s2), ptr + strlen(s1), strlen(ptr) - strlen(s1) + 1);
        memcpy(ptr, &s2[0], strlen(s2));
    }
    return s;
}

// 打开输入文件
int open_input_file(const char *src_name, const char *colorspace)
{
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0)
    {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        // 打开解码器实例
        AVDictionary *codec_options = nullptr;
        av_dict_set(&codec_options, "colorspace", colorspace, 0);
        ret = avcodec_open2(video_decode_ctx, video_codec, &codec_options);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    return 0;
}

// 打开输出文件
int open_output_file(const char *dest_name)
{
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dest_name);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dest_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open output_file %s.\n", dest_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open output_file %s.\n", dest_name);
    if (video_index >= 0)
    { // 创建编码器实例和新的视频流
        // 查找图片编码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_PNG);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_encode_ctx is null\n");
            return -1;
        }
        video_encode_ctx->pix_fmt = AV_PIX_FMT_RGB24;                   // png的像素格式
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx);  // 画面宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx); // 画面高度
        video_encode_ctx->time_base = (AVRational){1, 25};              // 时间基
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr);    // 打开编码器的实例
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
        const char *pix_names = av_get_pix_fmt_name(video_encode_ctx->pix_fmt);
        av_log(nullptr, AV_LOG_INFO, "pix_names: %s\n", pix_names);
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

// 初始化滤镜
int init_filter(const char *filters_desc)
{
    av_log(nullptr, AV_LOG_INFO, "filters_desc : %s\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");      // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                  // 输出滤镜的输入连接点
    AVFilterInOut *outputs = avfilter_inout_alloc();                 // 输入滤镜的输入连接点
    enum AVPixelFormat pis_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    // 分配滤镜图
    filter_graph = avfilter_graph_alloc();
    if (!inputs || !outputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    // 临时字符串，存放输入源的媒体参数信息，比如视频的宽高、像素格式等
    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
             src_video->time_base.num, src_video->time_base.den,
             video_decode_ctx->sample_aspect_ratio.num, video_decode_ctx->sample_aspect_ratio.den);
    av_log(nullptr, AV_LOG_INFO, "args : %s\n", args);

    // 创建输入滤镜实例，并添加到现有的滤镜图中
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
        return ret;
    }

    // 创建输出滤镜实例, 并添加到现有的滤镜图中
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }

    // outputs连接到输入滤镜
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // inputs连接到输出滤镜
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 将滤镜字符串中的滤镜添加到滤镜图中
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }

    // 检查滤镜字符串的有效性，连接所有滤镜
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs);  // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    av_log(nullptr, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
}

// 给视频帧编码，并写入压缩后的视频包
int output_video(AVFrame *frame)
{
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        AVPacket *packet = av_packet_alloc(); // 分配一个数据包
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对视频帧重新编码
int recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame)
{
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 把原始的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return -1;
        }
        while (1)
        {
            // 从滤镜接收器获取一个已经加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            output_video(filt_frame); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_name = "/root/av/fuzhou.mp4";
    const char *filters_desc = nullptr;
    if (argc > 1)
    {
        src_name = argv[1];
    }
    if (argc > 2)
    {
        filters_desc = argv[2]; // 过滤字符串从命令行读取
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "please enter command such as:\n  ./pngfilter src_name filters_desc\n");
        return -1;
    }
    const char *colorspace = "unknown";
    if (argc > 3)
    {
        colorspace = argv[3]; // 色度坐标从命令行读取
    }
    av_log(nullptr, AV_LOG_INFO, "colorspace: %s\n", colorspace);
    if (open_input_file(src_name, colorspace) < 0)
    { // 打开输入文件
        return -1;
    }
    const char *dest_name = "output_000.png";
    av_log(nullptr, AV_LOG_INFO, "dest_name: %s\n", dest_name);
    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(dest_name) < 0)
    { // 打开输出文件
        return -1;
    }
    int ret = -1;
    AVPacket *packet = av_packet_alloc();   // 分配一个数据包
    AVFrame *frame = av_frame_alloc();      // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0)
    { // 轮询数据包
        if (packet->stream_index == video_index)
        {                                            // 视频包需要重新编码
            recode_video(packet, frame, filt_frame); // 对视频帧重新编码
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = nullptr; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(nullptr);                   // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx);           // 写文件尾
    av_log(nullptr, AV_LOG_INFO, "Success process png file.\n");

    avfilter_free(buffersrc_ctx);            // 释放输入滤镜的实例
    avfilter_free(buffersink_ctx);           // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph);      // 释放滤镜图资源
    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&packet);                 // 释放数据包资源
    avio_close(out_fmt_ctx->pb);             // 关闭输出流
    avcodec_close(video_decode_ctx);         // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx);         // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx);      // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx);       // 关闭音视频文件
}