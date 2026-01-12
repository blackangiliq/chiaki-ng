// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.3

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <QDebug>
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
    uint32_t magic;          // 0x4B414843
    uint32_t version;        // 2
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;         // 0 = BGRA32
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;
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
    
    // Profiling (first 10 seconds only)
    bool profilingDone;
    uint64_t profileFrameCount;
    uint64_t profileTotalUs;
    LARGE_INTEGER perfFreq;
    LARGE_INTEGER profileStartTime;
#endif
};

#endif // CHIAKI_FRAMESHARING_H
