#include <iostream>
#include <vector>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <inttypes.h>
}
using namespace std;
vector<AVFormatContext *> vec_src_fmt_ctx = {nullptr, nullptr};     // 输入容器实例数组
vector<AVCodecContext *> vec_video_decode_ctx = {nullptr, nullptr}; // 输入的视频解码器实例数组
vector<AVStream *> vec_src_video_stream = {nullptr, nullptr};       // 输出实例视频流数组
vector<int> vec_src_video_stream_index = {-1, -1};                  // 输入实例视频流索引数组
vector<int> vec_src_audio_stream_index = {-1, -1};                  // 输入实例音频流索引数组
AVStream *p_dst_video_stream;                                       // 目标视频流
AVFormatContext *p_dst_fmt_ctx;                                     // 输出文件封装实例
AVCodecContext *p_video_encode_ctx;                                 // 视频编码器实例

/**
 * 打开输入文件，分配实例，查找流信息
 */
int open_src_file(int seq, const char *src_filename)
{
    // 打开文件分配实例
    int ret = avformat_open_input(&vec_src_fmt_ctx[seq], src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", src_filename);

    // 查找流信息
    ret = avformat_find_stream_info(vec_src_fmt_ctx[seq], nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_filename);
        return -1;
    }

    // 获取视频流索引
    vec_src_video_stream_index[seq] = av_find_best_stream(vec_src_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "file %s's video stream index is %d.\n", src_filename, vec_src_video_stream_index[seq]);
    if (vec_src_video_stream_index[seq] >= 0)
    {
        // 视频流
        vec_src_video_stream[seq] = vec_src_fmt_ctx[seq]->streams[vec_src_video_stream_index[seq]];
        enum AVCodecID video_codec_id = vec_src_video_stream[seq]->codecpar->codec_id;
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find video codec.\n");
            return -1;
        }
        // 分配解码器实例
        vec_video_decode_ctx[seq] = avcodec_alloc_context3(video_codec);
        if (!vec_video_decode_ctx[seq])
        {
            av_log(nullptr, AV_LOG_ERROR, "alloc video decode ctx occur error.\n");
            return -1;
        }
        // 把视频流中的参数复制解码器实例
        avcodec_parameters_to_context(vec_video_decode_ctx[seq], vec_src_video_stream[seq]->codecpar);
        // 打开编码器实例
        ret = avcodec_open2(vec_video_decode_ctx[seq], video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "open video decode ctx occur error.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find video steram in file %s.\n", src_filename);
        return -1;
    }

    // 找到音频流
    vec_src_audio_stream_index[seq] = av_find_best_stream(vec_src_fmt_ctx[seq], AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av_log(nullptr, AV_LOG_INFO, "file %s's audio stream index is %d.\n\n", src_filename, vec_src_audio_stream_index[seq]);
    return 0;
}

/**
 * 打开dst文件
 */
int open_dst_file(const char *dst_filename)
{
    //  分配实例
    int ret = avformat_alloc_output_context2(&p_dst_fmt_ctx, nullptr, nullptr, dst_filename);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Alloc dst fmt_ctx occur error.\n");
        return -1;
    }
    // 打开输出流
    ret = avio_open(&p_dst_fmt_ctx->pb, dst_filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open ds file occur error.\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open dst file %s.\n", dst_filename);

    if (vec_src_video_stream_index[0] >= 0)
    {
        // 通过src文件的视频解码器给dst创建视频编码器
        AVStream *src_video_stream = vec_src_fmt_ctx[0]->streams[vec_src_video_stream_index[0]];
        enum AVCodecID video_codec_id = src_video_stream->codecpar->codec_id;
        const AVCodec *dst_video_codec = avcodec_find_encoder(video_codec_id);
        if (!dst_video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find dst video codec.\n");
            return -1;
        }
        p_video_encode_ctx = avcodec_alloc_context3(dst_video_codec);
        if (!p_video_encode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "alloc video eocode ctx occur error.\n");
            return -1;
        }
        // 把源视频流中的编解码参数复制给编码器的实例
        avcodec_parameters_to_context(p_video_encode_ctx, src_video_stream->codecpar);
        // 单独复制时间基和帧率
        p_video_encode_ctx->framerate = src_video_stream->r_frame_rate;
        // framerate.num值过大，会导致视频头一秒变灰色
        if (p_video_encode_ctx->framerate.num > 60)
        {
            p_video_encode_ctx->framerate = (AVRational){25, 1};
        }
        p_video_encode_ctx->time_base = src_video_stream->time_base;
        p_video_encode_ctx->gop_size = 12; // 关键帧间隔距离
        // video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (p_dst_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        {
            p_video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        // 打开编码器
        ret = avcodec_open2(p_video_encode_ctx, dst_video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open video encodec ctx.\n");
            return -1;
        }

        // 创建数据流
        p_dst_video_stream = avformat_new_stream(p_dst_fmt_ctx, nullptr);
        avcodec_parameters_from_context(p_dst_video_stream->codecpar, p_video_encode_ctx);
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        // p_dst_video_stream->time_base = src_video->time_base;
        p_dst_video_stream->codecpar->codec_tag = 0;
    }

    // 写入文件头
    ret = avformat_write_header(p_dst_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_INFO, "write file header occur error %d.\n", ret);
        return -1;
    }

    av_log(nullptr, AV_LOG_INFO, "Success write file header.\n\n");
    return 0;
}

