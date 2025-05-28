#include <iostream>
#include <list>
#include <SDL.h>

#undef main

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#define MAX_AUDIO_FRAME_SIZE 8096   // 一帧音频最大长度（样本数），该值不能太小
int audio_len = 0;                  // 一帧PCM音频的数据长度
unsigned char *audio_pos = nullptr; // 当前读取的位置

AVFormatContext *in_fmt_ctx = nullptr;      // 输入文件实例
AVCodecContext *video_decode_ctx = nullptr; // 视频解码器
AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器
AVStream *src_video = nullptr;              // 输入视频流
AVStream *src_audio = nullptr;              // 输入音频流
int video_index = -1;                       // 输入视频索引
int audio_index = -1;                       // 输入音频索引
SwrContext *swr_ctx = nullptr;              // 采样器实例

SDL_Window *window;     // 声明窗口
SDL_Renderer *renderer; // 声明sdl渲染器
SDL_Texture *texture;   // 声明sdl纹理
SDL_Rect rect;          // 声明渲染区域
SDL_Event event;        // 什么SDL事件

AVFrame *video_frame = nullptr; // 声明一个视频帧
int out_buffer_size;            // 缓冲区大小
unsigned char *out_buff;        // 缓冲区的位置

int interval;                     // 视频帧之间的播放间隔
int play_video = 0;               // 是否正在播放视频
int is_close = 0;                 // 是否关闭窗口
int has_audio = 0;                // 是否拥有音频流
double audio_time = 0;            // 音频时钟，当前音频包对应的时间值
double video_time = 0;            // 视频时钟，当前视频包对应的时间值
SDL_mutex *list_lock = nullptr;   // 声明队列锁，防止线程同时操作包队列
SDL_mutex *frame_lock = nullptr;  // 声明帧锁，防止线程同时操作视频帧
SDL_Thread *sdl_thread = nullptr; // 声明一个线程
std::list<AVPacket> packet_list;  // 存放视频包的队列

// 回调函数
void fill_audio(void *para, uint8_t *stream, int len)
{
    SDL_memset(stream, 0, len); // 将缓冲区清零
    if (audio_len == 0)
    {
        return;
    }
    len = (len > audio_len ? audio_len : len);
    // 将音频混合到缓冲区
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

int open_input_file(const char *src_name)
{
    // 打开文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0)
    {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 从流中复制参数到解码器
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        // 打开解码器
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    }
    // 查找音频流
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(NULL, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        // 打开编码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    return 0;
}

// 分线程处理任务
int thread_work(void *arg)
{
    while (1)
    {
        if (is_close) // 关闭窗口
        {
            break;
        }
        SDL_LockMutex(list_lock); // 对队列加锁
        if (packet_list.empty())
        {
            SDL_UnlockMutex(list_lock); // 对队列解锁
            SDL_Delay(10);
            continue;
        }
        AVPacket packet = packet_list.front(); // 从队列头部取视频包
        packet_list.pop_front();               // 队列头部出列
        SDL_UnlockMutex(list_lock);            // 对队列锁解锁
        if (packet.dts != AV_NOPTS_VALUE)      // 保存视频时钟
        {
            video_time = av_q2d(src_video->time_base) * packet.dts;
        }
        // 分配一个数据帧
        AVFrame *frame = av_frame_alloc();
        // 发送压缩数据到解码器
        int ret = avcodec_send_packet(video_decode_ctx, &packet);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            continue;
        }
        while (1)
        {
            // 从解码器实例获取还原后的数据帧
            ret = avcodec_receive_frame(video_decode_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                break;
            }
            SDL_LockMutex(frame_lock);           // 对帧锁加锁
                                                 // 以下深度复制AVFrame（完整复制，不是简单引用）
            video_frame->format = frame->format; // 像素格式（视频）或者采样格式（音频）
            video_frame->width = frame->width;   // 视频宽度
            video_frame->height = frame->height; // 视频高度
            // video_frame->ch_layout = frame->ch_layout; // 声道布局，音频需要
            // video_frame->nb_samples = frame->nb_samples; // 采样数，音频需要
            av_frame_get_buffer(video_frame, 32);    // 重新分配数据帧的缓冲区（储存视频或音频要用）
            av_frame_copy(video_frame, frame);       // 复制数据帧的缓冲区数据
            av_frame_copy_props(video_frame, frame); // 复制数据帧的元数据
            play_video = 1;                          // 可以播放视频了
            SDL_UnlockMutex(frame_lock);             // 对帧锁解锁
            if (has_audio)                           // 存在音频流
            {
                // 如果视频包太早被解码出来，就要等待同时刻的音频时钟
                while (video_time > audio_time)
                {
                    SDL_Delay(5); // 延迟若干时间，单位毫秒
                }
            }
        }
        av_packet_unref(&packet); // 清除数据包
    }
    return 0;
}

// 准备SDL视频相关资源
int prepare_video(void)
{
    int fps = av_q2d(src_video->r_frame_rate); // 帧率
    interval = 1000 / fps;                     // 播放间隔

    // 初始化SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_ERROR, "can not initialize SDL\n");
        return -1;
    }
    // 创建SDL窗口
    window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              video_decode_ctx->width, video_decode_ctx->height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        av_log(NULL, AV_LOG_ERROR, "can not create window\n");
        return -1;
    }

    // 创建SDL渲染器
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
    {
        av_log(NULL, AV_LOG_ERROR, "can not create renderer\n");
        return -1;
    }
    // 创建SDL纹理
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                video_decode_ctx->width, video_decode_ctx->height);

    // 指定渲染区域
    rect = {0, 0, video_decode_ctx->width, video_decode_ctx->height};
    // 锁
    list_lock = SDL_CreateMutex();  // 用于调度包队列的锁
    frame_lock = SDL_CreateMutex(); // 用于视频帧的锁
    // 创建SDL线程，指定处理函数，返回线程编号
    sdl_thread = SDL_CreateThread(thread_work, "thread_work", nullptr);
    if (!sdl_thread)
    {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    return 0;
}

