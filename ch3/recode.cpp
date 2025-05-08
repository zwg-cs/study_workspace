#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
    // #include <libavutil/opt.h>
}

AVFormatContext *src_fmt_ctx = nullptr;     // 输入文件封装实例
AVFormatContext *dst_fmt_ctx = nullptr;     // 输出文件封装实例
AVCodecContext *video_decode_ctx = nullptr; // 视频解码器实例
AVCodecContext *video_encode_ctx = nullptr; // 视频编码器实例
AVStream *src_video_stream = nullptr;       // 源文件视频流
AVStream *src_audio_steram = nullptr;       // 源文件音频流
AVStream *dst_video_stream = nullptr;       // 目标文件视频流
int src_video_stream_index = -1;            // 源文件视频流索引
int src_audio_stream_index = -1;            // 源文件音频流索引

/**
 * 打开输入文件
 */
int open_input_file(const char *src_filename)
{
    int ret = 0;

    /* 打开输入文件 */
    ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open src file %s.\n", src_filename);

    /* 查找流信息 */
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_filename);
        return -1;
    }

    /* 获取视频流和音频流 */
    src_video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (src_video_stream_index >= 0)
    {
        // 查找视频解码器
        src_video_stream = src_fmt_ctx->streams[src_video_stream_index];
        enum AVCodecID video_codec_id = src_video_stream->codecpar->codec_id;
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video codec not find.\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_decode_ctx is null.\n");
            return -1;
        }

        /* 视频流参数复制到解码器实例 */
        avcodec_parameters_to_context(video_decode_ctx, src_video_stream->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video decode ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video stream in file %s.\n", src_filename);
        return -1;
    }
    /* 获取音频流 */
    src_audio_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (src_audio_stream_index >= 0)
    {
        src_audio_steram = src_fmt_ctx->streams[src_audio_stream_index];
    }
    return 0;
}

/**
 * 打开输出文件
 */
int open_output_file(const char *dst_filename)
{
    // 分配实例
    int ret = avformat_alloc_output_context2(&dst_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't alloc dst file %s.\n", dst_filename);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&dst_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open dst file %s.\n", dst_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open dst file %s.\n", dst_filename);
    // 创建编码器实例的新的视频流
    if (src_video_stream_index >= 0)
    {
        enum AVCodecID video_codec_id = src_video_stream->codecpar->codec_id;
        const AVCodec *video_codec = avcodec_find_encoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "video_codec is not found.\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "video encode ctx is null.\n");
            return -1;
        }
        // 把源视频流中的参数复制给编码器参数
        avcodec_parameters_to_context(video_encode_ctx, src_video_stream->codecpar);
        // 注意：帧率和时间基需要单独复制
        video_encode_ctx->framerate = src_video_stream->r_frame_rate;
        // 如果framerate.num值过大，会导致第一秒变灰色
        if (video_encode_ctx->framerate.num > 60)
        {
            video_encode_ctx->framerate = (AVRational){25, 1};
        }
        video_encode_ctx->time_base = src_audio_steram->time_base; // 时间基
        video_decode_ctx->gop_size = 12;                           // 关键帧的间隔距离
        // video_decode_ctx->max_b_frames = 0; //0表示不使用关键帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (dst_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // 打开编码器实例
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video encode ctx.\n");
            return -1;
        }

        // 创建数据里
        dst_video_stream = avformat_new_stream(dst_fmt_ctx, nullptr);
        // 把编码器实例的参数赋值给目标视频流
        avcodec_parameters_from_context(dst_video_stream->codecpar, video_encode_ctx);
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        // dest_video->time_base = src_video->time_base;
        dst_video_stream->codecpar->codec_tag = 0;
    }
    if (src_audio_stream_index >= 0)
    {
        AVStream *dst_audio_stream = avformat_new_stream(dst_fmt_ctx, nullptr);
        avcodec_parameters_copy(dst_audio_stream->codecpar, src_audio_steram->codecpar);
        dst_audio_stream->codecpar->codec_tag = 0;
    }
    // 写入文件头
    ret = avformat_write_header(dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "write file header occur error %d.\n", ret);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n");
    return 0;
}

/**
 *
 */
int output_video(AVFrame *frame)
{
    // 把frame数据发送给编码器
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame to encoder occur error.\n");
        return ret;
    }
    // 这里循环调用avcodec_receive_packet，同样考虑B帧影响，B帧的压缩依靠前后数据
    while (1) // 从编码器获取编码后的packet
    {
        AVPacket *packet = av_packet_alloc();
        ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error.\n");
            break;
        }
        av_packet_rescale_ts(packet, src_video_stream->time_base, dst_video_stream->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(dst_fmt_ctx, packet);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error.\n");
            break;
        }
        av_packet_unref(packet);
    }
    return ret;
}

/**
 * 对视频帧重新解码
 */
int recode_video(AVPacket *pkt, AVFrame *frame)
{
    // 未解压的数据包发送到解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, pkt);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send pkt to decode occur error.\n");
        return ret;
    }

    // 下面循环调用avcodec_receive_frame是因为有B帧影响，在收到一个B帧后，解码器无法解码出原始数据帧，
    // 还需要后面的P帧。此时会返回AVERROR(EAGAIN)，表示信息不足，还需更多信息
    while (1)
    {
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN) ? 0 : 1);
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur erroe.\n");
            break;
        }
        output_video(frame); // 给视频帧编码，并写入压缩后的视频包
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *dst_filename = "recode.mp4";
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        dst_filename = argv[2];
    }

    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }

    if (open_output_file(dst_filename) < 0)
    {
        return -1;
    }

    int ret = -1;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(src_fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == src_video_stream_index) // 视频帧
        {
            pkt->stream_index = 0;
            recode_video(pkt, frame);
        }
        else
        {
            pkt->stream_index = 1;
            ret = av_write_frame(dst_fmt_ctx, pkt); // 音频包直接写入文件
            if (ret < 0)
            {
                av_log(nullptr, AV_LOG_ERROR, "write audio pkt occur error.\n");
                break;
            }
        }
        av_packet_unref(pkt);
    }
    pkt->data = nullptr; // 传入一个空包，冲走解码器缓存
    pkt->size = 0;
    recode_video(pkt, frame); // 对视频帧重新编码
    output_video(nullptr);    // 传入一个空帧，冲走解码器缓存

    av_write_trailer(dst_fmt_ctx); // 写入文件尾
    av_log(nullptr, AV_LOG_INFO, "Success recode file.\n");

    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&pkt);                    // 释放数据包资源
    avio_close(dst_fmt_ctx->pb);             // 关闭输出流
    avcodec_close(video_encode_ctx);         // 关闭编码器器实例
    avcodec_free_context(&video_decode_ctx); // 释放编码器实例
    avcodec_close(video_decode_ctx);         // 关闭解码器实例
    avcodec_free_context(&video_decode_ctx); // 释放解码器实例
    avformat_free_context(dst_fmt_ctx);      // 释放封装器实例
    avformat_close_input(&src_fmt_ctx);      // 关闭音视频文件

    return 0;
}