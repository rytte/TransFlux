#ifndef MEDIACONVERTER_H
#define MEDIACONVERTER_H

#include <iostream>
#include "Logger.h"
#include "Tool.h"
#include <map>
#include <chrono>

#define DEMUX_WITHOUT_FFMPEG

extern"C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavcodec/bsf.h"
#include "libavutil/time.h"
#include <libavutil/intreadwrite.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}


class MediaConverter
{
public:
    MediaConverter();

    enum MediaFormat {
        H264,
        YUV,
        UDP,
        MP4,
        MKV,
        MOV,
        FLV,
        AVI
    };

    // File/Stream <--> H264 <--> YUV <--> RGB

    std::map<std::string, int> m_tbnMap = {
        {"flv", 1000},
        {"mp4", 90000},
        {"avi", 600},
        {"matroska", 1000}, // MKV
        {"mov", 90000},
    };

    //TS格式

    //file >> aac（暂不支持stream输入）
    int demuxFileToAac(const std::string& inputFile, const std::string& outputFile);

    //file/stream >> h264, avcc >> annex-b（输入为stream时，至少要保存到一个I帧且在前80帧内(大致)）
    int demuxFileToH264(const std::string& inputFile, const std::string& outputFile);

    //h264 >> file, annex-b >> avcc（输出不能为stream，因为无封装信息）（输出的avi是annexb格式）
    int muxH264ToFile(const std::string& inputFile, const std::string& outputFile);

    //file/h264/stream >> yuv（输出格式为YUV420P）
    int decodeFromAnyToYuv(const std::string& inputFile, const std::string& outputFile);

    //yuv >> h264（输入格式为YUV420P）
    int encodeFromYuvToH264(const std::string& inputFile, const std::string& outputFile, int width, int height, int fps);

    //file/stream >> file/stream. 本地推流、拉流保存、转封装、流转发（输出为stream时，需要延时）（输入输出为avi时无效，可能需要转格式）
    int convertFileToFile(const std::string& inputFile, const std::string& outputFile);

    // AVCC     MP4、FLV、MKV
    // Annex-B  流媒体、TS
};


#endif // MEDIACONVERTER_H
