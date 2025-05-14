#include <iostream>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

AVFormatContext *src_fmt_ctx = nullptr;             // 输入文件封装器实例
AVFormatContext *gif_fmt_ctx = nullptr;             // gif图像封装器实例
AVCodecContext *video_decode_ctx = nullptr;         // video解码器实例
AVCodecContext *gif_encode_ctx = nullptr;           // gif编码器实例
AVStream *video_stream = nullptr;                   // 输入视频流
AVStream *gif_stream = nullptr;                     // gif数据流
AVFrame *rgb_frame = nullptr;                       // RGB数据帧
int video_stream_index = -1;                        // 视频流索引
enum AVPixelFormat target_format = AV_PIX_FMT_BGR8; // gif像素格式
struct SwsContext *swsContext = nullptr;            // 图像转换器的实例

/**
 * 打开输入文件
 */
int open_input_file(const char *src_filename)
{
    // 打开音视频
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_filename);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(src_fmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index >= 0)
    {
        video_stream = src_fmt_ctx->streams[video_stream_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
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
    return 0;
}

/**
 * 打开输出文件
 */
int open_output_file(const char *gif_name)
{
    // 分配实例
    int ret = avformat_alloc_output_context2(&gif_fmt_ctx, nullptr, nullptr, gif_name);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", gif_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&gif_fmt_ctx->pb, gif_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", gif_name);
        return -1;
    }
    // 查找编码器
    const AVCodec *gif_codec = avcodec_find_encoder(AV_CODEC_ID_GIF);
    if (!gif_codec)
    {
        av_log(NULL, AV_LOG_ERROR, "gif_codec not found\n");
        return -1;
    }
    // 分配编码器实例
    gif_encode_ctx = avcodec_alloc_context3(gif_codec);
    if (!gif_encode_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "gif_encode_ctx is null\n");
        return -1;
    }
    // 设置参数
    gif_encode_ctx->pix_fmt = target_format;             // 像素格式
    gif_encode_ctx->width = video_decode_ctx->width;     // 宽
    gif_encode_ctx->height = video_decode_ctx->height;   // 高
    gif_encode_ctx->time_base = video_stream->time_base; // 时间基
    av_log(NULL, AV_LOG_INFO, "gif codec_id = %d\n", gif_encode_ctx->codec_id);
    // 打开编码器实例
    ret = avcodec_open2(gif_encode_ctx, gif_codec, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }

    // 创建数据流
    gif_stream = avformat_new_stream(gif_fmt_ctx, 0);
    // 编码器参赛复制给目标流
    avcodec_parameters_from_context(gif_stream->codecpar, gif_encode_ctx);
    // 写入文件头
    ret = avformat_write_header(gif_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }

    return 0;
}

/**
 * 初始化图像转换器实例
 */
int init_sws_context(void)
{
    // 分配图像转换器实例
    swsContext = sws_getContext(video_decode_ctx->width, video_decode_ctx->height, AV_PIX_FMT_YUV420P,
                                video_decode_ctx->width, video_decode_ctx->height, target_format,
                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext == nullptr)
    {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null\n");
        return -1;
    }

    rgb_frame = av_frame_alloc();                 // 分配一个数据帧
    rgb_frame->format = target_format;            // 像素格式
    rgb_frame->width = video_decode_ctx->width;   // 宽
    rgb_frame->height = video_decode_ctx->height; // 高
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                   video_decode_ctx->width, video_decode_ctx->height, target_format, 1);
    return 0;
}

int save_gif_file(AVFrame *frame, int save_index)
{
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    av_log(NULL, AV_LOG_INFO, "format = %d, width = %d, height = %d\n",
           frame->format, frame->width, frame->height);

    AVPacket *pacekt = av_packet_alloc();
    int ret = avcodec_send_frame(gif_encode_ctx, frame);
    while (ret == 0)
    {
        ret = avcodec_receive_packet(gif_encode_ctx, pacekt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 时间戳转换时间基
        av_packet_rescale_ts(pacekt, video_stream->time_base, gif_stream->time_base);
        // 向文件写入数据包
        ret = av_write_frame(gif_fmt_ctx, pacekt);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
    }
    av_packet_unref(pacekt);
    return 0;
}

int decode_video(AVPacket *packet, AVFrame *frame, int save_index)
{
    // 数据包送入解码器
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1)
    {
        // 从解码器读取解码后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 转换器开始处理输出图像，把YUV图像转为RGB图像
        sws_scale(swsContext, (const uint8_t *const *)frame->data, frame->linesize,
                  0, frame->height, rgb_frame->data, rgb_frame->linesize);
        rgb_frame->pts = frame->pts;
        save_gif_file(rgb_frame, save_index); // 把视频帧保存为GIF图片
        break;
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    int save_index = 100;
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        save_index = atoi(argv[2]);
    }

    char gif_name[20] = {0};
    sprintf(gif_name, "output_%03d.gif", save_index);
    // 打开输入文件
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }
    // 打开输出文件
    if (open_output_file(gif_name) < 0)
    {
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target image file is %s\n", gif_name);
    // 初始化图像转换器的实例
    if (init_sws_context() < 0)
    {
        return -1;
    }

    int ret = -1;
    int packet_index = -1;                // 数据包的索引序号
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc();    // 分配一个数据帧

    while (av_read_frame(src_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        if (packet->stream_index == video_stream_index) // 视频吧重新编码
        {
            packet_index++;
            decode_video(packet, frame, save_index); // 对视频帧解码
            if (packet_index > save_index)           // 已经采集足够数量帧
            {
                break;
            }
        }
    }

    // 写入文件尾
    av_write_trailer(gif_fmt_ctx);
    av_log(NULL, AV_LOG_INFO, "Success save %d_index frame as gif file.\n", save_index);

    // 释放资源
    sws_freeContext(swsContext);             // 释放图像转换器实例
    avio_close(gif_fmt_ctx->pb);             // 关闭输出流
    av_frame_free(&rgb_frame);               // 释放数据帧资源
    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&packet);                 // 释放数据包资源
    avcodec_close(video_decode_ctx);         // 关闭解码器
    avcodec_free_context(&video_decode_ctx); // 释放视频解码实例
    avcodec_close(gif_encode_ctx);           // 关闭编码器
    avcodec_free_context(&gif_encode_ctx);   // 释放gif编码器实例
    avformat_free_context(gif_fmt_ctx);      // 释放gif封装实例
    avformat_close_input(&src_fmt_ctx);      // 释放输入文件封装实例
}