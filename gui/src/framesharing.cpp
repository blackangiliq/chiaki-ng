// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Fast Frame Sharing - v4.0 (CPU Optimized)

#include "framesharing.h"
#include <cstring>

FrameSharing::FrameSharing() 
    : active(false), frameNumber(0), w(0), h(0), swsCtx(nullptr)
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

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    
#ifdef _WIN32
    profilingDone = false;
    profileFrameCount = 0;
    profileTotalUs = 0;
    QueryPerformanceFrequency(&perfFreq);
    
    size_t dataSize = (size_t)w * h * 4;
    size_t totalSize = sizeof(FrameSharingHeader) + dataSize;
    
    hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                               0, (DWORD)totalSize, L"ChiakiFrameShare");
    if (!hMap) return false;
    
    mem = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if (!mem) {
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
    
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    memset(hdr, 0, sizeof(FrameSharingHeader));
    hdr->magic = 0x4B414843;
    hdr->version = 4;
    hdr->format = 0; // BGRA
    
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        UnmapViewOfFile(mem); mem = nullptr;
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
    
    QueryPerformanceCounter(&profileStartTime);
    active.store(true);
    return true;
#else
    return false;
#endif
}

void FrameSharing::shutdown()
{
    active.store(false);
    
#ifdef _WIN32
    if (mem) { UnmapViewOfFile(mem); mem = nullptr; }
    if (hMap) { CloseHandle(hMap); hMap = nullptr; }
    if (hEvent) { CloseHandle(hEvent); hEvent = nullptr; }
#endif
    
    if (swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }
}

bool FrameSharing::sendFrame(AVFrame *frame)
{
    if (!active.load() || !frame || !frame->data[0]) 
        return false;
    
#ifdef _WIN32
    if (!mem) return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    if (fw > w || fh > h)
        return false;
    
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    LARGE_INTEGER t1, t2;
    bool shouldProfile = !profilingDone;
    if (shouldProfile) QueryPerformanceCounter(&t1);
    
    // Convert to BGRA
    swsCtx = sws_getCachedContext(swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_BGRA,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) return false;
    
    int stride = fw * 4;
    uint8_t *dstSlice[4] = { dst, nullptr, nullptr, nullptr };
    int dstStride[4] = { stride, 0, 0, 0 };
    
    if (sws_scale(swsCtx, frame->data, frame->linesize, 0, fh,
                  dstSlice, dstStride) != fh)
        return false;
    
    if (shouldProfile) {
        QueryPerformanceCounter(&t2);
        profileTotalUs += ((t2.QuadPart - t1.QuadPart) * 1000000) / perfFreq.QuadPart;
        profileFrameCount++;
        
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if ((double)(now.QuadPart - profileStartTime.QuadPart) / perfFreq.QuadPart >= 10.0)
            profilingDone = true;
    }
    
    frameNumber++;
    hdr->width = fw;
    hdr->height = fh;
    hdr->stride = stride;
    hdr->dataSize = stride * fh;
    hdr->frameNumber = frameNumber;
    hdr->timestamp = GetTickCount64();
    
    MemoryBarrier();
    hdr->ready = 1;
    SetEvent(hEvent);
    
    return true;
#else
    return false;
#endif
}
