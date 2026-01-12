// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v2.3

#include "framesharing.h"

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    
#ifdef Q_OS_WIN
    // Reset profiling
    profilingDone = false;
    profileFrameCount = 0;
    profileTotalUs = 0;
    QueryPerformanceFrequency(&perfFreq);
    
    size_t dataSize = (size_t)w * h * 4;
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
    hdr->version = 2;
    hdr->format = 0;
    
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        qWarning() << "FrameSharing: CreateEvent failed:" << GetLastError();
        UnmapViewOfFile(mem); mem = nullptr;
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
    
    QueryPerformanceCounter(&profileStartTime);
#else
    qWarning() << "FrameSharing: Only supported on Windows";
    return false;
#endif

    active.store(true);
    qInfo() << "FrameSharing: Ready!" << w << "x" << h;
    return true;
}

void FrameSharing::shutdown()
{
    active.store(false);
    
#ifdef Q_OS_WIN
    // Log profiling results if collected
    if (profileFrameCount > 0 && !profilingDone) {
        double avgUs = (double)profileTotalUs / profileFrameCount;
        qInfo() << "FrameSharing: Profiling -" << profileFrameCount << "frames, avg write time:" << avgUs << "us (" << (avgUs/1000.0) << "ms)";
    }
    
    if (mem) { UnmapViewOfFile(mem); mem = nullptr; }
    if (hMap) { CloseHandle(hMap); hMap = nullptr; }
    if (hEvent) { CloseHandle(hEvent); hEvent = nullptr; }
#endif
    
    if (swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }
    
    if (frameNumber > 0)
        qInfo() << "FrameSharing: Total sent:" << frameNumber << "frames";
}

bool FrameSharing::sendFrame(AVFrame *frame)
{
    if (!active.load() || !frame || !frame->data[0] || !mem) 
        return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    if (fw > w || fh > h)
        return false;
    
#ifdef Q_OS_WIN
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    swsCtx = sws_getCachedContext(swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_BGRA,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) return false;
    
    int stride = fw * 4;
    uint8_t *dstSlice[4] = { dst, nullptr, nullptr, nullptr };
    int dstStride[4] = { stride, 0, 0, 0 };
    
    // Profile write time for first 10 seconds only
    LARGE_INTEGER t1, t2;
    bool shouldProfile = !profilingDone;
    if (shouldProfile) QueryPerformanceCounter(&t1);
    
    if (sws_scale(swsCtx, frame->data, frame->linesize, 0, fh,
                  dstSlice, dstStride) != fh)
        return false;
    
    if (shouldProfile) {
        QueryPerformanceCounter(&t2);
        profileTotalUs += ((t2.QuadPart - t1.QuadPart) * 1000000) / perfFreq.QuadPart;
        profileFrameCount++;
        
        // Check if 10 seconds passed
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsedSec = (double)(now.QuadPart - profileStartTime.QuadPart) / perfFreq.QuadPart;
        
        if (elapsedSec >= 10.0) {
            double avgUs = (double)profileTotalUs / profileFrameCount;
            double avgMs = avgUs / 1000.0;
            int dataSize = stride * fh;
            double mbps = (dataSize / 1024.0 / 1024.0) / (avgMs / 1000.0);
            
            qInfo() << "=== FrameSharing Profiling (10s) ===";
            qInfo() << "  Resolution:" << fw << "x" << fh;
            qInfo() << "  Frames:" << profileFrameCount;
            qInfo() << "  Avg write:" << avgMs << "ms (" << avgUs << "us)";
            qInfo() << "  Speed:" << mbps << "MB/s";
            qInfo() << "  Frame size:" << (dataSize/1024) << "KB";
            qInfo() << "====================================";
            
            profilingDone = true;
        }
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
    
    // Log resolution change only
    static int lastW = 0, lastH = 0;
    if (fw != lastW || fh != lastH) {
        qInfo() << "FrameSharing:" << fw << "x" << fh;
        lastW = fw; lastH = fh;
    }
    
    return true;
#else
    return false;
#endif
}
