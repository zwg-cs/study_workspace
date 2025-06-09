# ffmpeg学习总结
这是我的ffmpeg学习笔记，主要内容是跟着参考书《FFmpeg开发实践 从零基础到短视频上线》做的
我当前使用的ffmpeg版本是6.1.2，上面所有c++代码在linux和windows上测试通过。


## ch1 安装配置环境
该章节主要关于在linux和windows行编译配置ffmpeg。涉及一些基本的库，比如nasm，x264，x265，在linux上配置环境变量。
在windows上使用visual studio,msys2等工具编译ffmpeg。配置好环境后能成功运行ffmpeg命令。


## ch2 基础开发
1. 编译avs2（国家数字音视频标准AVS）编解码库源码，并在ffmpeg中启用avs2编解码器。
验证avs2编解码功能是否正常
```bash
ffmpeg -i input.mp4 -r 25 -acodec aac -vcodec avs2 output.ts
```
2. AVFormatContext封装器实例    
AVFormatContext的主要函数（读文件）说明如下：    
* avformat_open_input：打开音视频文件。
* avformat_find_stream_info：查找音视频文件中的流信息。
* av_find_best_stream：寻找指定类型的数据流。
* avformat_close_input：关闭音视频文件。   
AVFormatContext的主要函数（写文件）说明如下：    
* avformat_alloc_output_context2：分配待输出的音视频文件封装实例。
* avio_open：打开音视频实例的输出流。
* avformat_new_stream：给音视频文件创建指定编码器的数据流。
* avio_close：关闭音视频实例的输出流。
* avformat_free_context：释放音视频文件的封装实例。

3. AVcodec编解码器
* avcodec_find_decoder_by_name：根据编解码器名称查找编解码器。
* avcodec_find_encoder_by_name：根据编解码器名称查找编码器。
* avcodec_find_decoder：根据编解码器ID查找编解码器。
* avcodec_find_encoder：根据编解码器ID查找编码器。
4. AVcodecContext编解码器实例
* AVCodecContext的主要函数说明如下：
* avcodec_alloc_context3：获取指定编解码器的实例。
* avcodec_open2：打开编解码器的实例。
* avcodec_close：关闭编解码器的实例。
* avcodec_free_context：释放编解码器的实例

5. AVFrame音视频帧 AVPacket音视频包 AVStream音视频流
* 数据流AVStream，分为音频流、视频流、字幕流等类型。
* 数据包AVPacket，数据流内部由一个个数据包组成，数据包存放着压缩后的音视频数据。
* 数据帧AVFrame，压缩后的数据包经过解码器处理，就还原成了数据帧。
* 对于视频来说，一个视频帧就是一幅完整的图像。
* 对于音频来说，一个音频帧包含的是一小段时间的音频采样。

6. AVFilter 滤镜   
滤镜AVFilter定义了滤镜的规格，avfilter_get_by_name：根据滤镜名称获取滤镜。
* buffer: 视频输入滤镜
* buffersink: 视频输出滤镜
* abuffer: 音频输入滤镜
* abuffersink: 音频输出滤镜  
AVFilterContext滤镜实例  
* avfilter_graph_create_filter：创建滤镜实例。
* av_buffersrc_add_frame_flags：把一个数据帧添加到输入滤镜实例中
* av_buffersink_get_frame：从输出滤镜实例中获取一个数据帧。
* avfilter_free：释放滤镜实例。    
AVFilterGraph滤镜图，执行具体的滤镜操作。
* avfilter_graph_alloc：分配滤镜图。
* avfilter_graph_parse_ptr：通过字符串解析滤镜图。
* avfilter_graph_config：检查滤过滤字符串的有效性，配置滤镜图。
* avfilter_graph_free：释放滤镜图。    
AVFilterInOut滤镜输入输出实例
* avfilter_inout_alloc：分配滤镜输入输出实例。
* avfilter_inout_free：释放滤镜输入输出实例。

7. 打开和关闭音视频文件
```cpp
// 打开输入音视频文件
avformat_open_input(&fmt_ctx, input_file, nullptr, nullptr);
// 查找音视频文件中的流信息
avformat_find_stream_info(fmt_ctx, nullptr);
// 查看音视频的基本信息
av_dump_format(fmt_ctx, 0, input_file, 0);
```
8. 打开编解码器流程
```cpp
// 查找编解码器
AVCodec *codec = avcodec_find_decoder(codec_id);
// 分配编解码器上下文
AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
// 复制参数
avcodec_parameters_to_context(codec_ctx, stream->codecpar);
// 打开编解码器
avcodec_open2(codec_ctx, codec, nullptr);
```
9. 打开输出音视频文件
```cpp
// 分配输出音视频文件封装实例
avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, output_file);
// 打开输出流
avio_open(&out_fmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
// 创建输出流
AVStream *out_stream = avformat_new_stream(out_fmt_ctx, codec);
// 复制参数
avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
// 写入输出音视频文件头
avformat_write_header(out_fmt_ctx, nullptr);
// 写入输出音视频文件尾
av_write_trailer(out_fmt_ctx);
```
10. 初始化滤镜
```cpp
// 声明滤镜
AVFilter *buffersrc = avfilter_get_by_name("buffer");
AVFilter *buffersink = avfilter_get_by_name("buffersink");
// 创建滤镜实例
AVFilterContext *buffersrc_ctx = nullptr;
AVFilterContext *buffersink_ctx = nullptr;
AVFilterGraph *filter_graph = avfilter_graph_alloc();
// 分配滤镜的输入输出参数
AVFilterInOut *inputs = avfilter_inout_alloc();
AVFilterInOut *outputs = avfilter_inout_alloc();
// 拼接字符串参数
// 创建输入滤镜实例
avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
// 创建输出滤镜实例
avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
// 设置额外参数
av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
// 设置滤镜输入输出参数
// 把采用过滤字符串描述的图像添加到滤镜图中
avfilter_graph_parse_ptr(filter_graph, filter_desc, &inputs, &outputs, nullptr);
// 配置滤镜图
avfilter_graph_config(filter_graph, nullptr);
// 释放滤镜实例
avfilter_inout_free(&inputs);
```