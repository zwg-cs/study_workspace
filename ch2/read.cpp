/**
 * 打开关闭音视频
 */
#include <iostream>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    const char *filename = "/root/av/fuzhou.mp4";
    AVFormatContext *fmt_ctx = nullptr;

    // 从参数读入文件路径
    if (argc > 1)
    {
        filename = argv[1];
    }

    // 打开文件
    int ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Open file %s succeed.\n", filename);

    // 查找流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }

    av_log(nullptr, AV_LOG_INFO, "Find stream info succeed.\n");
    // 打印一些文件信息
    const AVInputFormat *iformat = fmt_ctx->iformat;
    av_log(nullptr, AV_LOG_INFO, "format name is %s.\n", iformat->name);
    av_log(nullptr, AV_LOG_INFO, "format long name is %s.\n", iformat->long_name);

    // 格式化输出信息
    av_dump_format(fmt_ctx, 0, filename, 0);
    // 关闭文件
    avformat_close_input(&fmt_ctx);
    return 0;
}