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
}

#define ARRAY_LEN 4

AVFormatContext *in_fmt_ctx[ARRAY_LEN];      // 输入文件的封装器实例
AVCodecContext *video_decode_ctx[ARRAY_LEN]; // 视频解码器的实例
int video_index[ARRAY_LEN];                  // 视频流的索引
int audio_index = -1;                        // 音频流的索引
AVStream *src_video[ARRAY_LEN];              // 源文件的视频流
AVStream *src_audio = NULL;                  // 源文件的音频流
AVStream *dest_video = NULL;                 // 目标文件的视频流
AVFormatContext *out_fmt_ctx;                // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = NULL;     // 视频编码器的实例

AVFilterContext *buffersrc_ctx[ARRAY_LEN]; // 输入滤镜的实例
AVFilterContext *buffersink_ctx = NULL;    // 输出滤镜的实例
AVFilterGraph *filter_graph = NULL;        // 滤镜图
int i = -1;                                // 数组下标

// 打开输入文件
int open_input_file(int seq, const char *src_name)
{
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx[seq], src_name, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx[seq], NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index[seq] >= 0)
    {
        src_video[seq] = in_fmt_ctx[seq]->streams[video_index[seq]];
        enum AVCodecID video_codec_id = src_video[seq]->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx[seq] = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx[seq])
        {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx[seq], src_video[seq]->codecpar);
        ret = avcodec_open2(video_decode_ctx[seq], video_codec, NULL); // 打开解码器的实例
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    if (seq == 0)
    {
        // 找到音频流的索引
        audio_index = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (audio_index >= 0)
        {
            src_audio = in_fmt_ctx[seq]->streams[audio_index];
        }
    }
    return 0;
}

// 打开输出文件
int open_output_file(const char *dest_name)
{
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dest_name);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dest_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", dest_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open output_file %s.\n", dest_name);
    if (video_index[0] >= 0)
    { // 创建编码器实例和新的视频流
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_encoder(src_video[0]->codecpar->codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "video_encode_ctx is null\n");
            return -1;
        }
        video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx); // 帧率
        video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx);  // 时间基
        // video_encode_ctx->gop_size = 12;                                            // 关键帧的间隔距离
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx);  // 视频宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx); // 视频高度
        // 视频的像素格式（颜色空间）
        video_encode_ctx->pix_fmt = (enum AVPixelFormat)av_buffersink_get_format(buffersink_ctx);
        // video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        //  AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, NULL); // 打开编码器的实例
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
    }
    if (audio_index >= 0)
    {                                                                  // 源文件有音频流，就给目标文件创建音频流
        AVStream *dest_audio = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

// 从指定的输入文件获取一个数据帧
int get_frame(AVFormatContext *fmt_ctx, AVCodecContext *decode_ctx, int index, AVPacket *packet, AVFrame *frame)
{
    int ret = 0;
    while ((ret = av_read_frame(fmt_ctx, packet)) >= 0)
    { // 轮询数据包
        if (packet->stream_index == index)
        {
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0)
            {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    continue;
                }
            }
            break;
        }
    }
    av_packet_unref(packet); // 清除数据包
    return ret;
}

int init_filter(const char *filters_desc)
{
    av_log(NULL, AV_LOG_INFO, "filters_desc : %s\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 输入滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                  // 输入滤镜的输入节点
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    filter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!inputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    const AVFilter *buffersrc[ARRAY_LEN];
    AVFilterInOut *outputs[ARRAY_LEN];
    char args[512];
    i = -1;
    while (++i < ARRAY_LEN)
    {
        buffersrc[i] = avfilter_get_by_name("buffer");
        outputs[i] = avfilter_inout_alloc();
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 video_decode_ctx[i]->width, video_decode_ctx[i]->height, video_decode_ctx[i]->pix_fmt,
                 src_video[i]->time_base.num, src_video[i]->time_base.den,
                 video_decode_ctx[i]->sample_aspect_ratio.num, video_decode_ctx[i]->sample_aspect_ratio.den);
        av_log(NULL, AV_LOG_INFO, "args : %s\n", args);
        // 创建输入滤镜实例，并将其添加到现有的滤镜图
        ret = avfilter_graph_create_filter(&buffersrc_ctx[i], buffersrc[i], "in", args, nullptr, filter_graph);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            return ret;
        }
        char put_name[8];
        snprintf(put_name, sizeof(put_name), "%d:v", i);
        outputs[i]->name = av_strdup(put_name); // 第i路视频流
        outputs[i]->filter_ctx = buffersrc_ctx[i];
        outputs[i]->pad_idx = 0;
        outputs[i]->next = NULL;
        if (i > 0)
        {
            outputs[i - 1]->next = outputs[i]; // 指向下一个输入输出参数
        }
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        return ret;
    }
    // 把采用过滤字符串描述的图形添加到滤镜图（引脚的输出和输入与滤镜容器的相反）
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, outputs, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(outputs); // 释放滤镜的输出参数
    av_log(NULL, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
}

