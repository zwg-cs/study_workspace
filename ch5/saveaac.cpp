#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

AVFormatContext *in_fmt_ctx = nullptr;      // 输入文件的封装器实例
AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
int audio_index = -1;                       // 音频流的索引
AVCodecContext *audio_encode_ctx = nullptr; // AAC编码器的实例

// 获取ADTS头部
void get_adts_header(AVCodecContext *codec_ctx, char *adts_header, int aac_length)
{
    uint8_t freq_index = 0; // 采样频率对应的索引
    switch (codec_ctx->sample_rate)
    {
    case 96000:
        freq_index = 0;
        break;
    case 88200:
        freq_index = 1;
        break;
    case 64000:
        freq_index = 2;
        break;
    case 48000:
        freq_index = 3;
        break;
    case 44100:
        freq_index = 4;
        break;
    case 32000:
        freq_index = 5;
        break;
    case 24000:
        freq_index = 6;
        break;
    case 22050:
        freq_index = 7;
        break;
    case 16000:
        freq_index = 8;
        break;
    case 12000:
        freq_index = 9;
        break;
    case 11025:
        freq_index = 10;
        break;
    case 8000:
        freq_index = 11;
        break;
    case 7350:
        freq_index = 12;
        break;
    default:
        freq_index = 4;
        break;
    }
    uint8_t nb_channels = codec_ctx->ch_layout.nb_channels; // 声道数量
    uint32_t frame_length = aac_length + 7;                 // adts头部的长度为7个字节
    adts_header[0] = 0xFF;                                  // 二进制值固定为 1111 1111
    // 二进制值为 1111 0001。其中前四位固定填1；第五位填0表示MPEG-4，填1表示MPEG-2；六七两位固定填0；第八位填0表示adts头长度9字节，填1表示adts头长度7字节
    adts_header[1] = 0xF1;
    // 二进制前两位表示AAC音频规格；中间四位表示采样率的索引；第七位填0即可；第八位填声道数量除以四的商
    adts_header[2] = ((codec_ctx->profile) << 6) + (freq_index << 2) + (nb_channels >> 2);
    // 二进制前两位填声道数量除以四的余数；中间四位填0即可；后面两位填frame_length的前2位（frame_length总长13位）
    adts_header[3] = (((nb_channels & 3) << 6) + (frame_length >> 11));
    // 二进制填frame_length的第3位到第10位，“& 0x7FF”表示先取后面11位（掐掉前两位），“>> 3”表示再截掉末尾的3位，结果就取到了中间的8位
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    // 二进制前3位填frame_length的后3位，“& 7”表示取后3位（7的二进制是111）；后5位填1
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    // 二进制前6位填1；后2位填0，表示一个ADTS帧包含一个AAC帧，就是每帧ADTS包含的AAC帧数量减1
    adts_header[6] = 0xFC;
    return;
}

// 初始化AAC编码器
int init_audio_encoder(void)
{
    const AVCodec *audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audio_codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio_codec is nullptr.\n");
        return -1;
    }
    const enum AVSampleFormat *p = audio_codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE)
    { // 使用AV_SAMPLE_FMT_NONE作为结束符
        av_log(nullptr, AV_LOG_INFO, "audio_codec support format %d\n", *p);
        p++;
    }
    audio_encode_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_encode_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "audio_encode_ctx is nullptr");
        return -1;
    }
    // 设置参数
    audio_encode_ctx->sample_fmt = audio_decode_ctx->sample_fmt;   // 采样格式
    audio_encode_ctx->ch_layout = audio_decode_ctx->ch_layout;     // 声道布局
    audio_encode_ctx->bit_rate = audio_decode_ctx->bit_rate;       // 比特率
    audio_encode_ctx->sample_rate = audio_decode_ctx->sample_rate; // 采样率，单位次每秒
    audio_encode_ctx->profile = audio_decode_ctx->profile;         // AAC规格
    // 打开编码器
    int ret = avcodec_open2(audio_encode_ctx, audio_codec, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
        return -1;
    }
    return 0;
}

// 打开输入文件
int open_input_file(const char *src_name)
{
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        AVStream *src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec *)avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr); // 打开解码器的实例
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    return 0;
}

int save_aac_file(FILE *fp_out, AVFrame *frame)
{
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    int ret = avcodec_send_frame(audio_encode_ctx, frame);
    while (ret == 0)
    {
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        char head[7] = {0};
        get_adts_header(audio_encode_ctx, head, packet->size);
        fwrite(head, 1, sizeof(head), fp_out); // 写入ADTS头部
        // 把编码后的AAC数据包写入文件
        fwrite(packet->data, 1, packet->size, fp_out);
    }
    av_packet_free(&packet); // 释放数据包资源
    return 0;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    if (argc > 1)
    {
        src_filename = argv[1];
    }
    const char *aac_name = "output_saveaac.aac";
    // 打开输入文件
    if (open_input_file(src_filename) < 0)
    {
        return -1;
    }
    // 初始化AAC编码器的实例
    if (init_audio_encoder() < 0)
    {
        return -1;
    }
    FILE *fp_out = fopen(aac_name, "wb"); // 以写方式打开文件
    if (!fp_out)
    {
        av_log(nullptr, AV_LOG_ERROR, "open file %s fail.\n", aac_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "target audio file is %s\n", aac_name);
    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc();    // 分配一个数据帧

    while (av_read_frame(in_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == audio_index)
        {
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0)
            {
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    continue;
                }
                else if (ret < 0)
                {
                    av_log(nullptr, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                save_aac_file(fp_out, frame); // 把音频帧保存到AAC文件
            }
            else
            {
                av_log(nullptr, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet); // 清空数据包
    }
    save_aac_file(fp_out, nullptr); // 传入一个空帧
    av_log(nullptr, AV_LOG_INFO, "Success save audio frame as aac file.\n");
    fclose(fp_out); // 关闭文件

    av_frame_free(&frame);                   // 释放数据帧资源
    av_packet_free(&packet);                 // 释放数据包资源
    avcodec_close(audio_decode_ctx);         // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avcodec_close(audio_encode_ctx);         // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    avformat_close_input(&in_fmt_ctx);       // 关闭音视频文件
}