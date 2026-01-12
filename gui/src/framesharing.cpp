// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "framesharing.h"
#include <QDebug>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

// ============================================================================
// FrameSharingLogger Implementation
// ============================================================================

FrameSharingLogger& FrameSharingLogger::instance()
{
    static FrameSharingLogger instance;
    return instance;
}

FrameSharingLogger::FrameSharingLogger()
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    
    logFilePath = logDir + "/framesharing.log";
    logFile.setFileName(logFilePath);
    
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "FrameSharing: Could not open log file:" << logFilePath;
    } else {
        log("INFO", "=== FrameSharing Log Started (v2 - Ring Buffer + HW Support) ===");
    }
}

FrameSharingLogger::~FrameSharingLogger()
{
    if (logFile.isOpen()) {
        log("INFO", "=== FrameSharing Log Ended ===");
        logFile.close();
    }
}

void FrameSharingLogger::log(const QString& level, const QString& message)
{
    QMutexLocker locker(&mutex);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3\n").arg(timestamp, level, message);
    
    if (logFile.isOpen()) {
        QTextStream stream(&logFile);
        stream << logLine;
        stream.flush();
    }
    
    if (level == "ERROR") {
        qCritical().noquote() << "[FrameSharing]" << message;
    } else if (level == "WARN") {
        qWarning().noquote() << "[FrameSharing]" << message;
    } else {
        qDebug().noquote() << "[FrameSharing]" << message;
    }
}

// ============================================================================
// FrameSharing Implementation
// ============================================================================

FrameSharing& FrameSharing::instance()
{
    static FrameSharing instance;
    return instance;
}

FrameSharing::FrameSharing()
    : active(false)
    , initialized(false)
    , frameWidth(0)
    , frameHeight(0)
    , frameCounter(0)
    , successFrameCounter(0)
    , hwFrameCounter(0)
    , lastErrorFormat(-1)
#ifdef Q_OS_WIN
    , hMapFile(nullptr)
    , hEvent(nullptr)
    , mappedMemory(nullptr)
    , header(nullptr)
    , frameDataArea(nullptr)
    , totalMappedSize(0)
    , singleFrameSize(0)
#endif
    , swFrame(nullptr)
    , swsContext(nullptr)
{
    FrameSharingLogger::instance().logInfo("FrameSharing instance created (v2)");
}

FrameSharing::~FrameSharing()
{
    shutdown();
    FrameSharingLogger::instance().logInfo("FrameSharing instance destroyed");
}

void FrameSharing::cleanup()
{
    FrameSharingLogger::instance().logInfo("Cleaning up FrameSharing resources...");
    
#ifdef Q_OS_WIN
    if (mappedMemory != nullptr) {
        UnmapViewOfFile(mappedMemory);
        mappedMemory = nullptr;
        header = nullptr;
        frameDataArea = nullptr;
    }
    
    if (hMapFile != nullptr) {
        CloseHandle(hMapFile);
        hMapFile = nullptr;
    }
    
    if (hEvent != nullptr) {
        CloseHandle(hEvent);
        hEvent = nullptr;
    }
#endif

    if (swsContext != nullptr) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    
    if (swFrame != nullptr) {
        av_frame_free(&swFrame);
        swFrame = nullptr;
    }
    
    frameWidth = 0;
    frameHeight = 0;
#ifdef Q_OS_WIN
    totalMappedSize = 0;
    singleFrameSize = 0;
#endif
    
    FrameSharingLogger::instance().logInfo("FrameSharing cleanup completed");
}

bool FrameSharing::initialize(int width, int height)
{
    QMutexLocker locker(&sendMutex);
    
    FrameSharingLogger::instance().logInfo(
        QString("Initializing FrameSharing v2: %1x%2 with Ring Buffer").arg(width).arg(height));
    
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        FrameSharingLogger::instance().logError(
            QString("Invalid dimensions: %1x%2").arg(width).arg(height));
        return false;
    }
    
    if (initialized.load()) {
        FrameSharingLogger::instance().logInfo("Reinitializing...");
        cleanup();
    }
    
    frameWidth = width;
    frameHeight = height;
    frameCounter = 0;
    successFrameCounter = 0;
    hwFrameCounter = 0;
    lastErrorFormat = -1;
    