// 准备SDL音频相关资源
int prepare_audio(void)
{
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;    // 输出的声道布局
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;      // 输出的采样格式
    int out_sample_rate = 44100;                                 // 输出的采样率
    int out_nb_samples = audio_decode_ctx->frame_size;           // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels;                // 输出的声道数量
    int ret = swr_alloc_set_opts2(&swr_ctx,                      // 采样器实例
                                  &out_ch_layout,                // 输出通道布局
                                  out_sample_fmt,                // 输出的采样格式
                                  out_sample_rate,               // 输出的采样率
                                  &audio_decode_ctx->ch_layout,  // 输入声道布局
                                  audio_decode_ctx->sample_fmt,  // 输入采样格式
                                  audio_decode_ctx->sample_rate, // 输入采样率
                                  0, nullptr);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "swr_alloc_set_opts2 error %d\n", ret);
        return -1;
    }
    // 初始化采样器实例
    ret = swr_init(swr_ctx);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "swr_init error %d\n", ret);
        return -1;
    }
    // 计算输出缓冲区大小
    out_buffer_size = av_samples_get_buffer_size(nullptr, out_channels, out_nb_samples, out_sample_fmt, 1);
    // 分配缓冲区的空间
    out_buff = (unsigned char *)av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);

    SDL_AudioSpec audio_spec;                    // 声明SDL音频参数
    audio_spec.freq = out_sample_rate;           // 采样频率
    audio_spec.format = AUDIO_S16SYS;            // 采样格式
    audio_spec.channels = out_channels;          // 声道数量
    audio_spec.silence = 0;                      // 是否静音
    audio_spec.samples = out_nb_samples;         // 采样数量
    audio_spec.callback = fill_audio;            // 回调函数名称
    audio_spec.userdata = nullptr;               // 回调函数额外信息
    if (SDL_OpenAudio(&audio_spec, nullptr) < 0) // 打开扬声器
    {
        av_log(NULL, AV_LOG_ERROR, "open audio occur error\n");
        return -1;
    }
    SDL_PauseAudio(0); // 播放/暂停音频。参数为0表示播放，为1表示暂停
    return 0;
}

