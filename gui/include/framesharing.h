// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// GPU Texture Sharing - v3.0

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libswscale/swscale.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

// Shared header for CPU fallback mode
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;          // 0x4B414843
    uint32_t version;        // 3
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;         // 0=BGRA, 1=GPU_TEXTURE
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;
    volatile uint32_t ready;
    uint64_t sharedHandle;   // For GPU mode: HANDLE to shared texture
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
    bool isGpuMode() const { return gpuMode; }
    
    double getAvgWriteTimeUs() const { return profileFrameCount > 0 ? (double)profileTotalUs / profileFrameCount : 0; }
    uint64_t getProfileFrameCount() const { return profileFrameCount; }

private:
    FrameSharing();
    ~FrameSharing() { shutdown(); }
    
    bool initD3D11();
    bool sendFrameGpu(AVFrame *frame);
    bool sendFrameCpu(AVFrame *frame);
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    SwsContext *swsCtx;
    bool gpuMode;
    
#ifdef Q_OS_WIN
    // Shared memory (CPU fallback)
    HANDLE hMap, hEvent;
    void *mem;
    
    // D3D11 GPU sharing
    ID3D11Device *d3dDevice;
    ID3D11DeviceContext *d3dContext;
    ID3D11Texture2D *sharedTexture;
    HANDLE sharedHandle;
    
    // Profiling
    bool profilingDone;
    uint64_t profileFrameCount;
    uint64_t profileTotalUs;
    LARGE_INTEGER perfFreq;
    LARGE_INTEGER profileStartTime;
#endif
};

#endif // CHIAKI_FRAMESHARING_H
