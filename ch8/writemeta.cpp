#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <string.h>
}

const char *get_file_ext(const char *file_name)
{
    const char *dot = strrchr(file_name, '.'); // 从右向左在 file_name 中查找字符 . 的最后一次出现位置。
    if (!dot || dot == file_name)
    {
        return nullptr;
    }
    else
    {
        return dot + 1;
    }
}

int main(int argc, char **argv)
{
    const char *src_name = "/root/av/fuzhou.mp4";
    if (argc > 1)
    {
        src_name = argv[1];
    }
    const char *ext_name = get_file_ext(src_name);
    char dest_name[64];
    sprintf(dest_name, "output.%s", ext_name);
    const char *key = "";   // 元数据的键名
    const char *value = ""; // 元数据的值
    if (argc > 2)
    { // 从命令行获取元数据的键名
        key = argv[2];
    }
    if (argc > 3)
    { // 从命令行获取元数据的键值
        value = argv[3];
    }
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0)
    {
        src_video = in_fmt_ctx->streams[video_index];
    }
    AVStream *src_audio = NULL;
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0)
    {
        src_audio = in_fmt_ctx->streams[audio_index];
    }

    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例
    ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dest_name);
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

    const AVDictionaryEntry *tag = nullptr;
    // 逐个复制元数据
    // while ((tag = av_dict_get(in_fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    // {
    //     av_dict_set(&out_fmt_ctx->metadata, tag->key, tag->value, AV_DICT_IGNORE_SUFFIX);
    // }

    // 整体复制元数据
    ret = av_dict_copy(&out_fmt_ctx->metadata, in_fmt_ctx->metadata, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "copy meta occur error %d\n", ret);
        return -1;
    }
    ret = av_dict_set(&out_fmt_ctx->metadata, key, value, AV_DICT_IGNORE_SUFFIX);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "write meta occur error %d\n", ret);
        return -1;
    }

    av_log(NULL, AV_LOG_INFO, "Success write meta.\n");
    if (video_index >= 0)
    {                                                                  // 源文件有视频流，就给目标文件创建视频流
        AVStream *dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        dest_video->time_base = src_video->time_base;
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

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (av_read_frame(in_fmt_ctx, packet) >= 0)
    { // 轮询数据包
        // 有的文件视频流没在第一路，需要调整到第一路，因为目标的视频流默认第一路
        if (packet->stream_index == video_index)
        { // 视频包
            packet->stream_index = 0;
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        }
        else
        { // 音频包
            packet->stream_index = video_index < 0 ? 0 : 1;
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        }
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success copy file.\n");

    av_packet_free(&packet);            // 释放数据包资源
    avio_close(out_fmt_ctx->pb);        // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx);  // 关闭音视频文件

    return 0;
}
