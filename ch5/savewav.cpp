#include <iostream>
#include <sys/stat.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器实例

typedef struct WAVHeader
{
    char riffCkID[4];      // 固定填"RIFF"
    int32_t riffCkSize;    // RIFF块大小。文件总长减去riffCkID和riffCkSize两个字段的长度
    char format[4];        // 固定填"WAVE"
    char fmtCkID[4];       // 固定填"fmt "
    int32_t fmtCkSize;     // 格式块大小，从audioFormat到bitsPerSample各字段长度之和，为16
    int16_t audioFormat;   // 音频格式。1表示整数，3表示浮点数
    int16_t channels;      // 声道数量
    int32_t sampleRate;    // 采样频率，单位赫兹
    int32_t byteRate;      // 数据传输速率，单位字节每秒
    int16_t blockAlign;    // 采样大小，即每个样本占用的字节数
    int16_t bitsPerSample; // 每个样本占用的比特数量，即采样大小乘以8（样本大小以字节为单位）
    char dataCkID[4];      // 固定填"data"
    int32_t dataCkSize;    // 数据块大小。文件总长减去WAV头的长度
} WAVHeader;

// 把PCM文件转换为WAV文件
int save_wav_file(const char *pcm_filename)
{
    struct stat size;                   // 保存文件信息的结构
    if (stat(pcm_filename, &size) != 0) // 获取文件信息
    {
        av_log(nullptr, AV_LOG_ERROR, "file %s is not exists\n", pcm_filename);
        return -1;
    }
    // 以读的方式打开pcm文件
    FILE *fp_pcm = fopen(pcm_filename, "rb");
    if (!fp_pcm)
    {
        av_log(nullptr, AV_LOG_ERROR, "open file %s fail.\n", pcm_filename);
        return -1;
    }
    const char *wav_name = "output_savewav.wav";
    // 读写的方式打开wav文件
    FILE *fp_wav = fopen(wav_name, "wb");
    if (!fp_wav)
    {
        av_log(nullptr, AV_LOG_ERROR, "open file %s fail.\n", wav_name);
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO, "target audio file is %s\n", wav_name);
    int pcmDataSize = size.st_size; // pcm文件大小
    av_log(nullptr, AV_LOG_INFO, "pcmDataSize=%d\n", pcmDataSize);
    WAVHeader wavHeader; // wav文件头结构
    sprintf(wavHeader.riffCkID, "RIFF");
    // 设置 RIFF chunk size，RIFF chunk size 不包含 RIFF Chunk ID 和 RIFF Chunk Size的大小，所以用 PCM 数据大小加 RIFF 头信息大小减去 RIFF Chunk ID 和 RIFF Chunk Size的大小
    wavHeader.riffCkSize = (pcmDataSize + sizeof(WAVHeader) - 4 - 4);
    sprintf(wavHeader.format, "WAVE");
    sprintf(wavHeader.fmtCkID, "fmt ");
    wavHeader.fmtCkSize = 16;
    // 设置音频格式。1为整数，3为浮点数（含双精度数）
    if (audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLT || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBLP || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBL)
    {
        wavHeader.audioFormat = 3;
    }
    else
    {
        wavHeader.audioFormat = 1;
    }
    wavHeader.channels = audio_decode_ctx->ch_layout.nb_channels; // 声道数量
    wavHeader.sampleRate = audio_decode_ctx->sample_rate;         // 采样频率
    wavHeader.bitsPerSample = 8 * av_get_bytes_per_sample(audio_decode_ctx->sample_fmt);
    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) >> 3;
    //    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) / 8;
    wavHeader.byteRate = wavHeader.sampleRate * wavHeader.blockAlign;
    sprintf(wavHeader.dataCkID, "data");
    // 设置数据块大小，即实际PCM数据的长度，单位字节
    wavHeader.dataCkSize = pcmDataSize;
    // 向wav文件写入wav文件头信息
    fwrite((const char *)&wavHeader, 1, sizeof(WAVHeader), fp_wav);
    const int per_size = 1024;                          // 每次读取的大小
    uint8_t *per_buff = (uint8_t *)av_malloc(per_size); // 读取缓冲区
    int len = 0;
    // 循环读取PCM文件中的音频数据
    while ((len = fread(per_buff, 1, per_size, fp_pcm)) > 0)
    {
        fwrite(per_buff, 1, per_size, fp_wav); // 依次写入每个PCM数据
    }
    fclose(fp_pcm); // 关闭pcm文件
    fclose(fp_wav); // 关闭wav文件
    return 0;
}

