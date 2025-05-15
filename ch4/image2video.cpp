#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

AVFormatContext *src_fmt_ctx[2] = {nullptr, nullptr};   // 两个输入实例
AVFormatContext *dst_fmt_ctx = nullptr;                 // 输出示例
AVCodecContext *img_decode_ctx[2] = {nullptr, nullptr}; // 两个图像解码器实例
AVCodecContext *video_encode_ctx = nullptr;             // 视频编码器实例
int video_index[2] = {-1, -1};                          // 两个video stream 索引
AVStream *src_video = nullptr;                          // 源文件视频流
AVStream *dst_video = nullptr;                          // 目标文件视频流
struct SwsContext *swsContext[2] = {nullptr, nullptr};  // 图像转换器实例
AVFrame *yuv_frame[2] = {nullptr, nullptr};             // yuv数据帧
int packet_index = 0;                                   // 数据帧的索引

int open_input_file(int seq, const char *filename)
{
    // 打开封装实例
    int ret = avformat_open_input(&src_fmt_ctx[seq], filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s %d.\n", filename, ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx[seq], nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info %s.\n", filename);
        return -1;
    }
    // 获取视频流索引
    video_index[seq] = av_find_best_stream(src_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index[seq] >= 0)
    {
        AVStream *src_video = src_fmt_ctx[seq]->streams[video_index[seq]];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec is null %s.\n", filename);
            return -1;
        }
        // 分配解码器实例
        img_decode_ctx[seq] = avcodec_alloc_context3(video_codec);
        if (!img_decode_ctx[seq])
        {
            av_log(nullptr, AV_LOG_ERROR, "img_decode_ctx[%d] is nullptr.\n", seq);
        }
        // 从流中复制参数给解码器
        avcodec_parameters_to_context(img_decode_ctx[seq], src_video->codecpar);
        // 打开解码器实例
        ret = avcodec_open2(img_decode_ctx[seq], video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Open img_decode_ctx[%d] failed.\n", seq);
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "not find video steram %s.\n", filename);
        return -1;
    }

    return 0;
}

int open_output_file(const char *dst_filename)
{
    // 分配输出实例
    int ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc output file %s.\n", dst_filename);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&dst_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "can't open dst file %s.\n", dst_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_ERROR, "Success open dst file %s.\n", dst_filename);
    // 创建编码器实例和新的流
    if (video_index[0] >= 0)
    {
        src_video = src_fmt_ctx[0]->streams[video_index[0]];
        // 查找视频编码器
        const AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video codec not found %s.\n", dst_filename);
            return -1;
        }
        // 分配编码器实例
        video_encode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_encode_ctx is null %s.\n", dst_filename);
            return -1;
        }
        // 设置编码器参数
        video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P;         // 设置像素格式
        video_encode_ctx->width = src_video->codecpar->width;   // 宽
        video_encode_ctx->height = src_video->codecpar->height; // 高
        video_encode_ctx->framerate = (AVRational){25, 1};      // 帧率
        video_encode_ctx->time_base = (AVRational){1, 25};      // 时间基
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (dst_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        // 打开编码器实例
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        // 给目标文件创建数据流
        dst_video = avformat_new_stream(dst_fmt_ctx, nullptr);
        // 通过编码器复制参数到数据流
        avcodec_parameters_from_context(dst_video->codecpar, video_encode_ctx);
        dst_video->codecpar->codec_tag = 0;
    }
    // 写入文件头
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");
    return 0;
}

int init_sws_context(int seq)
{
    // 视频像素格式为YUV420P
    enum AVPixelFormat target_format = AV_PIX_FMT_YUV420P;
    swsContext[seq] = sws_getContext(img_decode_ctx[seq]->width, img_decode_ctx[seq]->height, img_decode_ctx[seq]->pix_fmt,
                                     video_encode_ctx->width, video_encode_ctx->height, target_format,
                                     SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext[seq] == nullptr)
    {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null %d.\n", seq);
        return -1;
    }
    yuv_frame[seq] = av_frame_alloc();                 // 分配一个像素帧
    yuv_frame[seq]->format = target_format;            // 设置像素格式
    yuv_frame[seq]->width = video_encode_ctx->width;   // 像素帧宽
    yuv_frame[seq]->height = video_encode_ctx->height; // 像素帧高
    // 分配缓冲区空间
    av_image_alloc(yuv_frame[seq]->data, yuv_frame[seq]->linesize,
                   video_encode_ctx->width, video_encode_ctx->height, target_format, 1);

    return 0;
}

