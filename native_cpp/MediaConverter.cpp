#include "MediaConverter.h"

// 根据采样率返回 ADTS 头中规定的采样率索引（freqIdx）
int get_sample_rate_index(int sample_rate) {
    switch(sample_rate) {
        case 96000: return 0;
        case 88200: return 1;
        case 64000: return 2;
        case 48000: return 3;
        case 44100: return 4;
        case 32000: return 5;
        case 24000: return 6;
        case 22050: return 7;
        case 16000: return 8;
        case 12000: return 9;
        case 11025: return 10;
        case 8000:  return 11;
        default:    return 4; // 默认 44100Hz
    }
}

// 构造 AAC 的 ADTS header
void fillAdtsHeader(uint8_t *adtsHeader, int profile, int freqIdx, int chanCfg, int packetLen)
{
    /*
     * 0xFF, 0xF1, 0x4C, 0x80, 0x03, 0x7F, 0xFC
     *
     * 【adts_fixed_header】
     * syncword                   12    所有位必须为1，即0xFFF。
     * ID						  1		0代表MPEG-4, 1代表MPEG-2。
     * layer					  2		所有位必须为0。
     * protection_absent		  1		1代表没有CRC，0代表有。
     * profile					  2		配置级别（Table1.17）
     * sampling_frequency_index   4		标识使用的采样频率，具体见表。
     * private_bit				  1		see ISO / IEC 11172 - 3, subclause 2.4.2.3 (Table 8).
     * channel_configuration	  3		取值为0时，通过inband 的PCE设置channel configuration。
     * original / copy			  1		编码时设置为0，解码时忽略。
     * home                       1		编码时设置为0，解码时忽略。
     *
     * 【adts_variable_header】
     * copyright_identification_bit         1	  编码时设置为0，解码时忽略。
     * copyright_identification_start		1	  编码时设置为0，解码时忽略
     * frame_length                         13	  帧长度，包括header和crc的长度，单位byte
     * adts_buffer_fullness                 11
     * number_of_raw_data_blocks_in_frame	2	  ADTS帧中的AAC Frames(RDB)数减1, 为了最大的兼容性通常每个ADTS frame 包含一个AAC frame。
     */

    // 固定 ADTS 头结构，共 7 字节
    adtsHeader[0] = 0xFF;
    adtsHeader[1] = 0xF1; // MPEG-4 为 0xF1, 如果是 MPEG-2 可改为 0xF9
    adtsHeader[2] = ((profile - 1) << 6) | (freqIdx << 2) | (chanCfg >> 2);
    adtsHeader[3] = ((chanCfg & 3) << 6) | (packetLen >> 11);
    adtsHeader[4] = (packetLen & 0x7FF) >> 3;
    adtsHeader[5] = ((packetLen & 7) << 5) | 0x1F;
    adtsHeader[6] = 0xFC;
}

MediaConverter::MediaConverter()
{
    //const char* input_file = "http://devimages.apple.com/iphone/samples/bipbop/gear1/prog_index.m3u8";
    //const char* input_file = "rtmp://58.200.131.2:1935/livetv/cctv1";
    //const char* input_file = "http://192.168.3.86:9091/videos/record/XUNI0000000001-20201026_064506.flv";


    logger.debug("avformat_version :%d", avformat_version());
    av_log_set_level(AV_LOG_DEBUG);

//    demuxFileToH264("../media/aki.mp4", "../media/aki.h264");
//    muxH264ToFile("../media/1.mp4", "udp://238.1.1.123:1234");
//    decodeFromAnyToYuv("udp://238.1.1.123:1234", "../media/2.yuv");
//    encodeFromYuvToH264("../media/1.yuv", "../media/1.h264", 1920, 1080, 30);
//    muxH264ToFile("../media/1.h264", "../media/111.mp4");
    convertFileToFile("../media/1.mp4", "udp://238.1.1.123:1234");

//    demuxFileToAac("udp://238.1.1.123:1234", "../media/aki.aac");

//    printH264Stream("../media/aki.aac", 100);
}