#ifdef Q_OS_WIN
    // Calculate sizes
    int stride = width * 4; // BGRA = 4 bytes per pixel
    singleFrameSize = (size_t)stride * height;
    size_t ringBufferDataSize = singleFrameSize * FRAME_SHARING_RING_BUFFER_SIZE;
    totalMappedSize = sizeof(FrameSharingHeader) + ringBufferDataSize;
    
    FrameSharingLogger::instance().logInfo(
        QString("Memory layout: Header=%1, Frame=%2, RingBuffer=%3, Total=%4 bytes")
        .arg(sizeof(FrameSharingHeader))
        .arg(singleFrameSize)
        .arg(ringBufferDataSize)
        .arg(totalMappedSize));
    
    // Create file mapping
    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        (DWORD)totalMappedSize,
        L"ChiakiFrameShare"
    );
    
    if (hMapFile == nullptr) {
        FrameSharingLogger::instance().logError(
            QString("CreateFileMapping failed: %1").arg(GetLastError()));
        cleanup();
        return false;
    }
    
    // Map the view
    mappedMemory = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        totalMappedSize
    );
    
    if (mappedMemory == nullptr) {
        FrameSharingLogger::instance().logError(
            QString("MapViewOfFile failed: %1").arg(GetLastError()));
        cleanup();
        return false;
    }
    
    // Set up pointers
    header = static_cast<FrameSharingHeader*>(mappedMemory);
    frameDataArea = reinterpret_cast<uint8_t*>(mappedMemory) + sizeof(FrameSharingHeader);
    
    // Initialize header
    memset(header, 0, sizeof(FrameSharingHeader));
    header->magic = 0x4B414843; // "CHAK"
    header->version = 2;        // Version 2 with ring buffer
    header->width = width;
    header->height = height;
    header->stride = stride;
    header->format = 0; // BGRA32
    header->frameDataSize = (uint32_t)singleFrameSize;
    header->ringBufferSize = FRAME_SHARING_RING_BUFFER_SIZE;
    header->ringBufferFrameOffset = sizeof(FrameSharingHeader);
    header->writeIndex = 0;
    header->readIndex = 0;
    header->totalFrames = 0;
    header->timestamp = 0;
    
    for (int i = 0; i < FRAME_SHARING_RING_BUFFER_SIZE; i++) {
        header->frameReady[i] = 0;
        header->frameTimestamp[i] = 0;
        header->frameNumber[i] = 0;
    }
    
    // Create event for signaling
    hEvent = CreateEventW(nullptr, FALSE, FALSE, L"ChiakiFrameEvent");
    if (hEvent == nullptr) {
        FrameSharingLogger::instance().logError(
            QString("CreateEvent failed: %1").arg(GetLastError()));
        cleanup();
        return false;
    }
    
#else
    FrameSharingLogger::instance().logWarning("Frame sharing is only supported on Windows");
    return false;
#endif

    // Allocate software frame for hardware frame transfer
    swFrame = av_frame_alloc();
    if (swFrame == nullptr) {
        FrameSharingLogger::instance().logError("Failed to allocate software frame");
        cleanup();
        return false;
    }
    
    initialized.store(true);
    active.store(true);
    
    FrameSharingLogger::instance().logInfo("=============================================");
    FrameSharingLogger::instance().logInfo("  FRAME SHARING v2 INITIALIZED!");
    FrameSharingLogger::instance().logInfo("=============================================");
    FrameSharingLogger::instance().logInfo(QString("  Resolution: %1x%2 @ 60fps").arg(width).arg(height));
    FrameSharingLogger::instance().logInfo(QString("  Ring Buffer: %1 frames").arg(FRAME_SHARING_RING_BUFFER_SIZE));
    FrameSharingLogger::instance().logInfo(QString("  Frame size: %1 KB").arg(singleFrameSize / 1024));
    FrameSharingLogger::instance().logInfo("  Hardware frames: SUPPORTED (D3D11/DXVA2)");
    FrameSharingLogger::instance().logInfo("---------------------------------------------");
    FrameSharingLogger::instance().logInfo("  C# Client: Use ChiakiFrameReceiver v2");
    FrameSharingLogger::instance().logInfo("  Memory Name: ChiakiFrameShare");
    FrameSharingLogger::instance().logInfo("  Event Name: ChiakiFrameEvent");
    FrameSharingLogger::instance().logInfo("=============================================");
    
    return true;
}

