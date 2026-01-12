// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Ultra-Fast Frame Sharing - v3.1 with Async Processing
// Zero impact on main Chiaki thread

#include "framesharing.h"
#include <cstring>

FrameSharing::FrameSharing()
{
#ifdef _WIN32
    perfFreq.QuadPart = 0;
    QueryPerformanceFrequency(&perfFreq);
#endif
}

#ifdef _WIN32

uint8_t* FrameSharing::getBufferPtr(int bufferIndex)
{
    if (!mem) return nullptr;
    size_t singleBufferSize = (size_t)maxW * maxH * 4;
    size_t offset = sizeof(FrameSharingHeader) + (bufferIndex * singleBufferSize);
    return static_cast<uint8_t*>(mem) + offset;
}

void FrameSharing::switchWriteBuffer()
{
    currentWriteBuffer = 1 - currentWriteBuffer;
}

#endif

bool FrameSharing::initialize(int maxWidth, int maxHeight)
{
    if (active.load(std::memory_order_acquire)) 
        shutdown();
    
    maxW = maxWidth;
    maxH = maxHeight;
    frameNumber = 0;
    profileFrameCount = 0;
    profileTotalUs = 0;
    
#ifdef _WIN32
    currentWriteBuffer = 0;
    
    // Double buffer: header + buffer0 + buffer1
    size_t singleBufferSize = (size_t)maxW * maxH * 4;
    size_t totalSize = sizeof(FrameSharingHeader) + (singleBufferSize * 2);
    
    // Create shared memory
    hMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE, 
        nullptr, 
        PAGE_READWRITE,
        (DWORD)(totalSize >> 32), 
        (DWORD)(totalSize & 0xFFFFFFFF), 
        L"ChiakiFrameShare"
    );
    
    if (!hMap) {
        return false;
    }
    
    mem = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if (!mem) {
        CloseHandle(hMap);
        hMap = nullptr;
        return false;
    }
    
    // Initialize header
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    memset(hdr, 0, sizeof(FrameSharingHeader));
    hdr->magic = 0x4B414843;  // "CHAK"
    hdr->version = 3;
    hdr->maxWidth = maxW;
    hdr->maxHeight = maxH;
    hdr->writeBuffer = 0;
    hdr->readyBuffer = -1;
    hdr->producerLock = 0;
    hdr->consumerLock = 0;
    hdr->totalFramesWritten = 0;
    hdr->totalFramesRead = 0;
    hdr->droppedFrames = 0;
    
    MemoryBarrier();
    
    // Create event for signaling
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (!hEvent) {
        UnmapViewOfFile(mem);
        mem = nullptr;
        CloseHandle(hMap);
        hMap = nullptr;
        return false;
    }
    
    active.store(true, std::memory_order_release);
    
    // Start worker thread
    workerRunning.store(true, std::memory_order_release);
    worker = std::thread(&FrameSharing::workerThread, this);
    
    return true;
    
#else
    return false;
#endif
}

void FrameSharing::shutdown()
{
    // Stop worker thread first
    if (workerRunning.load(std::memory_order_acquire)) {
        workerRunning.store(false, std::memory_order_release);
        queueCV.notify_all();
        
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    active.store(false, std::memory_order_release);
    
    // Clear queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!frameQueue.empty()) {
            AVFrame *f = frameQueue.front();
            frameQueue.pop();
            av_frame_free(&f);
        }
    }
    
#ifdef _WIN32
    if (mem) {
        auto *hdr = static_cast<FrameSharingHeader*>(mem);
        hdr->readyBuffer = -1;
        MemoryBarrier();
        
        UnmapViewOfFile(mem);
        mem = nullptr;
    }
    if (hMap) {
        CloseHandle(hMap);
        hMap = nullptr;
    }
    if (hEvent) {
        CloseHandle(hEvent);
        hEvent = nullptr;
    }
#endif
    
    if (swsCtx) {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }
}

// Called from Chiaki's main thread - returns immediately
void FrameSharing::queueFrame(AVFrame *frame)
{
    if (!active.load(std::memory_order_acquire) || !frame || !frame->data[0])
        return;
    
    // Make a copy of the frame (reference counted - very fast)
    AVFrame *frameCopy = av_frame_clone(frame);
    if (!frameCopy)
        return;
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        // If queue is full, drop oldest frame (keep latest)
        while (frameQueue.size() >= MAX_QUEUE_SIZE) {
            AVFrame *old = frameQueue.front();
            frameQueue.pop();
            av_frame_free(&old);
            queuedFrames.fetch_sub(1, std::memory_order_relaxed);
        }
        
        frameQueue.push(frameCopy);
        queuedFrames.fetch_add(1, std::memory_order_relaxed);
    }
    
    queueCV.notify_one();
}