int MediaConverter::demuxFileToAac(const std::string &inputFile, const std::string &outputFile)
{
    // 1. 创建输入文件的格式上下文
    AVFormatContext* pFormatCtx = avformat_alloc_context();

    // 2. 打开输入文件
    int ret = avformat_open_input(&pFormatCtx, inputFile.c_str(), nullptr, nullptr);
    if (ret < 0) return ret;

    // 3. 读取文件中的流信息
    ret = avformat_find_stream_info(pFormatCtx, nullptr);
    if (ret < 0) return ret;

    // 获取视频数据
    av_dump_format(pFormatCtx, -1, nullptr, 0);

    // 4. 查找音频流索引
    int audioIndex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIndex < 0) return audioIndex;

    // 5. 获取音频流参数
    AVCodecParameters* codecpar = pFormatCtx->streams[audioIndex]->codecpar;
    if (codecpar->codec_id != AV_CODEC_ID_AAC) {
        logger.error("Not an AAC audio stream!");
        return -1;
    }

    // 6. 打开输出文件（裸 AAC，需手动添加 ADTS 头）
    FILE* fd = fopen(outputFile.c_str(), "wb");
    if (!fd) return -1;

    AVPacket* packet = av_packet_alloc();
    bool addAdtsHeader = true;   // 标志：是否为需要手动添加 ADTS 头的裸 AAC 数据（通常从 MP4 提取出来的 AAC 没有 ADTS）

    // 7. 遍历读取音频帧
    while (av_read_frame(pFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == audioIndex)
        {
            // AAC码流 [7字节ADTS头][ACC ES数据][7字节ADTS头][ACC ES数据]

            if (addAdtsHeader)
            {
                // 构造 ADTS header（7 字节）
                uint8_t adtsHeader[7];
                int profile = 2; // AAC LC (Low Complexity)
                int freqIdx = get_sample_rate_index(codecpar->sample_rate); // 采样率索引
                int chanCfg = codecpar->ch_layout.nb_channels;
                int packetLen = packet->size + 7;

                fillAdtsHeader(adtsHeader, profile, freqIdx, chanCfg, packetLen);

                // 写入 ADTS 头
                fwrite(adtsHeader, 1, 7, fd);
            }

            // 写入 AAC 裸流数据（不包含封装）
            fwrite(packet->data, 1, packet->size, fd);
        }

        av_packet_unref(packet);
    }

    // 清理资源
    avformat_close_input(&pFormatCtx);
    av_packet_free(&packet);
    fclose(fd);

    logger.info("Successfully demuxed AAC from [%s] to [%s]", inputFile.c_str(), outputFile.c_str());

    return 0;
}

int MediaConverter::demuxFileToH264(const std::string& inputFile, const std::string& outputFile)
{
    //1.创建上下文
    AVFormatContext* pFormatCtx = avformat_alloc_context();

    //2.打开文件
    int ret = avformat_open_input(&pFormatCtx, inputFile.data(), nullptr, nullptr);
    if(ret < 0) return ret;

    //3.上下文获取流信息
    ret = avformat_find_stream_info(pFormatCtx, nullptr);
    if(ret < 0) return ret;

    //4.查找视频流
    int videoIndex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoIndex < 0) return videoIndex;

    //5.获取视频数据
    av_dump_format(pFormatCtx, -1, nullptr, 0);

#ifndef DEMUX_WITHOUT_FFMPEG
    //获取h264_mp4toannexb过滤器
    const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");

    //过滤器上下文创建
    AVBSFContext *bsfCtx;
    ret = av_bsf_alloc(bsf, &bsfCtx);
    if(ret < 0) return ret;

    // 将视频流的解码参数（codecpar）复制到比特流过滤器的输入参数中
    ret = avcodec_parameters_copy(bsfCtx->par_in, pFormatCtx->streams[videoIndex]->codecpar);
    if(ret < 0) return ret;

    //初始化比特流过滤器上下文
    ret = av_bsf_init(bsfCtx);
    if(ret < 0) return ret;