void FrameSharing::shutdown()
{
    QMutexLocker locker(&sendMutex);
    
    if (!initialized.load()) {
        return;
    }
    
    double successRate = frameCounter > 0 ? (successFrameCounter * 100.0 / frameCounter) : 0;
    
    FrameSharingLogger::instance().logInfo("=============================================");
    FrameSharingLogger::instance().logInfo("  FRAME SHARING SESSION ENDED");
    FrameSharingLogger::instance().logInfo("=============================================");
    FrameSharingLogger::instance().logInfo(QString("  Total frames: %1").arg(frameCounter));
    FrameSharingLogger::instance().logInfo(QString("  Successfully shared: %1").arg(successFrameCounter));
    FrameSharingLogger::instance().logInfo(QString("  Hardware frames: %1").arg(hwFrameCounter));
    FrameSharingLogger::instance().logInfo(QString("  Success rate: %1%").arg(successRate, 0, 'f', 1));
    FrameSharingLogger::instance().logInfo("=============================================");
    
    active.store(false);
    initialized.store(false);
    
    cleanup();
}

bool FrameSharing::isHardwareFrame(AVFrame *frame)
{
    if (frame == nullptr) return false;
    if (frame->hw_frames_ctx != nullptr) return true;
    
    switch (frame->format) {
        case AV_PIX_FMT_D3D11:
        case AV_PIX_FMT_D3D11VA_VLD:
        case AV_PIX_FMT_DXVA2_VLD:
        case AV_PIX_FMT_VULKAN:
        case AV_PIX_FMT_VAAPI:
        case AV_PIX_FMT_CUDA:
        case AV_PIX_FMT_QSV:
        case AV_PIX_FMT_VDPAU:
        case AV_PIX_FMT_VIDEOTOOLBOX:
        case AV_PIX_FMT_MEDIACODEC:
        case AV_PIX_FMT_DRM_PRIME:
        case AV_PIX_FMT_OPENCL:
            return true;
        default:
            return false;
    }
}

QString FrameSharing::getPixelFormatName(int format)
{
    const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? QString::fromUtf8(name) : QString("unknown(%1)").arg(format);
}

bool FrameSharing::transferHardwareFrame(AVFrame *hwFrame, AVFrame *swFrameOut)
{
    if (hwFrame == nullptr || swFrameOut == nullptr) {
        return false;
    }
    
    // Reset the software frame
    av_frame_unref(swFrameOut);
    
    // Transfer data from GPU to CPU
    int ret = av_hwframe_transfer_data(swFrameOut, hwFrame, 0);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        if (lastErrorFormat != hwFrame->format) {
            lastErrorFormat = hwFrame->format;
            FrameSharingLogger::instance().logError(
                QString("av_hwframe_transfer_data failed: %1").arg(errBuf));
        }
        return false;
    }
    
    // Copy frame properties
    swFrameOut->width = hwFrame->width;
    swFrameOut->height = hwFrame->height;
    swFrameOut->pts = hwFrame->pts;
    
    return true;
}

uint8_t* FrameSharing::getFrameSlot(int index)
{
#ifdef Q_OS_WIN
    if (frameDataArea == nullptr || index < 0 || index >= FRAME_SHARING_RING_BUFFER_SIZE) {
        return nullptr;
    }
    return frameDataArea + (index * singleFrameSize);
#else
    return nullptr;
#endif
}

