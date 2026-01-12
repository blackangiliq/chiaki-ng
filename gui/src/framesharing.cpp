// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.2 (Dynamic Resolution)

#include "framesharing.h"

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    
#ifdef Q_OS_WIN
    size_t dataSize = (size_t)w * h * 4; // BGRA - max buffer size
    size_t totalSize = sizeof(FrameSharingHeader) + dataSize;
    
    // Create shared memory with max size
    hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                               0, (DWORD)totalSize, L"ChiakiFrameShare");
    if (!hMap) {
        qWarning() << "FrameSharing: CreateFileMapping failed:" << GetLastError();
        return false;
    }
    
    mem = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if (!mem) {
        qWarning() << "FrameSharing: MapViewOfFile failed:" << GetLastError();
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
    
    // Initialize header (will be updated per frame)
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    memset(hdr, 0, sizeof(FrameSharingHeader));
    hdr->magic = 0x4B414843;
    hdr->version = 2;
    hdr->format = 0; // BGRA
    
    // Create event for signaling
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        qWarning() << "FrameSharing: CreateEvent failed:" << GetLastError();
        UnmapViewOfFile(mem); mem = nullptr;
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
#else
    qWarning() << "FrameSharing: Only supported on Windows";
    return false;
#endif

    active.store(true);
    qInfo() << "FrameSharing: Ready! Max:" << w << "x" << h << "BGRA";
    return true;
}

void FrameSharing::shutdown()
{
    active.store(false);
    
#ifdef Q_OS_WIN
    if (mem) { UnmapViewOfFile(mem); mem = nullptr; }
    if (hMap) { CloseHandle(hMap); hMap = nullptr; }
    if (hEvent) { CloseHandle(hEvent); hEvent = nullptr; }
#endif
    
    if (swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }
    
    if (frameNumber > 0)
        qInfo() << "FrameSharing: Sent" << frameNumber << "frames";
}

bool FrameSharing::sendFrame(AVFrame *frame)
{
    // Fast path checks
    if (!active.load() || !frame || !frame->data[0] || !mem) 
        return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    // Check if frame fits in buffer
    if (fw > w || fh > h) {
        qWarning() << "FrameSharing: Frame" << fw << "x" << fh << "exceeds max" << w << "x" << h;
        return false;
    }
    
#ifdef Q_OS_WIN
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    // Get/create converter - output at FRAME's native resolution (no scaling!)
    swsCtx = sws_getCachedContext(swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_BGRA,  // Same size - just format conversion
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) return false;
    
    // Convert directly to shared memory
    int stride = fw * 4;
    uint8_t *dstSlice[4] = { dst, nullptr, nullptr, nullptr };
    int dstStride[4] = { stride, 0, 0, 0 };
    
    if (sws_scale(swsCtx, frame->data, frame->linesize, 0, fh,
                  dstSlice, dstStride) != fh)
        return false;
    
    // Update header with ACTUAL frame dimensions
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
    
    // Log on first frame or resolution change
    static int lastW = 0, lastH = 0;
    if (frameNumber == 1 || fw != lastW || fh != lastH) {
        qInfo() << "FrameSharing: Streaming" << fw << "x" << fh << "format:" << frame->format;
        lastW = fw;
        lastH = fh;
    }
    
    return true;
#else
    return false;
#endif
}
