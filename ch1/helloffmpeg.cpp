#include <iostream>
extern "C"
{
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{
    // 默认只打印AV_LOG_INFO及其以上的日志，调用av_log_set_level设置日志级别，打印该级别及其以上级别的日志
    av_log_set_level(AV_LOG_TRACE);
    av_log(nullptr, AV_LOG_FATAL, "致命错误\n");
    av_log(nullptr, AV_LOG_ERROR, "错误信息\n");
    av_log(nullptr, AV_LOG_WARNING, "警告信息\n");
    av_log(nullptr, AV_LOG_INFO, "标准信息\n");
    av_log(nullptr, AV_LOG_VERBOSE, "详细信息\n");
    av_log(nullptr, AV_LOG_DEBUG, "调试信息\n");
    av_log(nullptr, AV_LOG_TRACE, "及其冗长的调试信息\n");
    return 0;
}