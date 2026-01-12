// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// GPU Texture Sharing - v3.0

#include "framesharing.h"
#include <cstring>

FrameSharing::FrameSharing() 
    : active(false), frameNumber(0), w(0), h(0), swsCtx(nullptr), gpuMode(false)
#ifdef _WIN32
    , hMap(nullptr), hEvent(nullptr), mem(nullptr)
    , d3dDevice(nullptr), d3dContext(nullptr), sharedTexture(nullptr), sharedHandle(nullptr)
    , profilingDone(false), profileFrameCount(0), profileTotalUs(0)
#endif
{
#ifdef _WIN32
    perfFreq.QuadPart = 0;
    profileStartTime.QuadPart = 0;
#endif
}

#ifdef _WIN32
bool FrameSharing::initD3D11()
{
    D3D_FEATURE_LEVEL featureLevel;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice, &featureLevel, &d3dContext);
    
    if (FAILED(hr)) return false;
    
    // Create shared texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    
    hr = d3dDevice->CreateTexture2D(&desc, nullptr, &sharedTexture);
    if (FAILED(hr)) {
        d3dDevice->Release(); d3dDevice = nullptr;
        d3dContext->Release(); d3dContext = nullptr;
        return false;
    }
    
    // Get shared handle
    IDXGIResource *dxgiResource = nullptr;
    hr = sharedTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    if (SUCCEEDED(hr)) {
        dxgiResource->GetSharedHandle(&sharedHandle);
        dxgiResource->Release();
    }
    
    if (!sharedHandle) {
        sharedTexture->Release(); sharedTexture = nullptr;
        d3dDevice->Release(); d3dDevice = nullptr;
        d3dContext->Release(); d3dContext = nullptr;
        return false;
    }
    
    return true;
}

bool FrameSharing::sendFrameGpu(AVFrame *frame)
{
    if (!d3dDevice || !sharedTexture) return false;
    
    // Check if frame is D3D11 hardware frame (format 53 = AV_PIX_FMT_D3D11)
    if (frame->format == 53) {
        // Get D3D11 texture from AVFrame
        ID3D11Texture2D *srcTexture = (ID3D11Texture2D*)frame->data[0];
        int srcIndex = (int)(intptr_t)frame->data[1];
        
        if (srcTexture) {
            // Copy texture to shared texture (GPU to GPU - very fast!)
            d3dContext->CopySubresourceRegion(
                sharedTexture, 0, 0, 0, 0,
                srcTexture, srcIndex, nullptr);
            d3dContext->Flush();
            return true;
        }
    }
    
    // Not a D3D11 frame - fall back to CPU copy to GPU
    D3D11_TEXTURE2D_DESC desc;
    sharedTexture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    ID3D11Texture2D *stagingTexture = nullptr;
    if (FAILED(d3dDevice->CreateTexture2D(&desc, nullptr, &stagingTexture)))
        return false;
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(d3dContext->Map(stagingTexture, 0, D3D11_MAP_WRITE, 0, &mapped))) {
        stagingTexture->Release();
        return false;
    }
    
    // Convert frame to BGRA and copy to staging texture
    int fw = frame->width;
    int fh = frame->height;
    
    swsCtx = sws_getCachedContext(swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_BGRA,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsCtx) {
        d3dContext->Unmap(stagingTexture, 0);
        stagingTexture->Release();
        return false;
    }
    
    uint8_t *dstSlice[4] = { (uint8_t*)mapped.pData, nullptr, nullptr, nullptr };
    int dstStride[4] = { (int)mapped.RowPitch, 0, 0, 0 };
    
    sws_scale(swsCtx, frame->data, frame->linesize, 0, fh, dstSlice, dstStride);
    
    d3dContext->Unmap(stagingTexture, 0);
    d3dContext->CopyResource(sharedTexture, stagingTexture);
    d3dContext->Flush();
    stagingTexture->Release();
    
    return true;
}

bool FrameSharing::sendFrameCpu(AVFrame *frame)
{
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    uint8_t *dst = static_cast<uint8_t*>(mem) + sizeof(FrameSharingHeader);
    
    int fw = frame->width;
    int fh = frame->height;
    
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
    
    hdr->stride = stride;
    hdr->dataSize = stride * fh;
    return true;
}
#endif

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load()) shutdown();
    
    w = maxWidth;
    h = maxHeight;
    frameNumber = 0;
    gpuMode = false;
    
#ifdef _WIN32
    profilingDone = false;
    profileFrameCount = 0;
    profileTotalUs = 0;
    QueryPerformanceFrequency(&perfFreq);
    
    // Try GPU mode first
    if (initD3D11()) {
        gpuMode = true;
    }
    
    // Always create shared memory (for header + CPU fallback)
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
    hdr->version = 3;
    hdr->format = gpuMode ? 1 : 0; // 1 = GPU texture, 0 = BGRA in memory
    hdr->sharedHandle = (uint64_t)(uintptr_t)sharedHandle;
    
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
    if (sharedTexture) { sharedTexture->Release(); sharedTexture = nullptr; }
    if (d3dContext) { d3dContext->Release(); d3dContext = nullptr; }
    if (d3dDevice) { d3dDevice->Release(); d3dDevice = nullptr; }
    sharedHandle = nullptr;
    
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
    
    LARGE_INTEGER t1, t2;
    bool shouldProfile = !profilingDone;
    if (shouldProfile) QueryPerformanceCounter(&t1);
    
    bool success;
    if (gpuMode) {
        success = sendFrameGpu(frame);
        if (!success) {
            // Fallback to CPU mode
            gpuMode = false;
            hdr->format = 0;
            success = sendFrameCpu(frame);
        }
    } else {
        success = sendFrameCpu(frame);
    }
    
    if (!success) return false;
    
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
    hdr->sharedHandle = (uint64_t)(uintptr_t)sharedHandle;
    
    MemoryBarrier();
    hdr->ready = 1;
    SetEvent(hEvent);
    
    return true;
#else
    return false;
#endif
}
