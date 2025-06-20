#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream> // 添加ostringstream
#include <cstdarg> // 添加可变参数支持

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    Logger();
    ~Logger();

    void setLogLevel(LogLevel level);
    void setOutputFile(const std::string& filename);

    // 添加格式化的日志函数
    void trace(const char* format, ...);
    void debug(const char* format, ...);
    void info(const char* format, ...);
    void warning(const char* format, ...);
    void error(const char* format, ...);
    void critical(const char* format, ...);

    // 特化支持单个元素输出
    template <typename T>
    void trace(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::TRACE, oss.str());
    }
    template <typename T>
    void debug(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::DEBUG, oss.str());
    }

    template <typename T>
    void info(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::INFO, oss.str());
    }
    template <typename T>
    void warning(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::WARNING, oss.str());
    }
    template <typename T>
    void error(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::ERROR, oss.str());
    }
    template <typename T>
    void critical(const T& value) {
        thread_local std::ostringstream oss;
        oss << value;
        log(LogLevel::CRITICAL, oss.str());
    }

    // 特化支持数组输出
    template <typename T, std::size_t N>
    void trace(const T (&arr)[N]) {
        log(LogLevel::TRACE, formatArray(arr));
    }

    template <typename T, std::size_t N>
    void debug(const T (&arr)[N]) {
        log(LogLevel::DEBUG, formatArray(arr));
    }

    template <typename T, std::size_t N>
    void info(const T (&arr)[N]) {
        log(LogLevel::INFO, formatArray(arr));
    }

    template <typename T, std::size_t N>
    void warning(const T (&arr)[N]) {
        log(LogLevel::WARNING, formatArray(arr));
    }

    template <typename T, std::size_t N>
    void error(const T (&arr)[N]) {
        log(LogLevel::ERROR, formatArray(arr));
    }

    template <typename T, std::size_t N>
    void critical(const T (&arr)[N]) {
        log(LogLevel::CRITICAL, formatArray(arr));
    }

    void print(const char* name) {
        std::cout << "name:" << name << std::endl;
    }

private:
    template <typename T, std::size_t N>
    std::string formatArray(const T (&arr)[N]) {
        std::ostringstream oss;
        oss << "[";
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (std::is_same<T, const char*>::value) {    // 如果是 const char* 数组，空指针不作输出
                if (arr[i] != nullptr)
                    oss << arr[i];
            } else {
                oss << arr[i];
            }
            if (i < N - 1) oss << ", ";
        }
        oss << "]";
        return oss.str();
    }

    std::string logLevelToString(LogLevel level);
    std::string getCurrentTime(); // 获取当前时间
    void log(LogLevel level, const std::string& message);
    std::string colorize(const std::string& message, LogLevel level, bool isTime = false); // 添加颜色输出

    LogLevel logLevel_; // 当前日志等级
    std::ofstream outputFile_; // 输出文件流
    std::mutex mutex_; // 线程安全
    bool isOutputFileSet_; // 是否设置输出文件
};

extern Logger logger; // 声明全局变量

//单元测试
void LoggerTest();

#endif // LOGGER_H