#endif

    FILE* fd = fopen(outputFile.data(), "wb");
    AVPacket* packet = av_packet_alloc();
    bool isAvcc = false;

    while(true)
    {
        if(av_read_frame(pFormatCtx, packet) < 0)
            break;

        // 保存视频
        if(packet->stream_index == videoIndex)
        {
#ifndef DEMUX_WITHOUT_FFMPEG
            if(av_bsf_send_packet(bsfCtx, packet) == 0)
                while(av_bsf_receive_packet(bsfCtx, packet) == 0)

            fwrite(packet->data, 1, packet->size, fd);
#else
            /*
             * extradata: [1字节编码格式][4字节编码相关配置][1字节SPS的数量][2字节SPS的长度][SPS的NALU][1字节PPS的数量][2字节PPS的长度][PPS的NALU]
             *
             * extradata = {
             *     0x01,                    // 标识数据格式（在某些情况下为 0x01 表示 AVCC 格式）
             *     0x64, 0x00, 0x1E, 0xFF,  // 编码相关的配置数据，分别表示配置文件指示(High Profile)、配置文件兼容性(无)、AVC Level(3.1)、NALU长度字段(无效)
             *     0xE1,                    // SPS NAL unit 的数量：第六个字节的低5位
             *     0x00, 0x1D,              // SPS NAL unit 的长度，此处表示长度为 29 字节，如果是 AnnexB 格式的话，此处为起始码
             *     0x67, 0x64, 0x00, 0x1E,  // SPS NAL unit (0x67开头; 包括图像大小、色彩信息等)
             *     0xAC, 0xD9, 0x41, 0x91,
             *     0xDE, 0x79, 0x26, 0xA0,
             *     0x20, 0x20, 0x28, 0x00,
             *     0x00, 0x03, 0x00, 0x08,
             *     0x00, 0x00, 0x03, 0x01,
             *     0x90, 0x78, 0xB1, 0x6C,
             *     0xB0,
             *     0x01,                    // PPS NAL unit 的数量
             *     0x00, 0x05,              // PPS NAL unit 的长度，此处表示长度为 5 字节
             *     0x68, 0xEA, 0xE3, 0xF2,  // PPS NAL unit (0x68开头; 编码的图像参数)
             *     0xC0
             * };
             */

            //手动提取
            static bool wroteExtradata = false;
            if (!wroteExtradata)
            {
                // 写 SPS / PPS（仅一次）
                uint8_t* extradata = pFormatCtx->streams[videoIndex]->codecpar->extradata;
                int extradata_size = pFormatCtx->streams[videoIndex]->codecpar->extradata_size;
                isAvcc = extradata[0];  //extradata[0] == 1表示该流使用了 AVCC（AVCodecContext）格式的编码方式

                // 打印展示
                if (extradata && extradata_size > 0)
                {
                    logger.debug("extradata size: %d", extradata_size);
                    for (int i = 0; i < extradata_size; ++i)
                        logger.debug("extradata[%d]: 0x%02X", i, extradata[i]);
                }

                // 写数据
                if (extradata && extradata_size > 0 && isAvcc)
                {
                    int sps_count = extradata[5] & 0x1f;    //extradata第6个字节的低5位(&0x1f得到低5位)包含了SPS的数量
                    int pos = 6;                            //当前解析位置，从第六个字节开始

                    for (int i = 0; i < sps_count; ++i)
                    {
                        uint16_t sps_len = (extradata[pos] << 8) | extradata[pos + 1];  // 获取 SPS 长度，占2字节
                        pos += 2;                                   // 移动到下一个位置
                        fwrite("\x00\x00\x00\x01", 1, 4, fd);       // 添加 Annex-B 前缀
                        fwrite(extradata + pos, 1, sps_len, fd);    // 写入 SPS 数据
                        pos += sps_len;                             // 移动到下一个 SPS 的位置
                    }

                    int pps_count = extradata[pos++];
                    for (int i = 0; i < pps_count; ++i)
                    {
                        uint16_t pps_len = (extradata[pos] << 8) | extradata[pos + 1];
                        pos += 2;
                        fwrite("\x00\x00\x00\x01", 1, 4, fd);
                        fwrite(extradata + pos, 1, pps_len, fd);
                        pos += pps_len;
                    }
                }
//                // 查看 AVPacket::data 数据
//                for (int i = 0; i < 300; ++i)
//                    logger.warning("data[%d]: 0x%02X", i, packet->data[i]);

                wroteExtradata = true;
            }

            // 写入 packet 数据（从 AVCC 转成 Annex-B）  AVPacket::data [4字节长度][NALU][4字节长度][NALU]
            if (isAvcc)
            {
                // AVCC 转 Annex-B 转换
                uint8_t* data = packet->data;
                int size = packet->size;
                int offset = 0;

                while (offset + 4 <= size)
                {
                    uint32_t nal_size = AV_RB32(data + offset); // 读取 NAL 单元的大小（4 字节）
                    offset += 4;
                    if (offset + nal_size > size) break; // 避免越界

                    fwrite("\x00\x00\x00\x01", 1, 4, fd); // 添加 Annex-B 前缀
                    fwrite(data + offset, 1, nal_size, fd); // 写入 NAL 数据
                    offset += nal_size;
                }
            }
            else
            {
                // 已经是 Annex-B 格式，直接写入数据
                fwrite(packet->data, 1, packet->size, fd);
            }
#endif
        }
        av_packet_unref(packet);
    }

    avformat_close_input(&pFormatCtx);
    av_packet_free(&packet);
    fclose(fd);
