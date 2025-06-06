#include <iostream>
#include <SDL.h>

#undef main

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

int main(int argc, char **argv)
{

    const char *src_name = "rtsp://127.0.0.1:8554/stream";
    if (argc > 1)
    {
        src_name = argv[1];
    }
    // 分配输入文件的封装器实例（注意从网络拉流要调用avformat_alloc_context）
    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    // 打开媒体文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
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
    AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0)
    {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *)avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_ERROR, "can not initialize SDL\n");
        return -1;
    }
    // 创建SDL窗口
    SDL_Window *window = SDL_CreateWindow("Video Player",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          video_decode_ctx->width, video_decode_ctx->height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        av_log(NULL, AV_LOG_ERROR, "can not create window\n");
        return -1;
    }
    // 创建SDL渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
    {
        av_log(NULL, AV_LOG_ERROR, "can not create renderer\n");
        return -1;
    }
    // 创建SDL纹理
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
                                             SDL_TEXTUREACCESS_STREAMING, video_decode_ctx->width, video_decode_ctx->height);
    // 设置视频画面的SDL渲染区域（左上角横坐标、左上角纵坐标、宽度、高度）
    SDL_Rect rect = {0, 0, video_decode_ctx->width, video_decode_ctx->height};
    SDL_Event event;                               // 声明SDL事件
    int fps = av_q2d(src_video->r_frame_rate);     // 帧率
    int interval = round(1000 / fps);              // 根据帧率计算每帧之间的播放间隔
    AVPacket *packet = av_packet_alloc();          // 分配一个数据包
    AVFrame *frame = av_frame_alloc();             // 分配一个数据帧
    bool got_first_key_frame = false;              // 标记I帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        // 因为本地先推流后取流，可能会错过I帧，导致解码P帧和B帧的时候参考帧不可用，这里遇到第一个I帧再开始解码
        if (!(packet->flags & AV_PKT_FLAG_KEY) && !got_first_key_frame) // 如果当前包不是关键帧，且我们还没收到过关键帧
        {
            continue;
        }
        else
        {
            got_first_key_frame = true;
        }

        if (packet->stream_index == video_index)
        { // 视频包需要解码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(video_decode_ctx, packet);
            if (ret == 0)
            {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(video_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                // 刷新YUV纹理
                SDL_UpdateYUVTexture(texture, NULL,
                                     frame->data[0], frame->linesize[0],
                                     frame->data[1], frame->linesize[1],
                                     frame->data[2], frame->linesize[2]);
                // SDL_RenderClear(renderer); // 清空渲染器
                SDL_RenderCopy(renderer, texture, NULL, &rect); // 将纹理复制到渲染器
                SDL_RenderPresent(renderer);                    // 渲染器开始渲染
                SDL_Delay(interval);                            // 延迟若干时间，单位毫秒
                SDL_PollEvent(&event);                          // 轮询SDL事件
                switch (event.type)
                {
                case SDL_QUIT:   // 如果命令关闭窗口（单击了窗口右上角的叉号）
                    goto __QUIT; // 这里用goto不用break
                default:
                    break;
                }
            }
            else
            {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(NULL, AV_LOG_INFO, "Success pull video stream.\n");

__QUIT:
    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&packet);                 // 释放数据包资源
    avcodec_close(video_decode_ctx);         // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx);       // 关闭音视频文件

    SDL_DestroyTexture(texture);   // 销毁SDL纹理
    SDL_DestroyRenderer(renderer); // 销毁SDL渲染器
    SDL_DestroyWindow(window);     // 销毁SDL窗口
    SDL_Quit();                    // 退出SDL
    av_log(NULL, AV_LOG_INFO, "Quit SDL.\n");
    return 0;
}