#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
// 把内存对齐定义为2个字节，可避免因BITMAPFILEHEADER出现4字节的对齐造成bmp位图头出错的问题。很重要，如果不设置就会导致bmp数据格式错误！！！
#pragma pack(2)

AVCodecContext *video_decode_ctx = nullptr; // 编码器实例
int pacekt_index = -1;                      // 数据包索引编号

// 定义位图文件头的结构
typedef struct BITMAPFILEHEADER
{
    uint16_t bfType;      // 文件类型
    uint32_t bfSize;      // 文件大小
    uint16_t bfReserved1; // 保留字段1
    uint16_t bfReserved2; // 保留字段2
    uint32_t bfOffBits;   // 从文件开头到位图数据的偏移量（单位字节）
} BITMAPFILEHEADER;

// 定义位图信息头的结构
typedef struct BITMAPINFOHEADER
{
    uint32_t biSize;          // 信息头的长度（单位字节）
    uint32_t biWidth;         // 位图宽度（单位像素）
    uint32_t biHeight;        // 位图高度（单位像素）
    uint16_t biPlanes;        // 位图的面数（单位像素）
    uint16_t biBitCount;      // 单个像素的位数（单位比特）
    uint32_t biCompression;   // 压缩说明
    uint32_t biSizeImage;     // 位图数据的大小（单位字节）
    uint32_t biXPelsPerMeter; // 水平打印分辨率
    uint32_t biYPelsPerMeter; // 垂直打印分辨率
    uint32_t biClrUsed;       // 位图使用的颜色掩码
    uint32_t biClrImportant;  // 重要的颜色个数
} BITMAPINFOHEADER;

/**
 * 把视频帧保存为bmp图片 save_index表示要把第几个视频帧保存为图片
 */
int save_bmp_file(AVFrame *frame, int save_index)
{
    av_log(nullptr, AV_LOG_INFO, "format = %d, width = %d, height = %d.\n", frame->format, frame->width, frame->height);
    char bmp_name[20] = {0};
    sprintf(bmp_name, "output_bgr_%03d.bmp", save_index);
    av_log(nullptr, AV_LOG_INFO, "target image file is %s.\n", bmp_name);
    FILE *fp = fopen(bmp_name, "wb"); // 以写方式打开文件
    if (!fp)
    {
        av_log(nullptr, AV_LOG_ERROR, "open bmp file %s fail.\n", bmp_name);
        return -1;
    }

    enum AVPixelFormat target_format = AV_PIX_FMT_BGR24; // bmp的像素格式为BGR24
    // 分配图像转换器的实例，指定来源和目标的format,width和height
    struct SwsContext *swsContext = sws_getContext(
        frame->width, frame->height, AV_PIX_FMT_YUV420P,
        frame->width, frame->height, target_format,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsContext)
    {
        av_log(nullptr, AV_LOG_ERROR, "swsContex is nullptr.\n");
        return -1;
    }
    // 分配缓冲区，用于存放转换后的图像数据
    int buffer_size = av_image_get_buffer_size(target_format, frame->width, frame->height, 1);
    unsigned char *out_buffer = (unsigned char *)av_malloc((size_t)buffer_size * sizeof(unsigned char));
    int linesize[4] = {3 * frame->width, 0, 0, 0};

    // 转换器开始处理图像数据，把YUV图像转为YUVJ图像
    sws_scale(swsContext, (const uint8_t *const *)frame->data, frame->linesize,
              0, frame->height, (uint8_t **)&out_buffer, linesize);

    // 释放转换器实例
    sws_freeContext(swsContext);

    BITMAPFILEHEADER bmp_header; // 声明bmp文件的头结构
    BITMAPINFOHEADER bmp_info;   // 声明bmp文件的信息结构
    unsigned int data_size = (frame->width * 3 + 3) / 4 * 4 * frame->height;
    // 文件标识填“BM”（即0x4D42）表示位图
    bmp_header.bfType = 0x4D42;
    // 保留字段1。填0即可
    bmp_header.bfReserved1 = 0;
    // 保留字段2。填0即可
    bmp_header.bfReserved2 = 0;
    // 从文件开头到位图数据的偏移量（单位字节）
    bmp_header.bfOffBits = sizeof(bmp_header) + sizeof(bmp_info);
    // 整个文件的大小（单位字节）
    bmp_header.bfSize = bmp_header.bfOffBits + data_size;
    // 信息头的长度（单位字节）
    bmp_info.biSize = sizeof(bmp_info);
    // 位图宽度（单位像素）
    bmp_info.biWidth = frame->width;
    // 位图高度（单位像素）。若为正，表示倒向的位图；若为负，表示正向的位图
    bmp_info.biHeight = frame->height;
    // 位图的面数。填1即可
    bmp_info.biPlanes = 1;
    // 单个像素的位数（单位比特）。RGB各一个字节，总共3个字节也就是24位
    bmp_info.biBitCount = 24;
    // 压缩说明。0(BI_RGB)表示不压缩
    bmp_info.biCompression = 0;
    // 位图数据的大小（单位字节）
    bmp_info.biSizeImage = data_size;
    // 水平打印分辨率（单位：像素/米）。填0即可
    bmp_info.biXPelsPerMeter = 0;
    // 垂直打印分辨率（单位：像素/米）。填0即可
    bmp_info.biYPelsPerMeter = 0;
    // 位图使用的颜色掩码。填0即可
    bmp_info.biClrUsed = 0;
    // 重要的颜色个数。都是普通颜色，填0即可
    bmp_info.biClrImportant = 0;
    fwrite(&bmp_header, sizeof(bmp_header), 1, fp); // 写入bmp文件头
    fwrite(&bmp_info, sizeof(bmp_info), 1, fp);     // 写入bmp信息头
    uint8_t tmp[frame->width * 3];                  // 临时数据
    for (int i = 0; i < frame->height / 2; i++)
    { // 把缓冲区的图像数据倒置过来
        memcpy(tmp, &(out_buffer[frame->width * i * 3]), frame->width * 3);
        memcpy(&(out_buffer[frame->width * i * 3]), &(out_buffer[frame->width * (frame->height - 1 - i) * 3]), frame->width * 3);
        memcpy(&(out_buffer[frame->width * (frame->height - 1 - i) * 3]), tmp, frame->width * 3);
    }
    fwrite(out_buffer, frame->width * frame->height * 3, 1, fp); // 写入图像数据
    fclose(fp);                                                  // 关闭文件
    return 0;
}