// 给视频帧编码
int output_video(AVFrame *frame)
{
    int ret = avcodec_send_frame(p_video_encode_ctx, frame);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send frame to video encode occur error %d.\n", ret);
        return ret;
    }
    AVPacket *pkt = av_packet_alloc();
    while (1)
    {

        ret = avcodec_receive_packet(p_video_encode_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }

        av_packet_rescale_ts(pkt, vec_src_video_stream[0]->time_base, p_dst_video_stream->time_base);
        pkt->stream_index = 0;
        ret = av_write_frame(p_dst_fmt_ctx, pkt);
        av_log(nullptr, AV_LOG_INFO, "pts=%" PRId64 ", dts=%" PRId64 ".\n", pkt->pts, pkt->dts);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return ret;
}

// 对视频包重新编码
int recode_video(int seq, AVPacket *packet, AVFrame *frame, int64_t begin_video_pts)
{
    int ret = avcodec_send_packet(vec_video_decode_ctx[seq], packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send packet to decodeer occur error.\n");
        return -1;
    }

    while (1)
    {
        ret = avcodec_receive_frame(vec_video_decode_ctx[seq], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode pacekt occur error %d.\n", ret);
            break;
        }

        // 如果是第二个文件, 把第二个视频的pts转换到第一个视频
        if (seq == 1)
        {
            int64_t pts = av_rescale_q(frame->pts, vec_src_video_stream[1]->time_base, vec_src_video_stream[0]->time_base);
            frame->pts = pts + begin_video_pts;
        }
        ret = output_video(frame); // 给视频帧编码，然后写入输出文件
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "output video %d occur error %d\n", seq, ret);
            return ret;
        }
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename_0 = "/root/av/fuzhous.mp4"; // 这两个视频的分辨率是一样的
    const char *src_filename_1 = "/root/av/seas.mp4";
    const char *dst_filename = "mergevideo.mp4";

    if (argc > 1)
    {
        src_filename_0 = argv[1];
    }

    if (argc > 2)
    {
        src_filename_1 = argv[2];
    }

    if (argc > 3)
    {
        dst_filename = argv[3];
    }

    // 打开输入文件和输出文件
    if (open_src_file(0, src_filename_0) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open src file %s occur error.\n", src_filename_0);
    }

    if (open_src_file(1, src_filename_1) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open src file %s occur error.\n", src_filename_1);
    }

    if (open_dst_file(dst_filename) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open dst file %s occur error.\n", dst_filename);
    }

    av_log(nullptr, AV_LOG_INFO, "Success open src and dst files.\n");

    int ret = -1;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    // 原样复制第一个数据
    av_log(nullptr, AV_LOG_INFO, "Start video 0.\n");
    while (av_read_frame(vec_src_fmt_ctx[0], packet) >= 0)
    {
        if (packet->stream_index == vec_src_video_stream_index[0])
        {
            ret = recode_video(0, packet, frame, 0);
        }
        av_packet_unref(packet); // 清空数据包
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "recode video %d occur error %d.\n", 0, ret);
            return -1;
        }
    }
    // 传入一个空包，清空解码缓冲区
    packet->data = nullptr;
    packet->size = 0;
    recode_video(0, packet, frame, 0);

    av_log(nullptr, AV_LOG_INFO, "Start video 1.\n");
    // 获取第一个视频的时常,单位微秒
    int64_t begin_sec = vec_src_fmt_ctx[0]->duration;
    // 计算第一个视频末尾的时间基，作为第二个视频开始的时间基
    int64_t begin_video_pts = begin_sec / (1000.0 * 1000.0 * av_q2d(vec_src_video_stream[0]->time_base));
    av_log(nullptr, AV_LOG_INFO, "begin_video_pts=%" PRId64 ".\n", begin_video_pts);
    // 在末尾追加第二个数据
    while (av_read_frame(vec_src_fmt_ctx[1], packet) >= 0)
    {
        if (packet->stream_index == vec_src_video_stream_index[1])
        {
            ret = recode_video(1, packet, frame, begin_video_pts);
        }
        av_packet_unref(packet);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "recode video %d occur error %d.\n", 1, ret);
            return -1;
        }
    }
    // 传入一个空包，清空解码缓冲区
    packet->data = nullptr;
    packet->size = 0;
    recode_video(1, packet, frame, begin_video_pts);
    // 传入一个空帧，清空编码器缓冲区
    output_video(nullptr);
    // 写入文件尾
    av_write_trailer(p_dst_fmt_ctx);
    av_log(nullptr, AV_LOG_INFO, "Success write two video.\n");

    // 释放资源
    av_packet_free(&packet);                // 释放数据包
    av_frame_free(&frame);                  // 释放帧
    avio_close(p_dst_fmt_ctx->pb);          // 关闭输出流
    avcodec_close(vec_video_decode_ctx[0]); // 关闭解码器
    avcodec_close(vec_video_decode_ctx[1]);
    avcodec_close(p_video_encode_ctx);
    avcodec_free_context(&vec_video_decode_ctx[0]); // 释放编解码器实例
    avcodec_free_context(&vec_video_decode_ctx[1]);
    avcodec_free_context(&p_video_encode_ctx);
    avformat_close_input(&vec_src_fmt_ctx[0]); // 关闭src和dst文件
    avformat_close_input(&vec_src_fmt_ctx[1]);
    avformat_free_context(p_dst_fmt_ctx);
    return 0;
}