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

AVFormatContext *in_fmt_ctx[2] = {nullptr, nullptr};      // 输入文件的封装器实例
AVCodecContext *audio_decode_ctx[2] = {nullptr, nullptr}; // 音频解码器的实例
int audio_index[2] = {-1, -1};                            // 音频流的索引
AVStream *src_audio[2] = {nullptr, nullptr};              // 源文件的音频流
AVStream *dest_audio = nullptr;                           // 目标文件的音频流
AVFormatContext *out_fmt_ctx;                             // 输出文件的封装器实例
AVCodecContext *audio_encode_ctx = nullptr;               // 音频编码器的实例

AVFilterContext *buffersrc_ctx[2] = {nullptr, nullptr}; // 输入滤镜的实例
AVFilterContext *buffersink_ctx = nullptr;              // 输出滤镜的实例
AVFilterGraph *filter_graph = nullptr;                  // 滤镜图

// 打开输入文件
int open_input_file(int seq, const char *src_name)
{
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx[seq], src_name, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx[seq], nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到音频流的索引
    audio_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index[seq] >= 0)
    {
        src_audio[seq] = in_fmt_ctx[seq]->streams[audio_index[seq]];
        enum AVCodecID audio_codec_id = src_audio[seq]->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec *)avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx[seq] = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx[seq])
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_decode_ctx is nullptr\n");
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx[seq], src_audio[seq]->codecpar);
        ret = avcodec_open2(audio_decode_ctx[seq], audio_codec, nullptr); // 打开解码器的实例
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    return 0;
}

// 初始化滤镜
int init_filter(const char *filters_desc)
{
    av_log(nullptr, AV_LOG_INFO, "filters_desc : %s\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersrc[2];
    buffersrc[0] = avfilter_get_by_name("abuffer");                   // 第一个输入滤镜
    buffersrc[1] = avfilter_get_by_name("abuffer");                   // 第二个输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink"); // 输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc();                   // 输出滤镜的输入节点
    AVFilterInOut *outputs[2];                                        // 输入滤镜的输出节点数组
    outputs[0] = avfilter_inout_alloc();                              // 第一个输入滤镜的输出节点
    outputs[1] = avfilter_inout_alloc();                              // 第二个输入滤镜的输出节点
    filter_graph = avfilter_graph_alloc();                            // 滤镜图
    if (!inputs || !outputs[0] || !outputs[1] || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    // 第一个输入滤镜参数设定
    char ch_layout0[128];
    av_channel_layout_describe(&audio_decode_ctx[0]->ch_layout, ch_layout0, sizeof(ch_layout0));
    int nb_channels0 = audio_decode_ctx[0]->ch_layout.nb_channels;
    char args0[512];
    snprintf(args0, sizeof(args0), "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
             audio_decode_ctx[0]->sample_rate, av_get_sample_fmt_name(audio_decode_ctx[0]->sample_fmt),
             ch_layout0, nb_channels0,
             audio_decode_ctx[0]->time_base.num, audio_decode_ctx[0]->time_base.den);
    // 创建第一个输入滤镜
    ret = avfilter_graph_create_filter(&buffersrc_ctx[0], buffersrc[0], "in0", args0, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer0 source\n");
        return ret;
    }

    // 第二个输入滤镜参数设定
    char ch_layout1[128];
    av_channel_layout_describe(&audio_decode_ctx[1]->ch_layout, ch_layout1, sizeof(ch_layout1));
    int nb_channels1 = audio_decode_ctx[1]->ch_layout.nb_channels;
    char args1[512];
    snprintf(args1, sizeof(args1), "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
             audio_decode_ctx[1]->sample_rate, av_get_sample_fmt_name(audio_decode_ctx[1]->sample_fmt),
             ch_layout1, nb_channels1,
             audio_decode_ctx[1]->time_base.num, audio_decode_ctx[1]->time_base.den);
    // 创建第二个输出滤镜
    ret = avfilter_graph_create_filter(&buffersrc_ctx[1], buffersrc[1], "in1", args1, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer1 source\n");
        return ret;
    }
    // 创建输出滤镜
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // 设置输出滤镜的采样格式
    enum AVSampleFormat sample_fmts[2] = {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE};
    ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample format\n");
        return ret;
    }

    // 设置输入滤镜的输出节点
    outputs[0]->name = av_strdup("0:a");
    outputs[0]->filter_ctx = buffersrc_ctx[0];
    outputs[0]->pad_idx = 0;
    outputs[0]->next = outputs[1]; //  注意这里要指向下一个输入输出参数
    outputs[1]->name = av_strdup("1:a");
    outputs[1]->filter_ctx = buffersrc_ctx[1];
    outputs[1]->pad_idx = 0;
    outputs[1]->next = nullptr;
    // 设置输出滤镜的输入节点
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 将字符串描述的滤镜加入到滤镜图中
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, outputs, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查滤镜有效性，并连接
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }

    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(outputs); // 释放滤镜的输出参数
    av_log(nullptr, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
    return ret;
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
    if (audio_index[0] >= 0)
    { // 创建编码器实例和新的音频流
        // 查找音频编码器
        AVCodec *audio_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_MP3);
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
        audio_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx); // 时间基
        // 采样格式
        audio_encode_ctx->sample_fmt = (enum AVSampleFormat)av_buffersink_get_format(buffersink_ctx);
        // 采样率，单位赫兹每秒
        audio_encode_ctx->sample_rate = av_buffersink_get_sample_rate(buffersink_ctx);
        av_buffersink_get_ch_layout(buffersink_ctx, &audio_encode_ctx->ch_layout); // 声道布局
        ret = avcodec_open2(audio_encode_ctx, audio_codec, nullptr);               // 打开编码器的实例
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
            return -1;
        }
        dest_audio = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标音频流
        avcodec_parameters_from_context(dest_audio->codecpar, audio_encode_ctx);
        dest_audio->codecpar->codec_tag = 0;
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