#ifndef DEMUX_WITHOUT_FFMPEG
    av_bsf_free(&bsfCtx);
#endif

    logger.info("Successfully demuxed from [%s] to [%s]", inputFile.data(), outputFile.data());

    return 0;
}

int MediaConverter::muxH264ToFile(const std::string &inputFile, const std::string &outputFile)
{
    AVFormatContext *input_ctx = NULL, *output_ctx = NULL;
    int ret;

    // 打开输入文件
    if ((ret = avformat_open_input(&input_ctx, inputFile.data(), NULL, NULL)) < 0)
        return ret;

    // 读取输入文件流信息
    if ((ret = avformat_find_stream_info(input_ctx, NULL)) < 0)
        return ret;

    // 查找视频流
    int videoIndex = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoIndex < 0) return videoIndex;

    // 自动推测格式
    const AVOutputFormat *ofmt = av_guess_format(NULL, outputFile.data(), NULL);

    // 创建输出格式上下文
    if ((ret = avformat_alloc_output_context2(&output_ctx, ofmt, NULL, outputFile.data())) < 0)
        return ret;

    // 复制输入流到输出流
    AVStream *input_stream = input_ctx->streams[videoIndex];
    AVStream *output_stream = avformat_new_stream(output_ctx, NULL);
    if (!output_stream)
        return AVERROR(ENOMEM);

    // 复制输入流的编解码参数到输出流
    if ((ret = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar)) < 0)
        return ret;

    // 打开输出文件
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if ((ret = avio_open(&output_ctx->pb, outputFile.data(), AVIO_FLAG_WRITE)) < 0)
            return ret;
    }

    // 写文件头
    if ((ret = avformat_write_header(output_ctx, NULL)) < 0)
        return ret;

    // 计算帧率 (fps)
    int time_base_numerator = m_tbnMap[ofmt->name];
    double fps = av_q2d(input_stream->avg_frame_rate);  // 将 AVRational 转换为浮动类型的帧率
    int64_t frame_duration = time_base_numerator / fps;  // 每帧的时间间隔，时间基单位
    int64_t pts = 0;  // 初始化时间戳

    // 读取并转发数据包
    AVPacket* packet = av_packet_alloc();
    while (av_read_frame(input_ctx, packet) >= 0)
    {
        if (packet->stream_index == input_stream->index)
        {
            // 转发包到输出文件
            packet->stream_index = output_stream->index;

            // 手动设置时间戳：使用逐帧递增的时间戳
            if (packet->pts == AV_NOPTS_VALUE)
                packet->pts = pts;
            if (packet->dts == AV_NOPTS_VALUE)
                packet->dts = pts;

            // 递增时间戳
            pts += frame_duration;

            // 写入帧时长，该值会影响写入的总时长
            packet->duration = frame_duration;

            // 确保时间戳已经设置
            if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(EINVAL));
                logger.warning("timestamp not set: %s", errbuf);
                av_packet_unref(packet);
                continue;  // 跳过此数据包
            }

            // 写输出文件
            if ((av_interleaved_write_frame(output_ctx, packet)) < 0)
            {
                logger.error("write packet failed");
                av_packet_unref(packet);
                break;
            }
        }
        av_packet_unref(packet);
    }

    // 写文件尾
    av_write_trailer(output_ctx);

    // 释放资源
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);
    av_packet_free(&packet);

    logger.info("Successfully muxed from [%s] to [%s]", inputFile.data(), outputFile.data());

    return 0;
}

