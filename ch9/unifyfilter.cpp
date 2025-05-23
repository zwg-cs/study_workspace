#include <iostream>
#include <vector>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <string.h>
}

AVFormatContext *in_fmt_ctx = nullptr;      // 输入文件的封装器实例
AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
int video_index = -1;                       // 视频流的索引
int audio_index = -1;                       // 音频流的索引
AVStream *src_video = nullptr;              // 源文件的视频流
AVStream *src_audio = nullptr;              // 源文件的音频流
AVStream *dest_video = nullptr;             // 目标文件的视频流
AVStream *dest_audio = nullptr;             // 目标文件的音频流
AVFormatContext *out_fmt_ctx;               // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = nullptr; // 视频编码器的实例
AVCodecContext *audio_encode_ctx = nullptr; // 音频编码器的实例

AVFilterContext *buffersrc_ctx = nullptr;   // 视频输入滤镜的实例
AVFilterContext *buffersink_ctx = nullptr;  // 视频输出滤镜的实例
AVFilterGraph *filter_graph = nullptr;      // 视频滤镜图
AVFilterContext *abuffersrc_ctx = nullptr;  // 音频输入滤镜的实例
AVFilterContext *abuffersink_ctx = nullptr; // 音频输出滤镜的实例
AVFilterGraph *afilter_graph = nullptr;     // 音频滤镜图

std::vector<std::vector<float>> audio_sample_buffer; // [channel][sample]
bool buffer_initialized = false;
const int encoder_frame_size = 1024;

// 提取指定滤镜的描述字符串
const char *get_filter_desc(const char *filters_desc, const char *filter_prefix)
{
    char *ptr = nullptr;
    int len = strlen(filters_desc);
    char temp[len + 1];
    sprintf(temp, "%s", filters_desc);
    char *value = strtok(temp, ";");
    while (value)
    {
        if (strstr(value, filter_prefix) != nullptr)
        {
            size_t len = strlen(value) + 1;
            ptr = (char *)av_realloc(nullptr, len);
            if (ptr)
            {
                memcpy(ptr, value, len);
            }
        }
        value = strtok(nullptr, ";");
    }
    av_log(nullptr, AV_LOG_INFO, "find filter desc : %s\n", ptr + strlen(filter_prefix));
    return ptr + strlen(filter_prefix);
}

// 打开输入文件
int open_input_file(const char *src_filename)
{
    // 打开输入文件
    int ret = avformat_open_input(&in_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 获取视频流
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0)
    {
        // 获取视频流
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        // 分配解码器实例
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_decode_ctx is nullptr\n");
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        // 打开解码器
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
        return -1;
    }

    // 获取音频流
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        // 获取音频流
        src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找解码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        // 分配解码器实例
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_decode_ctx is nullptr\n");
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        // 打开解码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
        return -1;
    }
    return 0;
}

/**
 * 初始化视频滤镜
 */
int init_video_filter(const char *video_filters_desc)
{
    av_log(nullptr, AV_LOG_INFO, "video filters_desc = %s\n", video_filters_desc);
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");            // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");       // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                        // 输出滤镜连接点
    AVFilterInOut *outputs = avfilter_inout_alloc();                       // 输入滤镜连接点
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE}; // 采样格式数组
    filter_graph = avfilter_graph_alloc();                                 // 分配滤镜图

    if (!outputs || !inputs || !filter_graph)
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
    // 创建输入滤镜,并添加到滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
        return ret;
    }
    // 创建输出滤镜并添加到滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // 给输出滤镜设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    // 将输出节点连接到输入滤镜后
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // 将输入节点连接到输入滤镜后
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 将字符串描述的滤镜添加到滤镜图中
    ret = avfilter_graph_parse_ptr(filter_graph, video_filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查滤镜字符串的有效性，并配置前后连接
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs);  // 释放输入节点
    avfilter_inout_free(&outputs); // 释放输出节点
    av_log(nullptr, AV_LOG_INFO, "Success initialize video filter.\n");
    return ret;
}

/**
 * 初始化音频滤镜
 */
