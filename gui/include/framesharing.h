// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v3.0 (NV12 Direct)

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef _WIN32
#include <windows.h>
#endif

// Format constants
#define FRAME_FORMAT_BGRA  0
#define FRAME_FORMAT_NV12  1

#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;          // 0x4B414843 "CHAK"
    uint32_t version;        // 3
    uint32_t width;
    uint32_t height;
    uint32_t strideY;        // Y plane stride (was just 'stride')
    uint32_t strideUV;       // UV plane stride (new)
    uint32_t format;         // 0=BGRA, 1=NV12
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSizeY;      // Y plane size
    uint32_t dataSizeUV;     // UV plane size
    volatile uint32_t ready;
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
    
    // Get profiling results (call after 10 seconds)
    double getAvgWriteTimeUs() const { return profileFrameCount > 0 ? (double)profileTotalUs / profileFrameCount : 0; }
    uint64_t getProfileFrameCount() const { return profileFrameCount; }
    bool isProfilingDone() const { return profilingDone; }

private:
    FrameSharing() : active(false), frameNumber(0), swsCtx(nullptr)
#ifdef _WIN32
        , hMap(nullptr), hEvent(nullptr), mem(nullptr)
        , profilingDone(false), profileFrameCount(0), profileTotalUs(0)
#endif
    {
#ifdef _WIN32
        perfFreq.QuadPart = 0;
        profileStartTime.QuadPart = 0;
#endif
    }
    ~FrameSharing() { shutdown(); }
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    SwsContext *swsCtx;
    
#ifdef _WIN32
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