// 释放资源
void release(void)
{
    if (video_index >= 0)
    {
        av_frame_free(&video_frame);                // 释放数据帧
        int finish_status;                          // 线程结束标志
        SDL_WaitThread(sdl_thread, &finish_status); // 等待线程结束，结束标志在status字段返回
        SDL_DestroyMutex(list_lock);                // 销毁队列锁
        SDL_DestroyMutex(frame_lock);               // 销毁帧锁
        av_log(NULL, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
        avcodec_close(video_decode_ctx);         // 关闭视频解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
        SDL_DestroyTexture(texture);             // 销毁SDL纹理
        SDL_DestroyRenderer(renderer);           // 销毁SDL渲染器
        SDL_DestroyWindow(window);               // 销毁SDL窗口
    }

    if (audio_index >= 0)
    {
        avcodec_close(audio_decode_ctx);         // 关闭音频解码器的实例
        avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
        swr_free(&swr_ctx);                      // 释放采样器实例
        SDL_CloseAudio();                        // 关闭扬声器
    }

    SDL_Quit();                        // 退出SDL
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}
int main(int argc, char **argv)
{
    const char *src_name = "E:\\MyProjects\\av\\fuzhou.mp4";
    if (argc > 1)
    {
        src_name = argv[1];
    }

    // 打开输入文件
    if (open_input_file(src_name) < 0)
    {
        return -1;
    }

    // 准备SDL视频相关资源
    if (video_index >= 0)
    {
        if (prepare_video() < 0)
        {
            return -1;
        }
    }

    // 准备SDL音频相关资源
    if (audio_index >= 0)
    {
        if (prepare_audio() < 0)
        {
            return -1;
        }
    }

    int ret;
    AVPacket *packet = av_packet_alloc();          // 分配数据包
    AVFrame *audio_frame = av_frame_alloc();       // 分配数据帧
    video_frame = av_frame_alloc();                // 分配数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) // 轮询数据包
    {
        if (packet->stream_index == audio_index) // 如果是音频包
        {
            // 音频数据包送入解码器
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0)
            {
                ret = avcodec_receive_frame(audio_decode_ctx, audio_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                // 对音频重采样
                swr_convert(swr_ctx,
                            &out_buff, MAX_AUDIO_FRAME_SIZE,                             // 输出数据的内容的数据大小
                            (const uint8_t **)audio_frame->data, audio_frame->nb_samples // 输入数据的内容和数大小
                );
                while (audio_len > 0) // 如果还没播放完，就等待1ms
                {
                    SDL_Delay(1); // 延迟若干时间，单位毫秒
                }
                audio_pos = (unsigned char *)out_buff; // 把音频数据同步到缓冲区位置
                audio_len = out_buffer_size;           // 缓冲区打下
                has_audio = 1;                         // 找到音频
                if (packet->pts != AV_NOPTS_VALUE)     // 保存音频时钟
                {
                    audio_time = av_q2d(src_audio->time_base) * packet->pts;
                }
            }
            else
            {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        else if (packet->stream_index == video_index) // 如果是视频包
        {
            SDL_LockMutex(list_lock);       // 对队列锁加锁
            packet_list.push_back(*packet); // 将视频包加入队列
            SDL_UnlockMutex(list_lock);     // 对队列锁解锁
            if (!has_audio)                 // 不存在音频流
            {
                SDL_Delay(interval); // 延迟若干时间，单位毫秒
            }
        }
        if (play_video) // 运行播放视频
        {
            SDL_LockMutex(frame_lock); // 对帧加锁
            play_video = 0;
            // 刷新YUV纹理
            SDL_UpdateYUVTexture(texture, nullptr,
                                 video_frame->data[0], video_frame->linesize[0],
                                 video_frame->data[1], video_frame->linesize[1],
                                 video_frame->data[2], video_frame->linesize[2]);
            // 将纹理复制到渲染器
            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            // 渲染器开始渲染
            SDL_RenderPresent(renderer);
            SDL_UnlockMutex(frame_lock); // 对帧解锁
            SDL_PollEvent(&event);
            switch (event.type)
            {
            case SDL_QUIT:
                goto __QUIT;
            default:
                break;
            }
        }

        // av_packet_unref(packet); // 清除数据包（注意这里不能清除，因为从队列取出后已经清除）
    }
    av_log(NULL, AV_LOG_INFO, "Success play video file with audio stream.\n");
__QUIT:
    av_log(NULL, AV_LOG_INFO, "Close window.\n");
    is_close = 1;
    av_frame_free(&audio_frame);
    release(); // 释放资源
    av_log(NULL, AV_LOG_INFO, "Quit SDL.\n");
    return 0;
}