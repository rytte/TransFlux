#include "Utils.h"

#include <fstream>  // 用于写入 BMP 文件

// 像素存储格式
enum StorageFormat {
    RGBA = 0,
    BGRA
};

// 定义 BMP 文件头和信息头结构
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t fileType{0x4D42};  // 文件类型 'BM'
    uint32_t fileSize{0};       // 文件大小
    uint16_t reserved1{0};      // 保留字段
    uint16_t reserved2{0};      // 保留字段
    uint32_t offsetData{54};    // 图像数据的偏移量
};

struct BMPInfoHeader {
    uint32_t size{40};           // 结构体大小
    int32_t width{0};            // 图像宽度
    int32_t height{0};           // 图像高度
    uint16_t planes{1};          // 颜色平面数（总是为1）
    uint16_t bitCount{32};       // 每个像素的位数（这里是32位RGB格式）
    uint32_t compression{0};     // 压缩方式（0表示不压缩）
    uint32_t sizeImage{0};       // 图像大小（不压缩时可以为0）
    int32_t xPixelsPerMeter{0};  // 水平分辨率
    int32_t yPixelsPerMeter{0};  // 垂直分辨率
    uint32_t colorsUsed{0};      // 使用的颜色数
    uint32_t colorsImportant{0}; // 重要的颜色数
};
#pragma pack(pop)


static bool uSaveBMP(const char *filePath, uint8_t *buffer, int width, int height, StorageFormat inputFormat)
{
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;

    infoHeader.width = width;
    infoHeader.height = -height;  // BMP 的图像数据默认从左下角开始，负值表示从左上角开始
    fileHeader.fileSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + width * height * 4;  // 每像素 4 字节

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
        return false;

    // 写入文件头
    outFile.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));

    // 写入信息头
    outFile.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    // 写入像素数据（如果 buffer 是 RGBA 格式的，需要转换为 BMP 文件的 BGRA 顺序存储）
    if(inputFormat == StorageFormat::BGRA)
        outFile.write(reinterpret_cast<const char*>(buffer), width * height * 4);
    else if(inputFormat == StorageFormat::RGBA)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                uint8_t* pixel = buffer + (y * width + x) * 4;  // 4字节一个像素（RGBA）
                outFile.put(pixel[2]);  // B
                outFile.put(pixel[1]);  // G
                outFile.put(pixel[0]);  // R
                outFile.put(pixel[3]);  // A
            }
        }
    }

    outFile.close();
    return true;
}


//获取formatCtx
static AVFormatContext* uGetMediaInfo(const std::string& filePath)
{
    //1.创建上下文，注意使用后要释放
    AVFormatContext* formatCtx = avformat_alloc_context();

    //2.打开文件
    int ret = avformat_open_input(&formatCtx, filePath.data(), nullptr, nullptr);
    if(ret < 0)
    {
        logger.error("Error occurred in avformat_open_input");
        avformat_close_input(&formatCtx);
        return nullptr;
    }

    //3.上下文获取流信息
    ret = avformat_find_stream_info(formatCtx, nullptr);
    if(ret < 0)
    {
        logger.error("Error occurred in avformat_find_stream_info");
        avformat_close_input(&formatCtx);
        return nullptr;
    }

    return formatCtx;
}


int uGetMediaDuration(const std::string& filePath)
{
    AVFormatContext* formatCtx = uGetMediaInfo(filePath);
    if (!formatCtx)
        return -1; // 如果无法获取格式上下文，返回-1
    int64_t duration = formatCtx->duration;  // 获取视频总时长（单位：微秒）
    avformat_close_input(&formatCtx);        // 释放资源
    int secs = duration / AV_TIME_BASE;      // 将微秒转换为秒
    if(secs < 0)                             // 直播流的情况下会小于0
        return 0;
    else
        return secs;
}


bool uSaveFrameToBmp(const std::string& filePath, const std::string& outputPath, int sec)
{
    AVFormatContext* formatCtx = uGetMediaInfo(filePath);

    int videoStreamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex == -1) {
        logger.error("Could not find a video stream.");
        avformat_close_input(&formatCtx);
        return false;
    }

    AVCodecParameters* codecpar = formatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        logger.error("Could not find decoder for codec ID: %d", codecpar->codec_id);
        avformat_close_input(&formatCtx);
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        logger.error("Could not allocate codec context.");
        avformat_close_input(&formatCtx);
        return false;
    }

    if (avcodec_parameters_to_context(codecCtx, codecpar) < 0) {
        logger.error("Could not copy codec parameters to codec context.");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return false;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        logger.error("Could not open codec.");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return false;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRgb = av_frame_alloc();

    // 跳转
    if (av_seek_frame(formatCtx, -1, sec, AVSEEK_FLAG_BACKWARD) < 0)
    {
        logger.error("Could not seek to frame, use current position frame.");
    }

    while (av_read_frame(formatCtx, packet) >= 0)
    {
        if (packet->stream_index == videoStreamIndex)
        {
            if (avcodec_send_packet(codecCtx, packet) >= 0)
            {
                int ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN))
                    continue;
                else if (ret < 0)
                {
                    logger.error("Could not receive a frame");
                    av_frame_free(&frameRgb);
                    av_frame_free(&frame);
                    av_packet_free(&packet);
                    avcodec_free_context(&codecCtx);
                    avformat_close_input(&formatCtx);
                    return false;
                }
                else
                    break;  // 获取到了frame直接返回
            }
        }
        av_packet_unref(packet);  // 释放数据包
    }

    // 设置输出图像的参数
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRgb->data, frameRgb->linesize, buffer, AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height, 1);

    // 初始化swsCtx（假设codecCtx的参数在整个过程中保持不变）
    SwsContext* swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx)
    {
        logger.error("Failed to initialize sws context.");
        av_freep(&buffer);
        av_frame_free(&frameRgb);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return false;
    }

    // 使用已经初始化的swsCtx进行转换
    sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height, frameRgb->data, frameRgb->linesize);

    // 保存为图片
    bool saved = uSaveBMP(outputPath.data(), buffer, codecCtx->width, codecCtx->height, StorageFormat::BGRA);

    // 清理资源
    sws_freeContext(swsCtx);
    av_freep(&buffer);
    av_frame_free(&frame);
    av_frame_free(&frameRgb);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);

    if(saved)
    {
        logger.info("Image saved successfully at: %s", outputPath.data());
        return true;
    }
    else
    {
        logger.info("Image saved failed at: %s", outputPath.data());
        return false;
    }
}

std::string uTimeFormatting(int secs)
{
    // 计算小时、分钟和秒
    int hours = secs / 3600;
    int minutes = (secs % 3600) / 60;
    int seconds = secs % 60;

    // 格式化为 HH:MM:SS
    char durationStr[9]; // 长度为 8 + 1（用于 '\0'）
    snprintf(durationStr, sizeof(durationStr), "%02d:%02d:%02d", hours, minutes, seconds);

    return std::string(durationStr); // 返回格式化后的字符串
}
