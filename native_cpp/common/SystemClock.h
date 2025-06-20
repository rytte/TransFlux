#ifndef SYSTEMCLOCK_H
#define SYSTEMCLOCK_H

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

class SystemClock
{
public:
    SystemClock() : m_timeAccumulated(0), m_isPaused(true), m_speed(1.0), m_shouldRun(false) {}

    // 启动计时器
    void start()
    {
        if (!m_shouldRun)
        {
            m_shouldRun = true;
            m_isPaused = false;
            m_timeAccumulated = 0.0;
            m_lastUpdateTime = std::chrono::steady_clock::now(); // 初始化开始时间
            m_timerThread = std::thread(&SystemClock::updateTime, this);
        }
    }

    // 暂停计时器
    void pause()
    {
        m_isPaused = true;
    }

    // 恢复计时器
    void resume()
    {
        if (m_isPaused)
        {
            m_isPaused = false;
            m_lastUpdateTime = std::chrono::steady_clock::now(); // 重置恢复后的时间基准
        }
    }

    // 设置播放速度
    void setSpeed(double speed)
    {
        if (speed > 0)
        {
            m_speed = speed;
        }
    }

    // 设置当前时间 (秒)
    void setTime(double timeInSeconds)
    {
        m_timeAccumulated = timeInSeconds * 1000.0;  // 转换为毫秒
    }

    // 获取当前播放时间 (秒)
    double getTime() const
    {
        return m_timeAccumulated / 1000.0;  // 转换为秒
    }

    // 停止计时器
    void stop()
    {
        m_shouldRun = false;
        m_timeAccumulated = 0.0;
        if (m_timerThread.joinable())
        {
            m_timerThread.join();
        }
    }

    ~SystemClock()
    {
        stop();
    }

private:
    void updateTime()
    {
        while (m_shouldRun)
        {
            if (!m_isPaused)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime).count();

                // 累加当前速度下的时间
                m_timeAccumulated = m_timeAccumulated + elapsed * m_speed;

                // 更新最后更新时间
                m_lastUpdateTime = now;
            }

            // 模拟帧率（例如每16ms更新一次，相当于60帧每秒）
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    std::atomic<bool> m_shouldRun;      // 用于控制计时器线程的运行
    std::atomic<bool> m_isPaused;       // 是否暂停
    std::atomic<double> m_timeAccumulated;  // 累积时间（毫秒）
    std::atomic<double> m_speed;        // 当前播放速度
    std::chrono::steady_clock::time_point m_lastUpdateTime; // 用于记录每次更新的时间点
    std::thread m_timerThread;          // 计时器线程
};



#endif // SYSTEMCLOCK_H
