// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v3.0 (NV12 - 62% smaller!)

#include "framesharing.h"

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    
#ifdef Q_OS_WIN
    // NV12: Y plane (w*h) + UV plane (w*h/2) = w*h*1.5
    size_t dataSize = (size_t)w * h * 3 / 2;
    size_t totalSize = sizeof(FrameSharingHeader) + dataSize;
    
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
    
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    memset(hdr, 0, sizeof(FrameSharingHeader));
    hdr->magic = 0x4B414843;
    hdr->version = 3;  // Version 3 = NV12 format
    hdr->format = 1;   // 1 = NV12
    
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
    qInfo() << "FrameSharing: Ready! Max:" << w << "x" << h << "NV12 (62% smaller!)";
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
    if (!active.load() || !frame || !frame->data[0] || !mem) 
        return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    if (fw > w || fh > h) {
        qWarning() << "FrameSharing: Frame" << fw << "x" << fh << "exceeds max" << w << "x" << h;
        return false;
    }
    
#ifdef Q_OS_WIN
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    // Convert to NV12 (much smaller than BGRA!)
    swsCtx = sws_getCachedContext(swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_NV12,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) return false;
    
    // NV12 layout: Y plane followed by interleaved UV plane
    int ySize = fw * fh;
    int uvSize = fw * fh / 2;
    
    uint8_t *dstSlice[4] = { dst, dst + ySize, nullptr, nullptr };
    int dstStride[4] = { fw, fw, 0, 0 };
    
    if (sws_scale(swsCtx, frame->data, frame->linesize, 0, fh,
                  dstSlice, dstStride) != fh)
        return false;
    
    // Update header
    frameNumber++;
    hdr->width = fw;
    hdr->height = fh;
    hdr->stride = fw;  // Y stride
    hdr->dataSize = ySize + uvSize;
    hdr->frameNumber = frameNumber;
    hdr->timestamp = GetTickCount64();
    
    MemoryBarrier();
    hdr->ready = 1;
    
    SetEvent(hEvent);
    
    static int lastW = 0, lastH = 0;
    if (frameNumber == 1 || fw != lastW || fh != lastH) {
        qInfo() << "FrameSharing: Streaming" << fw << "x" << fh << "NV12 (" << (ySize + uvSize) / 1024 << "KB)";
        lastW = fw;
        lastH = fh;
    }
    
    return true;
#else
    return false;
#endif
}