int MediaConverter::decodeFromAnyToYuv(const std::string &inputFile, const std::string &outputFile)
{
    //1.创建上下文
    AVFormatContext* formatCtx = avformat_alloc_context();

    //2.打开文件
    int ret = avformat_open_input(&formatCtx, inputFile.data(), nullptr, nullptr);
    if(ret < 0)
    {
        logger.error("Error occurred in avformat_open_input");
        avformat_close_input(&formatCtx);
        return false;
    }

    //3.上下文获取流信息
    ret = avformat_find_stream_info(formatCtx, nullptr);
    if(ret < 0)
    {
        logger.error("Error occurred in avformat_find_stream_info");
        avformat_close_input(&formatCtx);
        return false;
    }

    //4.查找视频流和音频流
    int videoIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoIndex < 0)
        logger.warning("Not found video stream");

    //5.获取视频数据
    av_dump_format(formatCtx, -1, nullptr, 0);

    // 创建音视频解码器上下文
    AVCodecContext* codecCtx = avcodec_alloc_context3(nullptr);
    // 解码器上下文获取参数
    ret = avcodec_parameters_to_context(codecCtx, formatCtx->streams[videoIndex]->codecpar);
    if(ret < 0)
        logger.error("Error occurred in avcodec_parameters_to_context");
    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(codecCtx->codec_id);
    if(!codec)
        logger.error("Error occurred in avcodec_find_decoder");
    else
        logger.info("Using video decoder: %s", codec->name);

    // 打开解码器并绑定上下文
    ret = avcodec_open2(codecCtx, codec, nullptr);
    if(ret < 0)
        logger.error("Error occurred in avcodec_open2");

    // 打开输出文件
    FILE* yuvFile = fopen(outputFile.data(), "wb");
    if (!yuvFile) {
        logger.error("Failed to open output file");
        avformat_close_input(&formatCtx);
        avcodec_free_context(&codecCtx);
        return -1;
    }

    // 相关变量初始化
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    int width = codecCtx->width;
    int height = codecCtx->height;
    bool eof = false;

    // 设置 YUV420P 格式缓冲区
    AVFrame* yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = width;
    yuvFrame->height = height;
    av_frame_get_buffer(yuvFrame, 0);

    // 初始化像素格式转换上下文（确保输出为YUV420P）
    SwsContext* swsCtx = sws_getContext(width, height, codecCtx->pix_fmt,
                            width, height, AV_PIX_FMT_YUV420P,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);

    while(!eof)
    {
        if(av_read_frame(formatCtx, packet) < 0)
            eof = true; // 还需要执行一次以冲刷解码器，因此不直接break

        if (packet->stream_index == videoIndex)
        {
            avcodec_send_packet(codecCtx, packet);

            while(true)
            {
                ret = avcodec_receive_frame(codecCtx, frame);
                if(ret < 0)
                {
                    if(ret == AVERROR_EOF)
                        logger.info("Media decode finished.");  // 视频解码结束
                    break;
                }

                if (codecCtx->pix_fmt == AV_PIX_FMT_YUV420P)
                {
                    // 写入YUV数据（Y -> U -> V）
                    for (int i = 0; i < height; ++i)
                        fwrite(frame->data[0] + i * frame->linesize[0], 1, width, yuvFile); // 写入 Y 分量
                    for (int i = 0; i < height / 2; ++i)
                        fwrite(frame->data[1] + i * frame->linesize[1], 1, width / 2, yuvFile); // 写入 U 分量
                    for (int i = 0; i < height / 2; ++i)
                        fwrite(frame->data[2] + i * frame->linesize[2], 1, width / 2, yuvFile); // 写入 V 分量
                }
                else
                {
                    // 转换为YUV420P
                    sws_scale(swsCtx,
                              frame->data, frame->linesize,
                              0, height,
                              yuvFrame->data, yuvFrame->linesize);

                    // 写入YUV数据（Y -> U -> V）
                    for (int i = 0; i < height; ++i)
                        fwrite(yuvFrame->data[0] + i * yuvFrame->linesize[0], 1, width, yuvFile); // 写入 Y 分量
                    for (int i = 0; i < height / 2; ++i)
                        fwrite(yuvFrame->data[1] + i * yuvFrame->linesize[1], 1, width / 2, yuvFile); // 写入 U 分量
                    for (int i = 0; i < height / 2; ++i)
                        fwrite(yuvFrame->data[2] + i * yuvFrame->linesize[2], 1, width / 2, yuvFile); // 写入 V 分量
                }

                av_frame_unref(frame);
            }
        }

        av_packet_unref(packet);
    }

    // 清理
    fclose(yuvFile);
    sws_freeContext(swsCtx);
    av_frame_free(&frame);
    av_frame_free(&yuvFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);

    logger.info("Successfully decoded from [%s] to [%s]", inputFile.data(), outputFile.data());

    return 0;
}