bool FrameSharing::convertFrameToBGRA(AVFrame *srcFrame, uint8_t *dstBuffer)
{
    if (srcFrame == nullptr || dstBuffer == nullptr) {
        return false;
    }
    
    if (srcFrame->data[0] == nullptr) {
        return false;
    }
    
    if (srcFrame->width != frameWidth || srcFrame->height != frameHeight) {
        FrameSharingLogger::instance().logWarning(
            QString("Size mismatch: %1x%2 vs %3x%4")
            .arg(srcFrame->width).arg(srcFrame->height)
            .arg(frameWidth).arg(frameHeight));
        return false;
    }
    
    // Get or create sws context
    swsContext = sws_getCachedContext(
        swsContext,
        srcFrame->width, srcFrame->height, static_cast<AVPixelFormat>(srcFrame->format),
        frameWidth, frameHeight, AV_PIX_FMT_BGRA,
        SWS_FAST_BILINEAR,  // Fast scaling algorithm
        nullptr, nullptr, nullptr
    );
    
    if (swsContext == nullptr) {
        if (lastErrorFormat != srcFrame->format) {
            lastErrorFormat = srcFrame->format;
            FrameSharingLogger::instance().logError(
                QString("Failed to create sws context for %1").arg(getPixelFormatName(srcFrame->format)));
        }
        return false;
    }
    
    // Set up destination
    uint8_t *dstData[4] = { dstBuffer, nullptr, nullptr, nullptr };
    int dstLinesize[4] = { frameWidth * 4, 0, 0, 0 };
    
    // Convert
    int result = sws_scale(
        swsContext,
        srcFrame->data, srcFrame->linesize,
        0, srcFrame->height,
        dstData, dstLinesize
    );
    
    return (result == frameHeight);
}

bool FrameSharing::sendFrame(AVFrame *frame)
{
    if (!active.load() || !initialized.load()) {
        return false;
    }
    
    QMutexLocker locker(&sendMutex);
    
    if (!active.load() || !initialized.load()) {
        return false;
    }
    
    frameCounter++;
    
#ifdef Q_OS_WIN
    if (mappedMemory == nullptr || header == nullptr || frameDataArea == nullptr) {
        return false;
    }
    
    if (frame == nullptr) {
        return false;
    }
    
    AVFrame *frameToConvert = frame;
    bool isHW = isHardwareFrame(frame);
    
    // Handle hardware frames - transfer to software
    if (isHW) {
        hwFrameCounter++;
        
        if (!transferHardwareFrame(frame, swFrame)) {
            // Transfer failed - skip this frame
            return false;
        }
        frameToConvert = swFrame;
    }
    
    // Get the next write slot in ring buffer
    uint32_t writeIdx = header->writeIndex;
    uint8_t *frameSlot = getFrameSlot(writeIdx);
    
    if (frameSlot == nullptr) {
        return false;
    }
    
    // Convert and write directly to shared memory
    if (!convertFrameToBGRA(frameToConvert, frameSlot)) {
        return false;
    }
    
    // Update frame metadata
    successFrameCounter++;
    uint64_t now = QDateTime::currentMSecsSinceEpoch();
    
    header->frameTimestamp[writeIdx] = now;
    header->frameNumber[writeIdx] = successFrameCounter;
    
    // Memory barrier before marking ready
    MemoryBarrier();
    
    header->frameReady[writeIdx] = 1;
    header->timestamp = now;
    header->totalFrames = successFrameCounter;
    
    // Advance write index (ring buffer)
    header->writeIndex = (writeIdx + 1) % FRAME_SHARING_RING_BUFFER_SIZE;
    
    // Memory barrier after all writes
    MemoryBarrier();
    
    // Signal the event
    if (hEvent != nullptr) {
        SetEvent(hEvent);
    }
    
    // Log first successful frame
    if (successFrameCounter == 1) {
        FrameSharingLogger::instance().logInfo("=============================================");
        FrameSharingLogger::instance().logInfo("  FIRST FRAME SHARED!");
        FrameSharingLogger::instance().logInfo(QString("  Source format: %1 (HW: %2)")
            .arg(getPixelFormatName(frame->format))
            .arg(isHW ? "YES" : "NO"));
        FrameSharingLogger::instance().logInfo(QString("  Size: %1x%2").arg(frame->width).arg(frame->height));
        FrameSharingLogger::instance().logInfo("=============================================");
    }
    
    // Log stats every 5 seconds (300 frames @ 60fps)
    if (successFrameCounter % 300 == 0) {
        double rate = frameCounter > 0 ? (successFrameCounter * 100.0 / frameCounter) : 0;
        FrameSharingLogger::instance().logInfo(
            QString("[STATS] Frames: %1 | HW: %2 | Success: %3%")
            .arg(successFrameCounter).arg(hwFrameCounter).arg(rate, 0, 'f', 1));
    }
    
    return true;
    
#else
    return false;
#endif
}
