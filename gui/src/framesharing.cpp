// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Simple & Fast Frame Sharing - v3.0 (Raw NV12 - No Conversion)

#include "framesharing.h"
#include <cstring>

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    
#ifdef _WIN32
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
    hdr->version = 3;  // v3 = Raw NV12
    hdr->format = 1;   // 1 = NV12
    
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        UnmapViewOfFile(mem); mem = nullptr;
        CloseHandle(hMap); hMap = nullptr;
        return false;
    }
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
    
    // Copy Y plane
    int ySize = fw * fh;
    if (frame->linesize[0] == fw) {
        memcpy(dst, frame->data[0], ySize);
    } else {
        for (int y = 0; y < fh; y++)
            memcpy(dst + y * fw, frame->data[0] + y * frame->linesize[0], fw);
    }
    
    // Copy UV plane (NV12: interleaved UV)
    uint8_t *uvDst = dst + ySize;
    int uvHeight = fh / 2;
    int uvWidth = fw;
    
    if (frame->data[1]) {
        // NV12 format - UV interleaved
        if (frame->linesize[1] == uvWidth) {
            memcpy(uvDst, frame->data[1], uvWidth * uvHeight);
        } else {
            for (int y = 0; y < uvHeight; y++)
                memcpy(uvDst + y * uvWidth, frame->data[1] + y * frame->linesize[1], uvWidth);
        }
    }
    
    frameNumber++;
    hdr->width = fw;
    hdr->height = fh;
    hdr->stride = fw;  // Y stride
    hdr->dataSize = ySize + uvWidth * uvHeight;
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
