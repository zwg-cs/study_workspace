#include "videodecoder.h"
#include <thread>
#include <QtDebug>
// 构造函数
VideoDecoder::VideoDecoder(int play_type, const char* video_path, VideoCallBack* callback)
{
    qInfo() << "VideoDecoder play_type=" << play_type << '\n';
    m_play_type = play_type;
    m_video_path = video_path;
    m_callback = callback;
}

// 析构函数
VideoDecoder::~VideoDecoder(){}


// 开始解码
void VideoDecoder::start()
{
    is_stop = false;
    // 开启分线程播放视频，detach表示分离该线程
    std::thread([this](){
        int ret = playVideo();
        qInfo() << "play result: " << ret << '\n';
    }).detach();
}

// 停止解码
void VideoDecoder::stop()
{
    is_pause = false;
    is_stop = true;
}

// 暂停解码
void VideoDecoder::pause()
{
    is_pause = true;
}

// 恢复解码
void VideoDecoder::resum()
{
    is_pause = false;
}

// 播放视频
int VideoDecoder::playVideo()
{
    qInfo() << "playVideo " << m_video_path << '\n';
    AVFormatContext* in_fmt_ctx = avformat_alloc_context(); // 输入文件的封装器实例
    // 打开文件
    int ret = avformat_open_input(&in_fmt_ctx, m_video_path, nullptr, nullptr);
    if (ret < 0) {
        qCritical() << "Can't open file " << m_video_path << '\n';
        return -1;
    }
    qInfo() << "Success open input_file " << m_video_path << '\n';
    // 查找流
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }
    AVCodecContext* video_decode_ctx = nullptr;
    AVStream* src_video = nullptr;
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(video_index >= 0)
    {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        const AVCodec* video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            qCritical() << "video_codec not found" << '\n';
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx) {
            qCritical() << "video_decode_ctx is null" << '\n';
            return -1;
        }
        // 复制参数
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        // 打开解码器
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0) {
            qCritical() << "Can't open video_decode_ctx" << '\n';
            return -1;
        }
    }
    else
    {
        qCritical() << "Can't find video stream" << '\n';
        return -1;
    }

    int width = video_decode_ctx->width;
    int height = video_decode_ctx->height;
    enum AVPixelFormat rgb_format = AV_PIX_FMT_RGB24; // QImage支持RGB空间
    // 分配图像转换器的实例，并分别指定源和目标的宽度、高度和像素格式
    SwsContext* m_rgb_sws = sws_getContext(width, height, video_decode_ctx->pix_fmt,
                                           width, height, rgb_format,SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (m_rgb_sws == NULL) {
        qCritical() << "rgb swsContext is null" << '\n';
        return -1;
    }
    // 分配一个rgb缓存帧
    AVFrame *m_rgb_frame = av_frame_alloc();
    // 分配缓冲区，用于存放转换后的图像数据
    int rgb_size = av_image_get_buffer_size(rgb_format, width, height, 1);
    m_rgb_buffer = (uint8_t*)av_malloc((size_t)rgb_size * sizeof(uint8_t));
    // 将数据帧和缓冲区关联
    av_image_fill_arrays(m_rgb_frame->data, m_rgb_frame->linesize, m_rgb_buffer, rgb_format, width, height, 1);


    // OpenGL支持YUV空间
    enum AVPixelFormat yuv_format = AV_PIX_FMT_YUV420P;
    // 分配YUV转换器的实例
    SwsContext* m_yuv_sws = sws_getContext(width, height, video_decode_ctx->pix_fmt,
                                           width, height, yuv_format, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if(m_yuv_sws == nullptr)
    {
        qCritical() << "yuv swsContext is null" << '\n';
        return -1;
    }
    AVFrame* m_yuv_frame = av_frame_alloc();
    // 分配缓冲区，用于转换后的图像数据
    int yuv_size = av_image_get_buffer_size(yuv_format, width, height, 1);
    m_yuv_buffer = (uint8_t*)av_malloc((size_t)yuv_size * sizeof(uint8_t));
    // 关联数据帧和缓冲区
    av_image_fill_arrays(m_yuv_frame->data, m_yuv_frame->linesize, m_yuv_buffer, yuv_format, width, height, 1);

    // 帧率
    int fps = av_q2d(src_video->r_frame_rate);
    // 根据帧率计算每帧之间的间隔
    int interval = round(1000 / fps);
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while(av_read_frame(in_fmt_ctx, packet) >=0 )  // 轮询数据包
    {
        if(packet->stream_index == video_index)
        {
            ret = avcodec_send_packet(video_decode_ctx, packet);
            if(ret == 0)
            {
                ret = avcodec_receive_frame(video_decode_ctx, frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }else if(ret < 0)
                {
                    qCritical() << "decode frame occur error " << ret << '\n';
                    continue;
                }
                // 使用rgb转换器将视频帧转换成RGB图像
                sws_scale(m_rgb_sws, (const uint8_t* const*)frame->data, frame->linesize,
                          0, frame->height,m_rgb_frame->data, m_rgb_frame->linesize);
                // 使用YUV转换器将视频转换成yuv图像
                sws_scale(m_yuv_sws, (const uint8_t* const *)frame->data, frame->linesize,
                          0, frame->height, m_yuv_frame->data, m_yuv_frame->linesize);
                // 显示图像
                displayImage(width, height);
                qInfo() << "interval=" << interval;
                _sleep(interval); // 延迟若干时间
                while(is_pause) // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
                {
                    _sleep(20); // 休眠20毫秒
                    if(is_stop)  // 暂停期间如果停止播放，就结束暂停
                    {
                        break; // 如果停止播放，就跳出循环结束解码
                    }
                }

                if(is_stop)
                {
                    break;
                }
            }else
            {
                qCritical() << "send packet occur error " << ret << '\n';
                return -1;
            }
        }
        av_packet_unref(packet);  // 清空数据包
    }

    return 0;
}

// 显示图像
void VideoDecoder::displayImage(int width, int height)
{
    VideoFramePtr videoFrame = std::make_shared<VideoFrame>();
    VideoFrame* ptr = videoFrame.get();
    ptr->initBuffer(width, height); // 初始化图像缓存
    if(m_play_type == 0)  // 使用QImage方式
    {
        ptr->setRGBbuf(m_rgb_buffer);  // 设置rgb缓存数据
    }else  // 使用OpenGL方式
    {
        ptr->setYUVbuf(m_yuv_buffer); // 设置YUV缓存数据
    }
    // 通知回调接口展示该视频帧的图像
    m_callback->onDisplayVideo(videoFrame);
}
