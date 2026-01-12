// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// GPU Texture Sharing - v5.0 (DirectX Zero-Copy)

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <atomic>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

// Shared header - keep in sync with C# client
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;          // 0x4B414843 ("CHAK")
    uint32_t version;        // 5
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;         // 0=CPU BGRA, 1=GPU Texture
    uint64_t timestamp;
    uint64_t frameNumber;
    uint32_t dataSize;
    volatile uint32_t ready;
    uint64_t sharedHandle;   // HANDLE to shared D3D11 texture
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

private:
    FrameSharing();
    ~FrameSharing() { shutdown(); }
    
    std::atomic<bool> active;
    uint64_t frameNumber;
    int w, h;
    SwsContext *swsCtx;
    bool gpuMode;
    
#ifdef _WIN32
    bool initGpu();
    
    // Shared memory (header + CPU fallback)
    HANDLE hMap, hEvent;
    void *mem;
    
    // D3D11 GPU sharing
    ID3D11Device *d3dDevice;
    ID3D11DeviceContext *d3dContext;
    ID3D11Texture2D *sharedTexture;
    ID3D11Texture2D *stagingTexture;
    HANDLE sharedHandle;
#endif
};

#define SHARE_FRAME(f) do { if(FrameSharing::instance().isActive()) FrameSharing::instance().sendFrame(f); } while(0)

#endif // CHIAKI_FRAMESHARING_H
