// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.0

#include "framesharing.h"

bool FrameSharing::initialize(int width, int height)
{
    QMutexLocker lock(&mutex);
    
    if (active.load()) shutdown();
    
    w = width;
    h = height;
    frameNumber = 0;
    
#ifdef Q_OS_WIN
    size_t dataSize = (size_t)w * h * 4; // BGRA
    size_t totalSize = sizeof(FrameSharingHeader) + dataSize;
    
    // Create shared memory
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
    
    // Initialize header
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    memset(hdr, 0, sizeof(FrameSharingHeader));
    hdr->magic = 0x4B414843;
    hdr->version = 2;
    hdr->width = w;
    hdr->height = h;
    hdr->stride = w * 4;
    hdr->format = 0; // BGRA
    hdr->dataSize = (uint32_t)dataSize;
    
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
    qInfo() << "FrameSharing: Ready!" << w << "x" << h << "BGRA";
    return true;
}

void FrameSharing::shutdown()
{
    QMutexLocker lock(&mutex);
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
    if (!active.load() || !frame || !frame->data[0]) return false;
    
    QMutexLocker lock(&mutex);
    if (!active.load()) return false;
    
#ifdef Q_OS_WIN
    if (!mem) return false;
    
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    // Get/create converter (cached)
    swsCtx = sws_getCachedContext(swsCtx,
        frame->width, frame->height, (AVPixelFormat)frame->format,
        w, h, AV_PIX_FMT_BGRA,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) {
        qWarning() << "FrameSharing: Can't convert format" << frame->format;
        return false;
    }
    
    // Convert directly to shared memory
    uint8_t *dstSlice[4] = { dst, nullptr, nullptr, nullptr };
    int dstStride[4] = { w * 4, 0, 0, 0 };
    
    int ret = sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
                        dstSlice, dstStride);
    if (ret != h) return false;
    
    // Update header
    frameNumber++;
    hdr->timestamp = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    hdr->frameNumber = frameNumber;
    MemoryBarrier();
    hdr->ready = 1;
    
    // Signal consumer
    if (hEvent) SetEvent(hEvent);
    
    // Log first frame and stats every 5 seconds (300 frames @ 60fps)
    if (frameNumber == 1) {
        qInfo() << "FrameSharing: First frame sent! Format:" << frame->format;
    } else if (frameNumber % 300 == 0) {
        qInfo() << "FrameSharing: Sent" << frameNumber << "frames";
    }
    
    return true;
#else
    return false;
#endif
}