int main(int argc, char **argv)
{
    const char *src_filename = "/root/av/fuzhou.mp4";
    const char *pcm_filename = "savepcm.pcm";

    // 输入文件封装实例
    AVFormatContext *src_fmt_ctx = nullptr;
    // 分配实例
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
        av_log(nullptr, AV_LOG_ERROR, "Can't find stream info.\n");
        return -1;
    }
    // 获取音频流索引
    int audio_index = av_find_best_stream(src_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0)
    {
        // 获取音频流
        AVStream *audio_stream = src_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 查找解码器
        const AVCodec *audio_codec = avcodec_find_decoder(audio_codec_id);
        if (!audio_codec)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio_codec is nullptr.\n");
            return -1;
        }
        // 获取解码器实例
        audio_decode_ctx = avcodec_alloc_context3(audio_codec);
        if (!audio_decode_ctx)
        {
            av_log(nullptr, AV_LOG_ERROR, "audio decoder is nullptr.\n");
            return -1;
        }
        // 从音频流中复制参数到解码器
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        av_log(nullptr, AV_LOG_INFO, "width = %d, height = %d.\n", audio_decode_ctx->width, audio_decode_ctx->height);
        av_log(nullptr, AV_LOG_INFO, "sample_fmt = %d, nb_samples = %d, nb_channels = %d.\n",
               audio_decode_ctx->sample_fmt,
               audio_decode_ctx->frame_size,
               audio_decode_ctx->ch_layout.nb_channels);
        av_log(nullptr, AV_LOG_INFO, "format_name = %s, is_planar = %d, data_size = %d.\n",
               av_get_sample_fmt_name(audio_decode_ctx->sample_fmt),
               av_sample_fmt_is_planar(audio_decode_ctx->sample_fmt),
               av_get_bytes_per_sample(audio_decode_ctx->sample_fmt));
        // 打开解码器
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr);
        if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "open audio decoder occur error.\n");
            return -1;
        }
    }
    else
    {
        av_log(nullptr, AV_LOG_ERROR, "Not find audio stream.\n");
        return -1;
    }

    // 打开pcm文件
    FILE *fp_out = fopen(pcm_filename, "wb");
    if (!fp_out)
    {
        av_log(nullptr, AV_LOG_ERROR, "open file %s fail.\n", pcm_filename);
        return -1;
    }

    av_log(nullptr, AV_LOG_INFO, "target audio info is %s.\n", pcm_filename);

    int i = 0;
    int j = 0;
    int data_size = 0;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    // 开始轮询数据
    while (av_read_frame(src_fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == audio_index)
        {
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret >= 0)
            {
                while (1)
                {
                    ret = avcodec_receive_frame(audio_decode_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    {
                        break;
                    }
                    else if (ret < 0)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "receive frame occur error %d.\n", ret);
                        break;
                    }
                    // 成功获取解码后的数据帧，将数据帧写入文件
                    if (av_sample_fmt_is_planar((enum AVSampleFormat)frame->format))
                    {
                        // 如果是平面模式，需要改变成交错模式
                        // 计算每个音频样本的字节大小（比如16位PCM是2字节，32位浮点是4字节）
                        data_size = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
                        i = 0;
                        while (i < frame->nb_samples) // 外层循环遍历每个样本（i从0到nb_samples-1）
                        {
                            j = 0;
                            while (j < frame->ch_layout.nb_channels) // 内层循环遍历每个声道（j从0到nb_channels-1）
                            {
                                fwrite(frame->data[j] + data_size * i, 1, data_size, fp_out);
                                // frame->data[j]：第j个声道的起始地址
                                // data_size * i：第i个样本的偏移量
                                // 写入大小为data_size的样本数据到文件fp_out
                                ++j;
                            }
                            ++i;
                        }
                    }
                    else
                    {
                        // 非平面模式，直接写入
                        fwrite(frame->extended_data[0], 1, frame->linesize[0], fp_out);
                    }
                    av_frame_unref(frame); // 清楚数据帧
                }
            }
            else
            {
                av_log(nullptr, AV_LOG_ERROR, "decode packet occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(packet);
    }

    fclose(fp_out); // 关闭文件
    av_log(nullptr, AV_LOG_INFO, "Success save audio frame as pcm file.\n");
    // 把PCM文件转换为WAV文件
    save_wav_file(pcm_filename);
    av_log(nullptr, AV_LOG_INFO, "Success save audio frame as wav file.\n");

    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_close(audio_decode_ctx);
    avcodec_free_context(&audio_decode_ctx);
    avformat_close_input(&src_fmt_ctx);
    return 0;
}