// Worker thread - processes frames asynchronously
void FrameSharing::workerThread()
{
#ifdef _WIN32
    // Set thread priority to below normal to avoid affecting Chiaki
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
    
    while (workerRunning.load(std::memory_order_acquire)) {
        AVFrame *frame = nullptr;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            
            // Wait for frame or shutdown
            queueCV.wait(lock, [this] {
                return !frameQueue.empty() || !workerRunning.load(std::memory_order_acquire);
            });
            
            if (!workerRunning.load(std::memory_order_acquire))
                break;
            
            if (!frameQueue.empty()) {
                frame = frameQueue.front();
                frameQueue.pop();
                queuedFrames.fetch_sub(1, std::memory_order_relaxed);
            }
        }
        
        if (frame) {
            processFrame(frame);
            av_frame_free(&frame);
        }
    }
}

// Actual processing - runs on worker thread
bool FrameSharing::processFrame(AVFrame *frame)
{
    if (!frame || !frame->data[0] || !mem)
        return false;
    
    int fw = frame->width;
    int fh = frame->height;
    
    if (fw > maxW || fh > maxH || fw < 16 || fh < 16)
        return false;
    
#ifdef _WIN32
    auto *hdr = static_cast<FrameSharingHeader*>(mem);
    
    int writeIdx = currentWriteBuffer;
    uint8_t *dst = getBufferPtr(writeIdx);
    
    if (!dst) return false;
    
    // Setup SWS context (cached)
    swsCtx = sws_getCachedContext(
        swsCtx,
        fw, fh, (AVPixelFormat)frame->format,
        fw, fh, AV_PIX_FMT_BGRA,
        SWS_POINT,  // Fastest - no interpolation
        nullptr, nullptr, nullptr
    );
    
    if (!swsCtx) return false;
    
    int stride = fw * 4;
    uint8_t *dstSlice[4] = { dst, nullptr, nullptr, nullptr };
    int dstStride[4] = { stride, 0, 0, 0 };
    
    // Profiling (first 100 frames only)
    LARGE_INTEGER t1, t2;
    bool shouldProfile = profileFrameCount < 100;
    if (shouldProfile) QueryPerformanceCounter(&t1);
    
    // Convert frame to BGRA
    int result = sws_scale(swsCtx, frame->data, frame->linesize, 0, fh, dstSlice, dstStride);
    if (result != fh)
        return false;
    
    if (shouldProfile) {
        QueryPerformanceCounter(&t2);
        profileTotalUs += ((t2.QuadPart - t1.QuadPart) * 1000000) / perfFreq.QuadPart;
        profileFrameCount++;
    }
    
    frameNumber++;
    
    // Update buffer metadata
    uint32_t dataSize = stride * fh;
    LARGE_INTEGER timestamp;
    QueryPerformanceCounter(&timestamp);
    uint64_t timestampUs = (timestamp.QuadPart * 1000000) / perfFreq.QuadPart;
    
    if (writeIdx == 0) {
        hdr->width0 = fw;
        hdr->height0 = fh;
        hdr->stride0 = stride;
        hdr->dataSize0 = dataSize;
        hdr->timestamp0 = timestampUs;
        hdr->frameNumber0 = frameNumber;
    } else {
        hdr->width1 = fw;
        hdr->height1 = fh;
        hdr->stride1 = stride;
        hdr->dataSize1 = dataSize;
        hdr->timestamp1 = timestampUs;
        hdr->frameNumber1 = frameNumber;
    }
    
    MemoryBarrier();
    
    // Track dropped frames
    int32_t prevReady = hdr->readyBuffer;
    if (prevReady != -1 && prevReady != writeIdx) {
        InterlockedIncrement64((volatile LONG64*)&hdr->droppedFrames);
    }
    
    // Mark buffer as ready
    InterlockedExchange((volatile LONG*)&hdr->readyBuffer, writeIdx);
    InterlockedIncrement64((volatile LONG64*)&hdr->totalFramesWritten);
    
    switchWriteBuffer();
    
    // Signal consumer
    SetEvent(hEvent);
    
    return true;
    
#else
    return false;
#endif
}

uint64_t FrameSharing::getTotalFramesWritten() const
{
#ifdef _WIN32
    if (!mem) return 0;
    auto *hdr = static_cast<const FrameSharingHeader*>(mem);
    return hdr->totalFramesWritten;
#else
    return 0;
#endif
}

uint64_t FrameSharing::getDroppedFrames() const
{
#ifdef _WIN32
    if (!mem) return 0;
    auto *hdr = static_cast<const FrameSharingHeader*>(mem);
    return hdr->droppedFrames;
#else
    return 0;
#endif
}
