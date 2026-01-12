// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.4 (with Performance Stats)

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;           // 0x4B414843 = "CHAK"
    uint32_t version;         // 3 (updated for stats)
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;
    volatile uint32_t ready;
    
    // Performance Statistics (v3)
    float measuredBitrateMbps;  // Current bitrate in Mbps
    float packetLossPercent;    // Packet loss percentage (0-100)
    uint32_t droppedFrames;     // Total dropped frames
    uint32_t targetFps;         // Target FPS (30 or 60)
    uint32_t actualFps;         // Actual measured FPS
    uint32_t reserved[4];       // Reserved for future use
};
#pragma pack(pop)

class FrameSharing
{
public:
    static FrameSharing& instance() {
        static FrameSharing inst;
        return inst;
    }
    
    bool initialize(int maxWidth, int maxHeight);
    void shutdown();
    bool sendFrame(AVFrame *frame);
    bool isActive() const { return active.load(); }
    
    // Update performance stats (call from main app)
    void updateStats(float bitrateMbps, float packetLoss, uint32_t dropped, uint32_t targetFps, uint32_t actualFps);
    
    // Get profiling results (call after 10 seconds)
    double getAvgWriteTimeUs() const { return profileFrameCount > 0 ? (double)profileTotalUs / profileFrameCount : 0; }
    uint64_t getProfileFrameCount() const { return profileFrameCount; }
    bool isProfilingDone() const { return profilingDone; }

private:
    FrameSharing() : active(false), frameNumber(0), swsCtx(nullptr)
#ifdef Q_OS_WIN
        , hMap(nullptr), hEvent(nullptr), mem(nullptr)
        , profilingDone(false), profileFrameCount(0), profileTotalUs(0)
#endif
    {
#ifdef Q_OS_WIN
        perfFreq.QuadPart = 0;
        profileStartTime.QuadPart = 0;
#endif
    }
    ~FrameSharing() { shutdown(); }
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    SwsContext *swsCtx;
    
#ifdef Q_OS_WIN
    HANDLE hMap, hEvent;
    void *mem;
    bool profilingDone;
    uint64_t profileFrameCount;
    uint64_t profileTotalUs;
    LARGE_INTEGER perfFreq;
    LARGE_INTEGER profileStartTime;
#endif
};

#endif // CHIAKI_FRAMESHARING_H