int output_video(AVFrame *frame)
{
    // 数据帧送入编码器
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame to encode occur error %d.\n", ret);
        return ret;
    }
    AVPacket *packet = av_packet_alloc();
    while (1)
    {
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
        // 修改时间戳
        av_packet_rescale_ts(packet, src_video->time_base, dst_video->time_base);
        ret = av_write_frame(dst_fmt_ctx, packet);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

int recode_video(int seq, AVPacket *pacekt, AVFrame *frame)
{
    int ret = avcodec_send_packet(img_decode_ctx[seq], pacekt);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send pacekt to decode %d occur error %d.\n", seq, ret);
        return -1;
    }
    while (1)
    {
        ret = avcodec_receive_frame(img_decode_ctx[seq], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "decode %d frame occur error %d.\n", seq, ret);
            break;
        }
        // 转换器把图像从RGB转为YUV
        sws_scale(swsContext[seq], (const uint8_t *const *)frame->data, frame->linesize,
                  0, frame->height, yuv_frame[seq]->data, yuv_frame[seq]->linesize);
        int i = 0;
        while (++i < 100) // 每张图片占据100个视频帧
        {
            yuv_frame[seq]->pts = packet_index++; // 播放时间戳要递增
            output_video(yuv_frame[seq]);         // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename0 = "/root/av/fuzhou.jpg";
    const char *src_filename1 = "/root/av/sea.png";
    const char *dst_filename = "imagetovideo.mp4";

    if (argc > 1)
    {
        src_filename0 = argv[1];
    }
    if (argc > 2)
    {
        src_filename1 = argv[2];
    }
    if (argc > 3)
    {
        dst_filename = argv[3];
    }

    // 打开第一个图像文件
    if (open_input_file(0, src_filename0) < 0)
    {
        return -1;
    }

    // 打开第二个图像文件
    if (open_input_file(1, src_filename1) < 0)
    {
        return -1;
    }
    // 打开输出问价
    if (open_output_file(dst_filename) < 0)
    {
        return -1;
    }
    // 初始化第一个图像转换器实例
    if (init_sws_context(0) < 0)
    {
        return -1;
    }
    // 初始化第二个图像转换器实例
    if (init_sws_context(1) < 0)
    {
        return -1;
    }

    int ret = -1;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    // 把第一张图像转为视频
    while (av_read_frame(src_fmt_ctx[0], packet) >= 0)
    {
        if (packet->stream_index == video_index[0])
        {
            recode_video(0, packet, frame);
        }
        av_packet_unref(packet);
    }
    packet->data = nullptr;
    packet->size = 0;
    recode_video(0, packet, frame);
    // 在视频末尾追加第二张图片
    while (av_read_frame(src_fmt_ctx[1], packet) >= 0)
    {
        if (packet->stream_index == video_index[1])
        {
            recode_video(1, packet, frame);
        }
        av_packet_unref(packet);
    }
    packet->data = nullptr;
    packet->size = 0;
    recode_video(1, packet, frame);
    output_video(nullptr);
    // 写入文件尾
    av_write_trailer(dst_fmt_ctx);

    av_log(NULL, AV_LOG_INFO, "Success convert image to video.\n");

    av_frame_free(&yuv_frame[0]);             // 释放数据帧资源
    av_frame_free(&yuv_frame[1]);             // 释放数据帧资源
    av_frame_free(&frame);                    // 释放数据帧资源
    av_packet_free(&packet);                  // 释放数据包资源
    avio_close(dst_fmt_ctx->pb);              // 关闭输出流
    avcodec_close(img_decode_ctx[0]);         // 关闭视频解码器的实例
    avcodec_free_context(&img_decode_ctx[0]); // 释放视频解码器的实例
    avcodec_close(img_decode_ctx[1]);         // 关闭视频解码器的实例
    avcodec_free_context(&img_decode_ctx[1]); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx);          // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx);  // 释放视频编码器的实例
    sws_freeContext(swsContext[0]);           // 释放图像转换器的实例
    sws_freeContext(swsContext[1]);           // 释放图像转换器的实例
    avformat_free_context(dst_fmt_ctx);       // 释放封装器的实例
    avformat_close_input(&src_fmt_ctx[0]);    // 关闭音视频文件
    avformat_close_input(&src_fmt_ctx[1]);    // 关闭音视频文件
}