// 给音频帧编码
int output_audio(AVFrame *frame)
{
    // 音频帧送入编码器
    int ret = avcodec_send_frame(audio_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    AVPacket *packet = av_packet_alloc();
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
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 时间戳转换
        av_packet_rescale_ts(packet, audio_encode_ctx->time_base, dest_audio->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 写入文件
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    return ret;
}

// 从指定的输入文件获取一个数据帧
int get_frame(AVFormatContext *fmt_ctx, AVCodecContext *decode_ctx, int index, AVPacket *packet, AVFrame *frame)
{
    int ret = 0;
    while (av_read_frame(fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == index)
        {
            ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0)
            {
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
    av_packet_unref(packet);
    return ret;
}

int recode_audio(AVPacket **packet, AVFrame **frame, AVFrame *filter_frame)
{
    // 把未解压的数据包放入解码器
    int ret = avcodec_send_packet(audio_decode_ctx[0], packet[0]);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }

    while (1)
    {
        // 从解码器获取数据帧
        ret = avcodec_receive_frame(audio_decode_ctx[0], frame[0]);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            return ret;
        }

        // 把第一个输入文件的数据送入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx[0], frame[0], AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return ret;
        }

        // 从指定的输入文件获取一个数据帧
        ret = get_frame(in_fmt_ctx[1], audio_decode_ctx[1], audio_index[1], packet[1], frame[1]);
        if (ret == 0) // 第二个文件没到末尾，就把数据帧添加到输入滤镜的缓冲区
        {
            ret = av_buffersrc_add_frame_flags(buffersrc_ctx[1], frame[1], AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                return ret;
            }
        }
        else // 第二个文件已到末尾，就把空白帧添加到输入滤镜的缓冲区
        {
            ret = av_buffersrc_add_frame_flags(buffersrc_ctx[1], nullptr, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                return ret;
            }
        }

        // 从滤镜图中接受一个已经加工的过滤帧
        while (1)
        {
            ret = av_buffersink_get_frame(buffersink_ctx, filter_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                return ret;
            }
            else if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                return ret;
            }
            output_audio(filter_frame); // 给音频编码
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_name0 = "1.mp3";
    const char *src_name1 = "2.mp3";
    const char *dest_name = "output_mixaudio.mp3";
    const char *filters_desc = "";

    if (argc > 1)
    {
        src_name0 = argv[1];
    }
    if (argc > 2)
    {
        src_name1 = argv[2];
    }
    if (argc > 3)
    {
        filters_desc = argv[3]; // 过滤字符串从命令行读取
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "please enter command such as:\n  ./mixaudio src_name0 src_name1 filters_desc\n");
        return -1;
    }

    // 打开第一个输入文件
    if (open_input_file(0, src_name0) < 0)
    {
        return -1;
    }
    // 打开第二个输入文件
    if (open_input_file(1, src_name1) < 0)
    {
        return -1;
    }

    // 初始化滤镜
    init_filter(filters_desc);
    // 打开输出文件
    if (open_output_file(dest_name) < 0)
    {
        return -1;
    }

    int ret = -1;
    AVPacket *packet[2];                      // 数据包数组
    packet[0] = av_packet_alloc();            // 第一个数据包
    packet[1] = av_packet_alloc();            // 第二个数据包
    AVFrame *frame[2];                        // 数据帧数组
    frame[0] = av_frame_alloc();              // 第一个数据帧
    frame[1] = av_frame_alloc();              // 第二个数据帧
    AVFrame *filter_frame = av_frame_alloc(); // 过滤后的数据帧
    while (av_read_frame(in_fmt_ctx[0], packet[0]) >= 0)
    {
        if (packet[0]->stream_index == audio_index[0])
        {
            recode_audio(packet, frame, filter_frame);
        }
        av_packet_unref(packet[0]);
    }
    output_audio(nullptr); // 传入一个空帧，冲走编码器缓存

    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(nullptr, AV_LOG_INFO, "Success mix audio file.\n");
    // 释放资源
    avcodec_free_context(&audio_decode_ctx[0]); // 释放第一个音频解码器（ffmpeg 7版本不需要avcodec_close()）
    avcodec_free_context(&audio_decode_ctx[1]); // 释放第二个音频编码器
    avformat_close_input(&in_fmt_ctx[0]);       // 释放第一个音频输入文件
    avformat_close_input(&in_fmt_ctx[1]);       // 释放第二个音频输入文件
    avfilter_free(buffersrc_ctx[0]);            // 释放输入滤镜的实例
    avfilter_free(buffersrc_ctx[1]);            // 释放输入滤镜的实例
    avfilter_free(buffersink_ctx);              // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph);         // 释放滤镜图资源
    avcodec_free_context(&audio_encode_ctx);    // 释放音频编码器的实例
    avformat_free_context(out_fmt_ctx);         // 释放封装器的实例
    av_frame_free(&frame[0]);                   // 释放数据帧资源
    av_frame_free(&frame[1]);                   // 释放数据帧资源
    av_frame_free(&filter_frame);               // 释放数据帧资源
    av_packet_free(&packet[0]);                 // 释放数据包资源
    av_packet_free(&packet[1]);                 // 释放数据包资源
    return 0;
}