int MediaConverter::encodeFromYuvToH264(const std::string &inputFile, const std::string &outputFile, int width, int height, int fps)
{
    // 1. 查找 H.264 编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        logger.error("H.264 encoder not found.");
        return -1;
    }

    // 2. 创建编码器上下文
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    codecCtx->width = width;
    codecCtx->height = height;
    codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx->time_base = AVRational{1, fps};
    codecCtx->framerate = AVRational{fps, 1};
    codecCtx->gop_size = 100;   // 每 100 帧一个 I 帧
    codecCtx->max_b_frames = 3; // 每两个 P 帧之间最多插入的 B 帧数

    /*
     * preset 值	编码速度	压缩效率（质量/体积）
     * ultrafast	最快	    最差
     * superfast	很快	    较差
     * veryfast	    快	    一般
     * faster       快      稍好
     * fast         中等	    稍好
     * medium       默认	    平衡
     * slow         慢      更好
     * slower	    更慢    很好
     * veryslow     最慢	   最佳压缩
     *
     *
     *  tune 值     作用描述
     *  film        优化电影内容，保留更多细节，减少压缩伪影，适合高质量视频。
     *  animation	优化动画视频内容，尽量减少编码过程中产生的伪影，适合动画视频。
     *  grain       保留细节，尤其是视频中的噪点（如颗粒感），适用于噪点较多的视频。
     *  fastdecode	优化解码速度，适用于低延迟应用，适合硬件解码或者实时应用场景。
     *  zerolatency	优化低延迟编码，适用于实时编码或直播场景，减少编码器内部的延迟。
     *  psnr        优化 PSNR（峰值信噪比）最大化，适用于研究和质量评估，但可能增加压缩体积。
     *  ssim        优化 SSIM（结构相似性指数）最大化，关注视频的结构保真度。
     *  allintra	优化为全内编码，即每帧都为 I 帧，适合逐帧编辑或需要完全随机访问的视频。
     *
     *
     * CRF 值	画质       文件大小       备注
     * 0        无损       最大          lossless 编码（仅限 yuv420p）
     * 18       非常高     大            肉眼几乎无损
     * 23       默认值     平衡          通常推荐值
     * 28       明显压缩   小            适用于尺寸优先
     * 30+      低画质     更小          仅适合预览用
     */

    // 可选：设置压缩参数
    av_opt_set(codecCtx->priv_data, "preset", "medium", 0);
    av_opt_set(codecCtx->priv_data, "crf", "23", 0); // x264/x265专用属性
