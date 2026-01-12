// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.0

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <QMutex>
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

// Shared header - keep in sync with C# client
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;          // 0x4B414843 ("CHAK")
    uint32_t version;        // 2
    uint32_t width;
    uint32_t height;
    uint32_t stride;         // width * 4 (BGRA)
    uint32_t format;         // 0 = BGRA32
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;
    volatile uint32_t ready; // 1 = new frame, 0 = consumed
};
#pragma pack(pop)

class FrameSharing
{
public:
    static FrameSharing& instance() {
        static FrameSharing inst;
        return inst;
    }
    
    bool initialize(int width, int height);
    void shutdown();
    bool sendFrame(AVFrame *frame);
    bool isActive() const { return active.load(); }

private:
    FrameSharing() : active(false), frameNumber(0), swsCtx(nullptr)
#ifdef Q_OS_WIN
        , hMap(nullptr), hEvent(nullptr), mem(nullptr)
#endif
    {}
    ~FrameSharing() { shutdown(); }
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    QMutex mutex;
    SwsContext *swsCtx;
    
#ifdef Q_OS_WIN
    HANDLE hMap, hEvent;
    void *mem;
#endif
};

// Simple macro for use in render code
#define SHARE_FRAME(f) do { if(FrameSharing::instance().isActive()) FrameSharing::instance().sendFrame(f); } while(0)

#endif // CHIAKI_FRAMESHARING_H
