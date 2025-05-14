#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

AVCodecContext *video_decode_ctx = nullptr; // 编码器实例
int pacekt_index = -1;                      // 数据包索引编号

/**
 * 把视频帧保存为jpg图片
 */
int save_png_file(AVFrame *frame, int save_index)
{
    av_log(nullptr, AV_LOG_INFO, "format = %d, width = %d, height = %d.\n", frame->format, frame->width, frame->height);
    char png_name[20] = {0};
    sprintf(png_name, "output_rgb_%03d.png", save_index);
    av_log(nullptr, AV_LOG_INFO, "target image file is %s.\n", png_name);

    enum AVPixelFormat target_format = AV_PIX_FMT_RGB24; // png的像素格式为RGB24
    // 分配图像转换器的实例，指定来源和目标的format,width和height
    struct SwsContext *swsContext = sws_getContext(
        frame->width, frame->height, AV_PIX_FMT_YUV420P,
        frame->width, frame->height, target_format,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsContext)
    {
        av_log(nullptr, AV_LOG_ERROR, "swsContex is nullptr.\n");
        return -1;
    }
    // 分配一个YUVJ数据帧
    AVFrame *rgb_frame = av_frame_alloc();
    rgb_frame->format = target_format;
    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;

    // 分配缓冲区，用于存放转换后的图像数据
    av_image_alloc(rgb_frame->data, rgb_frame->linesize, frame->width, frame->height, target_format, 1);

    // 转换器开始处理图像数据，把YUV图像转为YUVJ图像
    sws_scale(swsContext, (const uint8_t *const *)frame->data, frame->linesize,
              0, frame->height, rgb_frame->data, rgb_frame->linesize);

    // 释放转换器实例
    sws_freeContext(swsContext);

    AVFormatContext *png_fmt_ctx = nullptr;
    // 分配JPEG文件的封装实例
    int ret = avformat_alloc_output_context2(&png_fmt_ctx, nullptr, nullptr, png_name);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output_file %s.\n", png_name);
        return -1;
    }
    // 查找编码器
    const AVCodec *png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "png_codec not found\n");
        return -1;
    }
    // 获取编码器上下文
    AVCodecContext *png_encode_ctx = avcodec_alloc_context3(png_codec);
    if (!png_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "jpg_encode_ctx is null\n");
        return -1;
    }
    // 设置参数
    png_encode_ctx->pix_fmt = target_format;
    png_encode_ctx->width = frame->width;
    png_encode_ctx->height = frame->height;
    png_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
    av_log(nullptr, AV_LOG_INFO, "png codec_id = %d\n", png_encode_ctx->codec_id);
    // 打开编码器实例
    ret = avcodec_open2(png_encode_ctx, png_codec, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open jpg_encode_ctx.\n");
        return -1;
    }
    AVStream *png_stream = avformat_new_stream(png_fmt_ctx, 0);
    ret = avformat_write_header(png_fmt_ctx, nullptr); // 写入头文件
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    ret = avcodec_send_frame(png_encode_ctx, rgb_frame);
    while (ret == 0)
    {
        packet->stream_index = 0;
        ret = avcodec_receive_packet(png_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        ret = av_write_frame(png_fmt_ctx, packet);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }

    // 写入文件尾
    av_write_trailer(png_fmt_ctx);
    // 释放资源
    av_packet_free(&packet);
    av_frame_free(&rgb_frame);
    avcodec_close(png_encode_ctx);
    avcodec_free_context(&png_encode_ctx);
    avio_close(png_fmt_ctx->pb);
    avformat_close_input(&png_fmt_ctx);
    return 0;
}

/**
 * 解码视频
 */
int decode_video(AVPacket *packet, AVFrame *frame, int save_index)
{ // 未解压的数据包发送给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send pacekt to decoder occur error.\n");
        return -1;
    }
    while (1)
    {
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
        pacekt_index++;
        if (pacekt_index < save_index) // 没有找到对应序号
        {
            return AVERROR(EAGAIN);
        }
        save_png_file(frame, save_index);
        break;
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    int save_index = 0; // 保存第几帧
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        save_index = atoi(argv[2]);
    }

    // 打开源文件
    AVFormatContext *src_fmt_ctx = nullptr;
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_filename);
        return -1;
    }

    // 查找视频流
    int video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index >= 0)
    {
        AVStream *video_stream = src_fmt_ctx->streams[video_stream_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 找到解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find video_codec.\n");
            return -1;
        }
        // 分配解码器实例
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "Alloc video_decode_ctx occur error.\n");
            return -1;
        }
        // 复制参数到解码器
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        // 打开解码器
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "open decode ctx occur error.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "not find video stream.\n");
    }

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc();    // 分配一个数据帧
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index)
        {
            ret = decode_video(packet, frame, save_index); // 解码视频包
            if (ret == 0)
            {
                break; // 只保存一幅图像
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(nullptr, AV_LOG_INFO, "Success save %d_index frame as jpg file.\n", save_index);
    // 释放资源
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(video_decode_ctx);
    avcodec_free_context(&video_decode_ctx);
    avformat_close_input(&src_fmt_ctx);
}