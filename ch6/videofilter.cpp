#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <inttypes.h>
}

AVFormatContext *src_fmt_ctx = nullptr;     // 输入文件实例
AVFormatContext *dst_fmt_ctx = nullptr;     // 输出文件实例
AVCodecContext *video_decode_ctx = nullptr; // 解码器实例
AVCodecContext *video_encode_ctx = nullptr; // 编码器实例
AVStream *src_video = nullptr;              // 输入视频流
AVStream *src_audio = nullptr;              // 输入音频流
AVStream *dst_video = nullptr;              // 输出视频流
int video_index = -1;                       // 视频流索引
int audio_index = -1;                       // 音频流索引
AVFilterContext *buffersrc_ctx = nullptr;   // 输入滤镜实例
AVFilterContext *buffersink_ctx = nullptr;  // 输出滤镜实例
AVFilterGraph *filter_graph = nullptr;      // 滤镜图

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
        {
            memcpy(ptr, value, len);
        }
    }
    return ptr;
}

// 替换字符串中的特定字符串
// 用于将字符串 s 中的所有子串 s1 替换为 s2
char *strrpl(char *s, const char *s1, const char *s2)
{
    char *ptr;
    while (ptr = strstr(s, s1)) // 如果在s中找到s1
    {
        memmove(ptr + strlen(s2), ptr + strlen(s1), strlen(ptr) - strlen(s1) + 1);
        memcpy(ptr, &s2[0], strlen(s2));
    }
    return s;
}

// 打开输入文件
int open_input_file(const char *src_name)
{
    // 打开音视频文件
    int ret = avformat_open_input(&src_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0)
    {
        src_video = src_fmt_ctx->streams[video_index];
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
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
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
    // 找到音频流的索引
    audio_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        src_audio = src_fmt_ctx->streams[audio_index];
    }
    return 0;
}

int open_output_file(const char *dest_name)
{
    int ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dest_name);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dest_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&dst_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open output_file %s.\n", dest_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open output_file %s.\n", dest_name);
    if (video_index >= 0) // 创建编码器实例和新的视频流
    {
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频编码器
        const AVCodec *video_codec = avcodec_find_encoder(video_codec_id);
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
        // 设置参数
        video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx);               // 帧率
        video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx);                // 时间基
        video_encode_ctx->gop_size = 12;                                                          // 关键帧间隔
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx);                            // 视频宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx);                           // 视频高度
        video_encode_ctx->pix_fmt = (enum AVPixelFormat)av_buffersink_get_format(buffersink_ctx); // 视频的像素格式
        // video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        //  AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (dst_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        // 创建数据流
        dst_video = avformat_new_stream(dst_fmt_ctx, nullptr);
        // 把编码器的参数赋值给数据流
        avcodec_parameters_from_context(dst_video->codecpar, video_encode_ctx);
        dst_video->codecpar->codec_tag = 0;
    }
    if (audio_index >= 0)
    {                                                                     // 源文件有音频流，就给目标文件创建音频流
        AVStream *dest_audio = avformat_new_stream(dst_fmt_ctx, nullptr); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    // 写文件头
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

/**
 * 初始化滤镜
 */
int init_filter(const char *filters_desc)
{
    av_log(nullptr, AV_LOG_INFO, "filters_desc : %s.\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");      // 获取输出滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                  // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc();                 // 分配滤镜的输入输出参数
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    filter_graph = avfilter_graph_alloc(); // 分配滤镜图
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
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source.\n");
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Canno create buffer sink\n");
        return ret;
    }
    // 将二进制选项设置为整数列表，
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot ser output pixel format\n");
        return ret;
    }
    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;
    // 把采用过滤字符串描述的图像添加到滤镜图
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot parse graph string.\n");
        return ret;
    }

    // 检查过滤字符串的有效性，并配置滤镜图中所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs);  // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    av_log(nullptr, AV_LOG_INFO, "Success initialize filter.\n");
    return 0;
}

