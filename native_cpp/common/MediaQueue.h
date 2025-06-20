#ifndef MEDIAQUEUE_H
#define MEDIAQUEUE_H

#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

extern "C"
{
#include "libavcodec/avcodec.h"
}


class MediaQueue
{
public:
    MediaQueue() {}
    ~MediaQueue() { clear(); }

    // Reset the media queue for reuse
    void reset() { clear(); }

    void pushAudioFrame(const AVFrame* frame) { push(frame, audioFrames); }
    void pushVideoFrame(const AVFrame* frame) { push(frame, videoFrames); }
    void pushAudioPacket(const AVPacket* packet) { push(packet, audioPackets); }
    void pushVideoPacket(const AVPacket* packet) { push(packet, videoPackets); }
    AVFrame* popAudioFrame() { return pop(audioFrames); }
    AVFrame* popVideoFrame() { return pop(videoFrames); }
    AVPacket* popAudioPacket() { return pop(audioPackets); }
    AVPacket* popVideoPacket() { return pop(videoPackets); }

    int getAudioFrameCount() const { return audioFrames.size(); }
    int getVideoFrameCount() const { return videoFrames.size(); }
    int getAudioPacketCount() const { return audioPackets.size(); }
    int getVideoPacketCount() const { return videoPackets.size(); }

    // Clear all frames and packets
    void clear()
    {
        std::unique_lock<std::mutex> lock(mutex);

        while (!audioFrames.empty())
        {
            av_frame_free(&audioFrames.front());
            audioFrames.pop();
        }
        while (!videoFrames.empty())
        {
            av_frame_free(&videoFrames.front());
            videoFrames.pop();
        }
        while (!audioPackets.empty())
        {
            av_packet_free(&audioPackets.front());
            audioPackets.pop();
        }
        while (!videoPackets.empty())
        {
            av_packet_free(&videoPackets.front());
            videoPackets.pop();
        }
    }

private:
    // Template method to push a frame or packet into the respective queue
    template <typename T>
    void push(const T* data, std::queue<T*>& dataQueue)
    {
        T* newData = nullptr;
        if constexpr (std::is_same<T, AVFrame>::value)      // if constexpr: Cpp17
        {
            newData = av_frame_alloc();
            av_frame_ref(newData, data); // For AVFrame
        }
        else if constexpr (std::is_same<T, AVPacket>::value)
        {
            newData = av_packet_alloc();
            av_packet_ref(newData, data); // For AVPacket
        }

        std::unique_lock<std::mutex> lock(mutex);
        dataQueue.push(newData);
    }

    // Template method to pop a frame or packet from the respective queue
    template <typename T>
    T* pop(std::queue<T*>& dataQueue)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (dataQueue.empty())
            return nullptr;
        T* data = dataQueue.front();
        dataQueue.pop();
        return data;
    }

private:
    std::queue<AVFrame*> audioFrames; // Queue for audio frames
    std::queue<AVFrame*> videoFrames; // Queue for video frames
    std::queue<AVPacket*> audioPackets; // Queue for audio packets
    std::queue<AVPacket*> videoPackets; // Queue for video packets
    std::mutex mutex;                   // Mutex for thread safety
};


#endif // MEDIAQUEUE_H