/**
 * 解码视频
 */
int decode_video(AVPacket *packet, AVFrame *frame, int save_index)
{ // 未解压的数据包发送给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "send pacekt to decoder occur error.\n");
        return -1;
    }
    while (1)
    {
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        pacekt_index++;
        if (pacekt_index < save_index) // 没有找到对应序号
        {
            return AVERROR(EAGAIN);
        }
        save_bmp_file(frame, save_index);
        break;
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    int save_index = 0; // 保存第几帧
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    if (argc > 2)
    {
        save_index = atoi(argv[2]);
    }

    // 打开源文件
    AVFormatContext *src_fmt_ctx = nullptr;
    int ret = avformat_open_input(&src_fmt_ctx, src_filename, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open src file %s.\n", src_filename);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open file %s.\n", src_filename);
    // 查找流信息
    ret = avformat_find_stream_info(src_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info in file %s.\n", src_filename);
        return -1;
    }

    // 查找视频流
    int video_stream_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index >= 0)
    {
        AVStream *video_stream = src_fmt_ctx->streams[video_stream_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 找到解码器
        const AVCodec *video_codec = avcodec_find_decoder(video_codec_id);
        if (!video_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't find video_codec.\n");
            return -1;
        }
        // 分配解码器实例
        video_decode_ctx = avcodec_alloc_context3(video_codec);
        if (!video_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "Alloc video_decode_ctx occur error.\n");
            return -1;
        }
        // 复制参数到解码器
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        // 打开解码器
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "open decode ctx occur error.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "not find video stream.\n");
    }

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc();    // 分配一个数据帧
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index)
        {
            ret = decode_video(packet, frame, save_index); // 解码视频包
            if (ret == 0)
            {
                break; // 只保存一幅图像
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(nullptr, AV_LOG_INFO, "Success save %d_index frame as jpg file.\n", save_index);
    // 释放资源
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(video_decode_ctx);
    avcodec_free_context(&video_decode_ctx);
    avformat_close_input(&src_fmt_ctx);
}