//    av_opt_set(codecCtx->priv_data, "tune", "film", 0); // 优化电影内容
//    av_opt_set(codecCtx->priv_data, "bitrate", "1000000", 0); // 设置目标码率为 1 Mbps
//    av_opt_set(codecCtx->priv_data, "maxrate", "2000000", 0); // 最大码率 2 Mbps
//    av_opt_set(codecCtx->priv_data, "bufsize", "4000000", 0); // 设置缓冲区大小为 4 Mbps
//    av_opt_set(codecCtx->priv_data, "b-adapt", "2", 0); // 自适应 B 帧插入方式，0为完全按照max_b_frames插入（0禁用, 1简单, 2最强）
//    av_opt_set(codecCtx->priv_data, "level", "4.1", 0); // 编码器的级别（Level），控制视频的最大码率、分辨率等，常见值有 3.0, 3.1, 4.0, 4.1 等
//    av_opt_set(codecCtx->priv_data, "color_primaries", "bt709", 0); // 设置色彩主基准（例如 bt709，bt2020 等）。
//    av_opt_set(codecCtx->priv_data, "colorspace", "bt709", 0); // 设置色彩空间（例如 bt601, bt709）。
//    av_opt_set(codecCtx->priv_data, "colormatrix", "bt709", 0); // 设置颜色矩阵（例如 bt709，bt2020）。


    // 3. 打开编码器
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        logger.error("Failed to open encoder.");
        avcodec_free_context(&codecCtx);
        return -1;
    }

    // 4. 打开输入和输出文件
    FILE* inFile = fopen(inputFile.c_str(), "rb");
    FILE* outFile = fopen(outputFile.c_str(), "wb");

    // 5. 初始化 AVFrame
    AVFrame* frame = av_frame_alloc();
    frame->format = codecCtx->pix_fmt;
    frame->width = width;
    frame->height = height;
    av_frame_get_buffer(frame, 32); // 对齐 32 字节

    AVPacket* packet = av_packet_alloc();
    bool eof = false;
    int ret = 0;

    // 6. 开始读取 YUV 数据并编码
    while (!eof)
    {
        // 读取YUV数据
        for (int i = 0; i < height; ++i)
            if (fread(frame->data[0] + i * frame->linesize[0], 1, width, inFile) == 0) {
                eof = true;
                break;
            }

        for (int i = 0; i < height / 2; ++i)
            if (fread(frame->data[1] + i * frame->linesize[1], 1, width / 2, inFile) == 0) {
                eof = true;
                break;
            }

        for (int i = 0; i < height / 2; ++i)
            if (fread(frame->data[2] + i * frame->linesize[2], 1, width / 2, inFile) == 0) {
                eof = true;
                break;
            }

        // 添加pts（不加pts会减小体积）
//        static int frameIndex = 0;
//        frame->pts = frameIndex++;

        // 送入编码器
        avcodec_send_frame(codecCtx, frame);

        // 从编码器取出数据
        while (avcodec_receive_packet(codecCtx, packet) == 0)
        {
            fwrite(packet->data, 1, packet->size, outFile);
            av_packet_unref(packet);
        }
    }

    // 7. 刷新编码器
    avcodec_send_frame(codecCtx, nullptr);
    while (avcodec_receive_packet(codecCtx, packet) == 0)
    {
        fwrite(packet->data, 1, packet->size, outFile);
        av_packet_unref(packet);
    }

    // 8. 清理资源
    fclose(inFile);
    fclose(outFile);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);

    logger.info("Successfully encoded from [%s] to [%s]", inputFile.data(), outputFile.data());

    return 0;
}