int init_audio_filter(const char *audio_filters_desc)
{
    av_log(nullptr, AV_LOG_INFO, "audio filters_desc = %s\n", audio_filters_desc);
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("abuffer");      // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                   // 输出滤镜连接点
    AVFilterInOut *outputs = avfilter_inout_alloc();                  // 输入滤镜连接点
    afilter_graph = avfilter_graph_alloc();                           // 分配滤镜图

    if (!outputs || !inputs || !afilter_graph)
    {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    char ch_layout[128];
    av_channel_layout_describe(&audio_decode_ctx->ch_layout, ch_layout, sizeof(ch_layout));
    int nb_channels = audio_decode_ctx->ch_layout.nb_channels;
    // 临时字符串，存放输入源的媒体参数信息，比如音频的采样率、采样格式等
    char args[512];
    snprintf(args, sizeof(args), "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
             audio_decode_ctx->sample_rate, av_get_sample_fmt_name(audio_decode_ctx->sample_fmt), ch_layout, nb_channels,
             audio_decode_ctx->time_base.num, audio_decode_ctx->time_base.den);
    av_log(nullptr, AV_LOG_INFO, "args : %s\n", args);

    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&abuffersrc_ctx, buffersrc, "in",
                                       args, nullptr, afilter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&abuffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, afilter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }

    // atempo滤镜要求提前设置sample_fmts，否则av_buffersink_get_format得到的格式不对，会报错Specified sample format flt is invalid or not supported
    enum AVSampleFormat sample_fmts[] = {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE};
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置采样格式
    ret = av_opt_set_int_list(abuffersink_ctx, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample format\n");
        return ret;
    }

    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = abuffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = abuffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;
    // 把采用过滤字符串描述的图形添加到滤镜图
    ret = avfilter_graph_parse_ptr(afilter_graph, audio_filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(afilter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs);  // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    av_log(nullptr, AV_LOG_INFO, "Success initialize audio filter.\n");
    return ret;
}

/**
 * 打开输出文件
 */
int open_output_file(const char *dst_filename)
{
    // 分配输出文件实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dst_filename);
        return -1;
    }
    // 打开输入流
    ret = avio_open(&out_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open output_file %s.\n", dst_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open output_file %s.\n", dst_filename);

    // 创建视频流
    if (video_index >= 0)
    {
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_encoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_encode_ctx is nullptr\n");
            return -1;
        }
        // 设置参数
        video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx); // 帧率
        video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx);  // 时间基
        // video_encode_ctx->gop_size = 12;                                                          // 关键帧间隔
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx);                            // 视频宽
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx);                           // 视频高
        video_encode_ctx->pix_fmt = (enum AVPixelFormat)av_buffersink_get_format(buffersink_ctx); // 视频的像素格式（颜色空间）
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // 打开视频编码器
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        // 创建数据流
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr);
        // 把编码器实例参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
    }

    // 创建音频流
    if (audio_index >= 0)
    {
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频编码器
        AVCodec *audio_codec = (AVCodec *)avcodec_find_encoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_encode_ctx = avcodec_alloc_context3(audio_codec); // 分配编码器的实例
        if (!audio_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_encode_ctx is nullptr\n");
            return -1;
        }
        audio_encode_ctx->time_base = av_buffersink_get_time_base(abuffersink_ctx);                    // 时间基
        audio_encode_ctx->sample_fmt = (enum AVSampleFormat)av_buffersink_get_format(abuffersink_ctx); // 采样格式
        audio_encode_ctx->sample_rate = av_buffersink_get_sample_rate(abuffersink_ctx);                // 采样率
        av_buffersink_get_ch_layout(abuffersink_ctx, &audio_encode_ctx->ch_layout);                    // 声道布局
        // 打开编码器实例
        ret = avcodec_open2(audio_encode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
            return -1;
        }
        // 创建音频流
        dest_audio = avformat_new_stream(out_fmt_ctx, nullptr);
        // 把编码器实例的参数复制给目标音频流
        avcodec_parameters_from_context(dest_audio->codecpar, audio_encode_ctx);
        dest_audio->codecpar->codec_tag = 0;
    }
    // 写入文件头
    ret = avformat_write_header(out_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

int output_video(AVFrame *filter_frame)
{
    // 把数据帧送入编码器
    int ret = avcodec_send_frame(video_encode_ctx, filter_frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (1)
    {
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
        av_write_frame(out_fmt_ctx, packet);
        av_packet_unref(packet); // 清除数据包
    }
    av_packet_free(&packet);
    return ret;
}

/**
 *
 */
int decode_video(AVPacket *packet, AVFrame *frame, AVFrame *filter_frame)
{
    // 未解压的视频数据包发送视频解码器
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        // 从解码器获取解码后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 把原始的数据帧送入输入滤镜中
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return ret;
        }
        while (1)
        {
            // 从滤镜图中取出处理后的数据帧
            ret = av_buffersink_get_frame(buffersink_ctx, filter_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            // 将处理后的视频帧编码
            output_video(filter_frame);
        }
    }
    return ret;
}

int output_audio(AVFrame *filter_frame)
{
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(audio_encode_ctx, filter_frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (1)
    {
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
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
        av_packet_rescale_ts(packet, audio_encode_ctx->time_base, dest_audio->time_base);
        packet->stream_index = 1;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_packet_free(&packet);
    return ret;
}

/**
 * 解码音频流
 */
int decode_audio(AVPacket *packet, AVFrame *frame, AVFrame *filter_frame)
{
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(audio_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(audio_decode_ctx, frame);
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
        ret = av_buffersrc_add_frame_flags(abuffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return ret;
        }
        while (1)
        {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(abuffersink_ctx, filter_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            int nb_channels = filter_frame->ch_layout.nb_channels; // 通道数
            int nb_samples = filter_frame->nb_samples;             // 样本数

            // 初始化缓冲（只执行一次）
            if (!buffer_initialized)
            {
                audio_sample_buffer.resize(nb_channels);
                buffer_initialized = true;
            }

            // 拷贝滤镜输出帧中的样本到缓冲区
            for (int ch = 0; ch < nb_channels; ++ch)
            {
                float *src = (float *)filter_frame->data[ch];
                audio_sample_buffer[ch].insert(audio_sample_buffer[ch].end(), src, src + nb_samples);
            }

            // 只有有足够样本就打包编码
            while ((int)audio_sample_buffer[0].size() >= encoder_frame_size)
            {
                AVFrame *enc_frame = av_frame_alloc();
                enc_frame->nb_samples = encoder_frame_size;
                enc_frame->format = audio_encode_ctx->sample_fmt;
                enc_frame->ch_layout = audio_encode_ctx->ch_layout;
                enc_frame->sample_rate = audio_encode_ctx->sample_rate;

                av_frame_get_buffer(enc_frame, 0);

                // 填充编码帧的数据
                for (int ch = 0; ch < nb_channels; ++ch)
                {
                    float *dst = (float *)enc_frame->data[ch];
                    std::copy(audio_sample_buffer[ch].begin(), audio_sample_buffer[ch].begin() + encoder_frame_size, dst);

                    // 移除已经使用样本
                    audio_sample_buffer[ch].erase(audio_sample_buffer[ch].begin(), audio_sample_buffer[ch].begin() + encoder_frame_size);
                }
                output_audio(enc_frame);
                av_frame_free(&enc_frame);
            }

            // output_audio(filter_frame); // 给音频帧编码，并写入压缩后的音频包
        }
    }
    return ret;
}

// 主函数
int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *dst_filename = "output_unify.mp4";
    const char *filters_desc = "";

    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        filters_desc = argv[2];
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "please enter command such as:\n  ./unifyfilter src_name filters_desc\n");
        return -1;
    }

    // 获取视频过滤串
    const char *video_filters_desc = get_filter_desc(filters_desc, "[0:v]");
    // 获取音频过滤串
    const char *audio_filters_desc = get_filter_desc(filters_desc, "[0:a]");
    // 打开输入文件
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }
    // 初始化视频滤镜
    init_video_filter(video_filters_desc);
    // 初始化音频滤镜
    init_audio_filter(audio_filters_desc);
    // 打开输出文件
    if (open_output_file(dst_filename) < 0)
    {
        return -1;
    }

    int ret = -1;
    AVPacket *packet = av_packet_alloc();          // 数据包
    AVFrame *frame = av_frame_alloc();             // 数据帧
    AVFrame *filter_frame = av_frame_alloc();      // 过滤帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        if (packet->stream_index == video_index)
        {
            packet->stream_index = 0;
            decode_video(packet, frame, filter_frame);
        }
        else if (packet->stream_index == audio_index)
        {
            packet->stream_index = 1;
            decode_audio(packet, frame, filter_frame);
        }
        av_packet_unref(packet); // 清空数据包
    }
    packet->data = nullptr;
    packet->size = 0;
    decode_video(packet, frame, filter_frame); // 传入一个空包，冲走解码缓存
    output_video(nullptr);                     // // 传入一个空帧，冲走编码缓存
    packet->data = nullptr;
    packet->size = 0;
    decode_audio(packet, frame, filter_frame);
    output_video(nullptr);

    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(nullptr, AV_LOG_INFO, "Success process video file.\n");

    // 释放资源
    avcodec_close(video_decode_ctx);         // 关闭视频解码器
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器
    avcodec_close(audio_decode_ctx);         // 关闭音频解码器
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器
    avformat_free_context(in_fmt_ctx);       // 关闭输入文件
    avfilter_free(buffersrc_ctx);            // 释放视频输入滤镜
    avfilter_free(buffersink_ctx);           // 释放视频输出滤镜
    avfilter_graph_free(&filter_graph);      // 释放视频滤镜图
    avfilter_free(abuffersrc_ctx);           // 释放音频输入滤镜
    avfilter_free(abuffersink_ctx);          // 释放音频输出滤镜
    avfilter_graph_free(&afilter_graph);     // 释放音频滤镜图
    avcodec_close(video_encode_ctx);         // 关闭视频编码器
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器
    avcodec_close(audio_encode_ctx);         // 关闭音频解码器
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器
    avio_close(out_fmt_ctx->pb);             // 关闭输出流
    avformat_free_context(out_fmt_ctx);      // 释放输出实例
    av_packet_free(&packet);                 // 释放数据包
    av_frame_free(&frame);                   // 释放数据帧
    av_frame_free(&filter_frame);            // 释放过滤帧

    return 0;
}