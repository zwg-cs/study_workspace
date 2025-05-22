#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    AVFormatContext *src_fmt_ctx = nullptr;

    // 打开文件
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_filename);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(src_fmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    const AVDictionaryEntry *tag = nullptr;
    // 遍历音视频文件的元数据
    while ((tag = av_dict_get(src_fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        av_log(nullptr, AV_LOG_INFO, "metadata %s = %s.\n", tag->key, tag->value);
    }
    avformat_close_input(&src_fmt_ctx);
    return 0;
}