int MediaConverter::convertFileToFile(const std::string &inputFile, const std::string &outputFile)
{
    //1.定义输入输出格式上下文
    AVFormatContext* inputFormatCtx = nullptr;
    AVFormatContext* outputFormatCtx = nullptr;

    //2.打开输入文件
    int ret = avformat_open_input(&inputFormatCtx, inputFile.data(), nullptr, nullptr);
    if(ret < 0) return -1;

    //3.分析流信息
    ret = avformat_find_stream_info(inputFormatCtx, nullptr);
    if(ret < 0) return -1;

    //控制输出格式
    const char* ofmt_name = nullptr;
    if (outputFile.find("udp://") != std::string::npos)
        ofmt_name = "flv";

    //4.获取输出上下文
    ret = avformat_alloc_output_context2(&outputFormatCtx, nullptr, ofmt_name, outputFile.data());
    if(ret < 0) return -1;

    //5.获取流信息
    for(unsigned int i = 0; i < inputFormatCtx->nb_streams; i++)
    {
        AVStream *in_stream = inputFormatCtx->streams[i];
        AVStream *out_stream = avformat_new_stream(outputFormatCtx, nullptr);
        if(!out_stream) return -1;

        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if(ret < 0) return -1;

        out_stream->codecpar->codec_tag = 0;
    }

    //6.打印输入流信息
    av_dump_format(inputFormatCtx, 0, inputFile.data(), 0);

    //7.输出为文件时需要打开
    if(outputFormatCtx && !(outputFormatCtx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&outputFormatCtx->pb, outputFile.data(), AVIO_FLAG_WRITE);
        if(ret < 0) return -1;
    }

    //7.写文件头
    ret = avformat_write_header(outputFormatCtx, nullptr);
    if(ret < 0) return -1;

    av_dump_format(outputFormatCtx, 0, outputFile.data(), 1);          //打印输出信息

    //查找是否有视频流
    AVMediaType printMediaType = AVMEDIA_TYPE_AUDIO;
    int videoIndex = av_find_best_stream(inputFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoIndex >= 0)
        printMediaType = AVMEDIA_TYPE_VIDEO;

    uint64_t frameIndex = 0;
    AVPacket *packet = av_packet_alloc();
    int64_t startTime = av_gettime();            //计时
    AVRational time_base_q = {1, AV_TIME_BASE};  // us
    bool outputStream = (outputFile.find("://") != std::string::npos) && (outputFile.find("file://") == std::string::npos); // 网络流

    while(true)
    {
        //读取一个包
        if(av_read_frame(inputFormatCtx, packet) < 0)
            break;

        AVStream *inStream = inputFormatCtx->streams[packet->stream_index];
        AVStream *outStream = outputFormatCtx->streams[packet->stream_index];

        //输出到屏幕
        AVMediaType mediaType = inStream->codecpar->codec_type;
        if(mediaType == printMediaType)
        {
            if(frameIndex % 10 == 0)    //10帧1打印
            {
                uint64_t time = (av_gettime() - startTime) / AV_TIME_BASE;
                logger.debug("frameIndex: %llu , time: %llds", frameIndex, time);
            }
            frameIndex++;
        }

        //延时
        if(outputStream)
        {
            if(mediaType == AVMEDIA_TYPE_VIDEO)
            {
              int64_t pts_time = av_rescale_q(packet->pts, inStream->time_base, time_base_q); // us
              int64_t now_time = av_gettime() - startTime;
              if(pts_time > now_time)
                  av_usleep(pts_time - now_time);
            }
        }

        // PTS转换。输入、输出都是rtmp时，时基一样，可省略时间戳转换
        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

        // 写包
        ret = av_interleaved_write_frame(outputFormatCtx, packet);
        if(ret < 0) return ret;

        av_packet_unref(packet);
    }

    /* 写文件尾
    * 若是rtmp推流，可忽略以下报错信息，因为推流rtmp使用的flv格式不含时长和大小信息
    * [flv @ 0000000031af2880] Failed to update header with correct duration.
    * [flv @ 0000000031af2880] Failed to update header with correct filesize.
    */
    ret = av_write_trailer(outputFormatCtx);
    if(ret < 0) return ret;

    //关闭输入
    avformat_close_input(&inputFormatCtx);
    av_packet_free(&packet);

    if(outputFormatCtx && !(outputFormatCtx->flags & AVFMT_NOFILE))
    {
        avio_close(outputFormatCtx->pb);
    }
    avformat_free_context(outputFormatCtx);

    logger.info("Successfully conversion from [%s] to [%s]", inputFile.data(), outputFile.data());

    return 0;
}