int output_video(AVFrame *filt_frame)
{
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, filt_frame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    AVPacket *packet = av_packet_alloc();
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
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 转换时间戳
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dst_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(dst_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_packet_free(&packet);
    return ret;
}

/**
 * 对视频重新解码
 */
int recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame)
{
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return -1;
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
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 把原始数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            break;
        }
        while (1)
        {
            // 从输出滤镜的接收器获取一个已经加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            output_video(filt_frame);
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *filters_desc = "fps=15";
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
        av_log(nullptr, AV_LOG_ERROR, "please enter command such as:\n ./videofiler src_filename filters_desc\n");
        return -1;
    }

    // 打开输入文本和视频解码器
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }

    // 根据第一个滤镜名称构造出文件的名称
    const char *filter_name = get_filter_name(filters_desc);
    char dest_name[64];
    sprintf(dest_name, "output_%s.mp4", filter_name);
    av_log(nullptr, AV_LOG_INFO, "dest_name: %s\n", dest_name);
    // 修改视频速率，要考虑音频速率是否也跟着变化，setpts表示调整视频播放速度
    // 在字符串 filters_desc 中查找子字符串 setpts= 的首次出现位置。
    int is_setpts = (strstr(filters_desc, "setpts=") != nullptr);
    int is_trim = (strstr(filters_desc, "trim=") != nullptr);
    // 下面把过滤字符串中的特定串替换为相应数值
    char total_frames[16]; // 总帧数
    sprintf(total_frames, "%ld", src_video->nb_frames);
    // start_frame 可以使用算术表达式
    filters_desc = strrpl((char *)filters_desc, "TOTAL_FRAMES", total_frames);

    // 单出间隔
    int interval = 2;
    if (argc > 3)
    {
        interval = atoi(argv[3]);
    }

    // 开始淡出时间
    char start_time[16];
    sprintf(start_time, "%.2f", src_fmt_ctx->duration / 1000 / 1000.0 - interval);
    // start_time 不能使用算术表达式
    filters_desc = strrpl((char *)filters_desc, "START_TIME", start_time);

    // 初始化滤镜
    init_filter(filters_desc);

    // 打开输出文件
    if (open_output_file(dest_name) < 0)
    {
        return -1;
    }

    int ret = -1;
    AVPacket *packet = av_packet_alloc();           // 分配数据包
    AVFrame *frame = av_frame_alloc();              // 分配数据帧
    AVFrame *filt_frame = av_frame_alloc();         // 分配过滤后的数据帧
    while (av_read_frame(src_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        if (packet->stream_index == video_index)
        {
            packet->stream_index = 0;
            recode_video(packet, frame, filt_frame);
        }
        else if (packet->stream_index == audio_index && !is_setpts && !is_trim)
        {
            packet->stream_index = 1;
            // 音频包不重新编码，直接写入目标文件
            ret = av_write_frame(dst_fmt_ctx, packet);
            if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(packet);
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(NULL);                      // 传入一个空帧，冲走编码缓存
    av_write_trailer(dst_fmt_ctx);           // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success process video file.\n");

    // 释放资源
    avcodec_close(video_decode_ctx);         // 关闭解码器实例
    avcodec_free_context(&video_decode_ctx); // 释放解码器
    avcodec_close(video_encode_ctx);         // 关闭编码器实例
    avcodec_free_context(&video_encode_ctx); // 释放编码器
    avfilter_free(buffersrc_ctx);            // 释放输入滤镜实例
    avfilter_free(buffersink_ctx);           // 释放输出滤镜实例
    avfilter_graph_free(&filter_graph);      // 释放滤镜图
    avio_close(dst_fmt_ctx->pb);             // 关闭输出流
    avformat_free_context(dst_fmt_ctx);      // 释放输出实例
    avformat_close_input(&src_fmt_ctx);      // 关闭打开文件
    av_packet_free(&packet);                 // 释放数据包
    av_frame_free(&frame);                   // 释放数据帧
    av_frame_free(&filt_frame);              // 释放数据帧
    return 0;
}