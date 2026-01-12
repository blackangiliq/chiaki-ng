// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v3.0 (Raw NV12 - No Conversion)

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
}

#ifdef _WIN32
#include <windows.h>
#endif

#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;       // 0x4B414843 "CHAK"
    uint32_t version;     // 3 = Raw NV12
    uint32_t width;
    uint32_t height;
    uint32_t stride;      // Y plane stride
    uint32_t format;      // 1 = NV12
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;    // Total: Y + UV
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
    FrameSharing() : active(false), frameNumber(0)
#ifdef _WIN32
        , hMap(nullptr), hEvent(nullptr), mem(nullptr)
#endif
    {}
    ~FrameSharing() { shutdown(); }
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    
#ifdef _WIN32
    HANDLE hMap, hEvent;
    void *mem;
#endif
};

#endif // CHIAKI_FRAMESHARING_H
