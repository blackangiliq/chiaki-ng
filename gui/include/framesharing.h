// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Ultra-Fast Frame Sharing - v3.1 with Async Processing
// Zero impact on main Chiaki thread

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef _WIN32
#include <windows.h>
#endif

// Version 3 Header - Double Buffered
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;           // 0x4B414843 "CHAK"
    uint32_t version;         // 3
    uint32_t maxWidth;        // Maximum buffer width
    uint32_t maxHeight;       // Maximum buffer height
    
    // Buffer 0 metadata
    uint32_t width0;
    uint32_t height0;
    uint32_t stride0;
    uint32_t dataSize0;
    uint64_t timestamp0;
    uint64_t frameNumber0;
    
    // Buffer 1 metadata  
    uint32_t width1;
    uint32_t height1;
    uint32_t stride1;
    uint32_t dataSize1;
    uint64_t timestamp1;
    uint64_t frameNumber1;
    
    // Synchronization
    volatile int32_t writeBuffer;    // Which buffer producer is writing to (0 or 1)
    volatile int32_t readyBuffer;    // Which buffer is ready to read (-1 = none)
    volatile uint32_t producerLock;  // Simple spinlock for producer
    volatile uint32_t consumerLock;  // Simple spinlock for consumer
    
    // Performance counters
    volatile uint64_t totalFramesWritten;
    volatile uint64_t totalFramesRead;
    volatile uint64_t droppedFrames;
};
#pragma pack(pop)

// Header size: 120 bytes (no padding needed, pack(1) ensures tight packing)

class FrameSharing
{
public:
    static FrameSharing& instance() {
        static FrameSharing inst;
        return inst;
    }
    
    bool initialize(int maxWidth, int maxHeight);
    void shutdown();
    
    // Non-blocking: queues frame for async processing
    // Returns immediately, does NOT block Chiaki's main thread
    void queueFrame(AVFrame *frame);
    
    bool isActive() const { return active.load(std::memory_order_acquire); }
    
    // Statistics
    double getAvgWriteTimeUs() const { return profileFrameCount > 0 ? (double)profileTotalUs / profileFrameCount : 0; }
    uint64_t getProfileFrameCount() const { return profileFrameCount; }
    uint64_t getTotalFramesWritten() const;
    uint64_t getDroppedFrames() const;
    uint64_t getQueuedFrames() const { return queuedFrames.load(); }

private:
    FrameSharing();
    ~FrameSharing() { shutdown(); }
    
    // Non-copyable
    FrameSharing(const FrameSharing&) = delete;
    FrameSharing& operator=(const FrameSharing&) = delete;
    
    // Async processing
    void workerThread();
    bool processFrame(AVFrame *frame);
    
    std::atomic<bool> active{false};
    std::atomic<bool> workerRunning{false};
    std::thread worker;
    
    // Frame queue (lock-free single producer, single consumer)
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::queue<AVFrame*> frameQueue;
    static constexpr size_t MAX_QUEUE_SIZE = 2;  // Keep only latest frames
    std::atomic<uint64_t> queuedFrames{0};
    
    uint64_t frameNumber{0};
    int maxW{0}, maxH{0};
    SwsContext *swsCtx{nullptr};
    
#ifdef _WIN32
    HANDLE hMap{nullptr};
    HANDLE hEvent{nullptr};
    void *mem{nullptr};
    
    int currentWriteBuffer{0};
    
    // Profiling
    uint64_t profileFrameCount{0};
    uint64_t profileTotalUs{0};
    LARGE_INTEGER perfFreq;
    
    uint8_t* getBufferPtr(int bufferIndex);
    void switchWriteBuffer();
#endif
};

#endif // CHIAKI_FRAMESHARING_H
