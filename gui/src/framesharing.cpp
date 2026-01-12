// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v3.0 (NV12 Direct)

#include "framesharing.h"
#include <cstring>

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
    
    // NV12: Y plane (w*h) + UV plane (w*h/2) = 1.5 * w * h
    size_t dataSize = (size_t)w * h * 3 / 2;
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
    hdr->version = 3;
    hdr->format = FRAME_FORMAT_NV12;
    
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        UnmapViewOfFile(mem); mem = nullptr;
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
    
    QueryPerformanceCounter(&profileStartTime);
#else
    return false;
#endif

    active.store(true);
    return true;
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
    if (!active.load() || !frame || !frame->data[0] || !mem) 
        return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    if (fw > w || fh > h)
        return false;
    
#ifdef _WIN32
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    LARGE_INTEGER t1, t2;
    bool shouldProfile = !profilingDone;
    if (shouldProfile) QueryPerformanceCounter(&t1);
    
    AVPixelFormat srcFormat = (AVPixelFormat)frame->format;
    
    // Check if frame is NV12 - copy directly without conversion!
    if (srcFormat == AV_PIX_FMT_NV12) {
        // Copy Y plane
        int ySize = fw * fh;
        if (frame->linesize[0] == fw) {
            memcpy(dst, frame->data[0], ySize);
        } else {
            // Handle stride mismatch
            for (int y = 0; y < fh; y++) {
                memcpy(dst + y * fw, frame->data[0] + y * frame->linesize[0], fw);
            }
        }
        
        // Copy UV plane (interleaved)
        int uvSize = fw * fh / 2;
        uint8_t *uvDst = dst + ySize;
        if (frame->linesize[1] == fw) {
            memcpy(uvDst, frame->data[1], uvSize);
        } else {
            for (int y = 0; y < fh / 2; y++) {
                memcpy(uvDst + y * fw, frame->data[1] + y * frame->linesize[1], fw);
            }
        }
        
        hdr->strideY = fw;
        hdr->strideUV = fw;
        hdr->dataSizeY = ySize;
        hdr->dataSizeUV = uvSize;
        hdr->format = FRAME_FORMAT_NV12;
    }
    else {
        // Convert other formats to NV12
        swsCtx = sws_getCachedContext(swsCtx,
            fw, fh, srcFormat,
            fw, fh, AV_PIX_FMT_NV12,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        
        if (!swsCtx) return false;
        
        int ySize = fw * fh;
        int uvSize = fw * fh / 2;
        uint8_t *dstSlice[4] = { dst, dst + ySize, nullptr, nullptr };
        int dstStride[4] = { fw, fw, 0, 0 };
        
        if (sws_scale(swsCtx, frame->data, frame->linesize, 0, fh,
                      dstSlice, dstStride) != fh)
            return false;
        
        hdr->strideY = fw;
        hdr->strideUV = fw;
        hdr->dataSizeY = ySize;
        hdr->dataSizeUV = uvSize;
        hdr->format = FRAME_FORMAT_NV12;
    }
    
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
