#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

AVFormatContext *dst_fmt_ctx = nullptr;
AVStream *dst_video_stream = nullptr;
AVCodecContext *video_encode_ctx = nullptr;

/**
 * 打开输出文件
 *
 */
int open_output_file(const char *dst_filename)
{
    // 打开输入文件
    int ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc file %s.\n", dst_filename);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&dst_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", dst_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", dst_filename);

    // 分配编码器
    const AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video codec AV_CODEC_ID_H264.\n");
        return -1;
    }
    // 分配编码器实例
    video_encode_ctx = avcodec_alloc_context3(video_codec);
    if (!video_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "Alloc video encode ctx occur error.\n");
        return -1;
    }
    // 设置编码器参数
    video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P;    // 像素格式
    video_encode_ctx->width = 720;                     // 高
    video_encode_ctx->height = 480;                    // 宽
    video_encode_ctx->framerate = (AVRational){25, 1}; // 帧率
    video_encode_ctx->time_base = (AVRational){1, 25}; // 时间基

    // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
    if (dst_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开编码器
    ret = avcodec_open2(video_encode_ctx, video_codec, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open video encode ctx occur error.\n");
        return -1;
    }

    dst_video_stream = avformat_new_stream(dst_fmt_ctx, nullptr);
    if (!dst_video_stream)
    {
        av_log(nullptr, AV_LOG_ERROR, "New video stream occur error.\n");
        return -1;
    }
    // 从编码器实例复制参数到video流
    avcodec_parameters_from_context(dst_video_stream->codecpar, video_encode_ctx);
    dst_video_stream->codecpar->codec_tag = 0; // 非特殊情况都填0
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Write file header occur error.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");
    return 0;
}

int output_video(AVFrame *frame)
{
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Send fream to encode occur error.\n");
        return -1;
    }
    // 分配一个数据包
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
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 修改时间戳
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dst_video_stream->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(dst_fmt_ctx, packet);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清楚数据包
    }
    av_packet_free(&packet);
    return ret;
}

int main(int argc, char **argv)
{
    const char *dst_filename = "writeyue.mp4";
    if (argc > 1)
    {
        dst_filename = argv[1];
    }

    // 打开输出文件
    if (open_output_file(dst_filename) < 0)
    {
        return -1;
    }
    // 分配一个视频帧
    AVFrame *frame = av_frame_alloc();
    // 设置视频帧参数
    frame->format = video_encode_ctx->pix_fmt;
    frame->width = video_encode_ctx->width;
    frame->height = video_encode_ctx->height;
    int ret = av_frame_get_buffer(frame, 0); // 为数据帧分配缓冲区
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't allocate frame data %d.\n", ret);
        return -1;
    }
    int index = 0;
    while (index < 200)
    {
        ret = av_frame_make_writable(frame); // 确保数据是可写的
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't make frame writable %d.\n", ret);
            return -1;
        }
        int x, y;
        // 写入Y值
        for (y = 0; y < video_encode_ctx->height; ++y)
        {
            for (x = 0; x < video_encode_ctx->width; ++x)
            {
                frame->data[0][y * frame->linesize[0] + x] = 0; // Y值填入0
            }
        }

        // 写入U(Cb)和V(Cr)
        for (y = 0; y < video_encode_ctx->height / 2; ++y)
        {
            for (x = 0; x < video_encode_ctx->width / 2; ++x)
            {
                frame->data[1][y * frame->linesize[1] + x] = 0; // U值填0
                frame->data[2][y * frame->linesize[2] + x] = 0; // V值填0
            }
        }
        frame->pts = index++; // 时间戳递增
        output_video(frame);  // 给帧编码，写入视频包
    }
    av_log(nullptr, AV_LOG_INFO, "Succeee write yuv video.\n");
    av_write_trailer(dst_fmt_ctx);

    // 释放资源
    av_frame_free(&frame);
    avcodec_close(video_encode_ctx);
    avcodec_free_context(&video_encode_ctx);
    avio_close(dst_fmt_ctx->pb);
    avformat_free_context(dst_fmt_ctx);
    return 0;
}