// 给视频帧编码，并写入压缩后的视频包
int output_video(AVFrame *frame)
{
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
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
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对视频帧重新编码
int recode_video(AVPacket **packet, AVFrame **frame, AVFrame *filt_frame)
{
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx[0], packet[0]);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx[0], frame[0]);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            return ret;
        }
        // 把第一个文件的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx[0], frame[0], AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return ret;
        }
        i = 0;
        while (++i < ARRAY_LEN)
        {
            // 从指定的输入文件获取一个数据帧
            ret = get_frame(in_fmt_ctx[i], video_decode_ctx[i], video_index[i], packet[i], frame[i]);
            if (ret == 0)
            { // 后面的文件没到末尾，就把数据帧添加到输入滤镜的缓冲区
                ret = av_buffersrc_add_frame_flags(buffersrc_ctx[i], frame[i], AV_BUFFERSRC_FLAG_KEEP_REF);
                if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    return ret;
                }
            }
            else
            { // 后面的文件已到末尾，就把空白帧添加到输入滤镜的缓冲区
                ret = av_buffersrc_add_frame_flags(buffersrc_ctx[i], NULL, AV_BUFFERSRC_FLAG_KEEP_REF);
                if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    return ret;
                }
            }
        }
        while (1)
        {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                return ret;
            }
            else if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                return ret;
            }
            output_video(filt_frame); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_name[ARRAY_LEN] = {"../fuzhous.mp4", "../seas.mp4", "../seas.mp4", "../fuzhous.mp4"};
    const char *dest_name = "output_mixgrid.mp4";
    int width, height;
    const char *filters_desc = "";
    if (argc < 5)
    {
        av_log(NULL, AV_LOG_ERROR, "please enter command such as:\n  ./mixgrid src0 src1 src2 src3 filters_desc\n");
        return -1;
    }
    src_name[0] = argv[1];
    src_name[1] = argv[2];
    src_name[2] = argv[3];
    src_name[3] = argv[4];
    filters_desc = argv[5]; // 过滤字符串从命令行读取
    i = -1;
    while (++i < ARRAY_LEN)
    {
        if (open_input_file(i, src_name[i]) < 0)
        { // 打开第i+1个输入文件
            return -1;
        }
    }
    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(dest_name) < 0)
    { // 打开输出文件
        return -1;
    }

    int ret = -1;
    AVPacket *packet[ARRAY_LEN];
    AVFrame *frame[ARRAY_LEN];
    i = -1;
    while (++i < ARRAY_LEN)
    {
        packet[i] = av_packet_alloc(); // 分配一个数据包
        frame[i] = av_frame_alloc();   // 分配一个数据帧
    }
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx[0], packet[0]) >= 0)
    {
        if (packet[0]->stream_index == video_index[0])
        {
            packet[0]->stream_index = 0;
            recode_video(packet, frame, filt_frame); // 对视频帧重新编码
        }
        else if (packet[0]->stream_index == audio_index)
        {
            packet[0]->stream_index = 1;
            ret = av_write_frame(out_fmt_ctx, packet[0]);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
    }

    packet[0]->data = NULL; // 传入一个空包，冲走解码缓存
    packet[0]->size = 0;
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(NULL);                      // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx);           // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success mix grid file.\n");

    i = -1;

    while (++i < ARRAY_LEN)
    {
        avfilter_free(buffersrc_ctx[i]);            // 释放输入滤镜的实例
        av_frame_free(&frame[i]);                   // 释放数据帧资源
        av_packet_free(&packet[i]);                 // 释放数据包资源
        avcodec_free_context(&video_decode_ctx[i]); // 释放视频解码器的实例
        avformat_close_input(&in_fmt_ctx[i]);       // 关闭音视频文件
    }
    avfilter_free(buffersink_ctx);           // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph);      // 释放滤镜图资源
    av_frame_free(&filt_frame);              // 释放数据帧资源
    avio_close(out_fmt_ctx->pb);             // 关闭输出流
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx);      // 释放封装器的实例
    return 0;
}