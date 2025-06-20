#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include "Logger.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
}


//获取视频时长（秒）
int uGetMediaDuration(const std::string& filePath);

//保存视频帧
bool uSaveFrameToBmp(const std::string& filePath, const std::string& outputPath, int sec);

//格式化时长（hh:mm:ss）
std::string uTimeFormatting(int secs);


#endif // UTILS_H
