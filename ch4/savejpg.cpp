#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

AVCodecContext *video_decode_ctx = nullptr; // 编码器实例
int pacekt_index = -1;                      // 数据包索引编号

/**
 * 把视频帧保存为jpg图片
 */
int save_jpg_file(AVFrame *frame, int save_index)
{
    av_log(nullptr, AV_LOG_INFO, "format = %d, width = %d, height = %d.\n", frame->format, frame->width, frame->height);
    char jpg_name[20] = {0};
    sprintf(jpg_name, "output_%03d.jpg", save_index);
    av_log(nullptr, AV_LOG_INFO, "target image file is %s.\n", jpg_name);

    AVFormatContext *jpg_fmt_ctx = nullptr;
    // 分配JPEG文件的封装实例
    int ret = avformat_alloc_output_context2(&jpg_fmt_ctx, nullptr, nullptr, jpg_name);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output_file %s.\n", jpg_name);
        return -1;
    }
    // 查找编码器
    const AVCodec *jpg_codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpg_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "jpg_codec not found\n");
        return -1;
    }
    // 获取编码器上下文
    AVCodecContext *jpg_encode_ctx = avcodec_alloc_context3(jpg_codec);
    if (!jpg_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "jpg_encode_ctx is null\n");
        return -1;
    }
    // 设置参数
    jpg_encode_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpg_encode_ctx->width = frame->width;
    jpg_encode_ctx->height = frame->height;
    jpg_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
    av_log(nullptr, AV_LOG_INFO, "jpg codec_id = %d\n", jpg_encode_ctx->codec_id);
    // 打开编码器实例
    ret = avcodec_open2(jpg_encode_ctx, jpg_codec, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open jpg_encode_ctx.\n");
        return -1;
    }
    AVStream *jpg_stream = avformat_new_stream(jpg_fmt_ctx, 0);
    ret = avformat_write_header(jpg_fmt_ctx, nullptr); // 写入头文件
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    ret = avcodec_send_frame(jpg_encode_ctx, frame);
    while (ret == 0)
    {
        packet->stream_index = 0;
        ret = avcodec_receive_packet(jpg_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        ret = av_write_frame(jpg_fmt_ctx, packet);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
    }
    av_packet_unref(packet); // 清除数据包
    // 写入文件尾
    av_write_trailer(jpg_fmt_ctx);
    // 释放资源
    av_packet_free(&packet);
    avcodec_close(jpg_encode_ctx);
    avcodec_free_context(&jpg_encode_ctx);
    avio_close(jpg_fmt_ctx->pb);
    avformat_close_input(&jpg_fmt_ctx);
    return 0;
}

/**
 * 解码视频
 */
int decode_video(AVPacket *packet, AVFrame *frame, int save_index)
{
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
        }
        pacekt_index++;
        if (pacekt_index < save_index) // 没有找到对应序号
        {
            return AVERROR(EAGAIN);
        }
        save_jpg_file(frame, save_index);
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
                break;
            }
        }
    }
    av_log(nullptr, AV_LOG_INFO, "Success save %d_index frame as jpg file.\n", save_index);
    // 释放资源
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(video_decode_ctx);
    avcodec_free_context(&video_decode_ctx);
    avformat_close_input(&src_fmt_ctx);
}