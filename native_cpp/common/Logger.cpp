#include "Logger.h"

Logger logger; // 定义全局变量

Logger::Logger() : logLevel_(LogLevel::TRACE), isOutputFileSet_(false) {}

Logger::~Logger() {
    if (outputFile_.is_open()) {
        outputFile_.close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

void Logger::setOutputFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (outputFile_.is_open()) {
        outputFile_.close();
    }
    outputFile_.open(filename, std::ios::out | std::ios::app);
    isOutputFileSet_ = true; // 设置输出文件标志
}

std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE   ";
        case LogLevel::DEBUG: return "DEBUG   ";
        case LogLevel::INFO: return "INFO    ";
        case LogLevel::WARNING: return "WARNING ";
        case LogLevel::ERROR: return "ERROR   ";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN  ";
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    char buffer[20]; // yyyy-mm-dd hh:mm:ss
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buffer);
}

std::string Logger::colorize(const std::string& message, LogLevel level, bool isTime) {
    if (isTime) {
        return "\033[34m" + message + "\033[0m"; // 时间用蓝色
    }

    switch (level) {
        case LogLevel::TRACE:    return "\033[37m" + message + "\033[0m"; // TRACE用灰色
        case LogLevel::DEBUG:    return "\033[36m" + message + "\033[0m"; // DEBUG用青色
        case LogLevel::INFO:     return "\033[32m" + message + "\033[0m"; // INFO用绿色
        case LogLevel::WARNING:  return "\033[33m" + message + "\033[0m"; // WARNING用黄色
        case LogLevel::ERROR:    return "\033[31m" + message + "\033[0m"; // ERROR用红色
        case LogLevel::CRITICAL: return "\033[41m" + message + "\033[0m"; // CRITICAL用红色背景
        default:                 return message; // 默认无颜色
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level >= logLevel_) {
        std::string levelStr = colorize(logLevelToString(level), level);
        std::string currentTime = colorize(getCurrentTime(), level, true);
        std::string messageWithColor = colorize(message, level);

        std::string fullMessageWithoutColor = "[" + logLevelToString(level) + "][" + getCurrentTime() + "] - " + message;

        std::cout << "[" << levelStr << "][" << currentTime << "] - " << messageWithColor << std::endl;

        if (isOutputFileSet_ && outputFile_.is_open()) {
            outputFile_ << fullMessageWithoutColor << std::endl;
        }
    }
}

// 添加格式化日志函数
void Logger::trace(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args); // 使用vsnprintf格式化字符串
    va_end(args);
    log(LogLevel::TRACE, buffer);
}

void Logger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args); // 使用vsnprintf格式化字符串
    va_end(args);
    log(LogLevel::DEBUG, buffer);
}

void Logger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::INFO, buffer);
}

void Logger::warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::WARNING, buffer);
}

void Logger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::ERROR, buffer);
}

void Logger::critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LogLevel::CRITICAL, buffer);
}


//单元测试
void LoggerTest()
{
    const char* a = "abc";
    int b = 100;
    double c = 3.1415926;
    bool d = true;
    int arr[10] = {1, 2, 3, 4, 5};
    const char* strarr[10] = {"a", "bb", "ccc"};
    std::string strArrs[10] = {"Hello", "World", "C++"};
    float f[10] = {1.1, 2.22, 3.333, 4.4444, 5.55555};
    logger.warning("%s andme", a);
    logger.warning(a);
    logger.warning(b);
    logger.warning(c);
    logger.warning(d);
    logger.warning(arr);
    logger.warning(strarr);
    logger.warning(strArrs);
    logger.warning(f);
}

