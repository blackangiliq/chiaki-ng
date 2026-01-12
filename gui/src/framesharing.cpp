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
    // Create log directory if it doesn't exist
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    
    logFilePath = logDir + "/framesharing.log";
    logFile.setFileName(logFilePath);
    
    // Open in append mode
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "FrameSharing: Could not open log file:" << logFilePath;
    } else {
        log("INFO", "=== FrameSharing Log Started ===");
        log("INFO", QString("Log file: %1").arg(logFilePath));
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
    
    // Write to file
    if (logFile.isOpen()) {
        QTextStream stream(&logFile);
        stream << logLine;
        stream.flush();
    }
    
    // Also output to Qt debug
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
    , lastErrorFormat(-1)
    , hwFrameSkipCount(0)
#ifdef Q_OS_WIN
    , hMapFile(nullptr)
    , hEvent(nullptr)
    , mappedMemory(nullptr)
    , header(nullptr)
    , frameData(nullptr)
    , totalMappedSize(0)
#endif
    , swsContext(nullptr)
    , bgraBuffer(nullptr)
    , bgraBufferSize(0)
{
    FrameSharingLogger::instance().logInfo("FrameSharing instance created");
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
    // Unmap memory first
    if (mappedMemory != nullptr) {
        if (!UnmapViewOfFile(mappedMemory)) {
            FrameSharingLogger::instance().logWarning(
                QString("Failed to unmap view of file, error: %1").arg(GetLastError()));
        }
        mappedMemory = nullptr;
        header = nullptr;
        frameData = nullptr;
    }
    
    // Close file mapping handle
    if (hMapFile != nullptr) {
        if (!CloseHandle(hMapFile)) {
            FrameSharingLogger::instance().logWarning(
                QString("Failed to close map file handle, error: %1").arg(GetLastError()));
        }
        hMapFile = nullptr;
    }
    
    // Close event handle
    if (hEvent != nullptr) {
        if (!CloseHandle(hEvent)) {
            FrameSharingLogger::instance().logWarning(
                QString("Failed to close event handle, error: %1").arg(GetLastError()));
        }
        hEvent = nullptr;
    }
#endif

    // Free sws context
    if (swsContext != nullptr) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    
    // Free BGRA buffer
    if (bgraBuffer != nullptr) {
        av_free(bgraBuffer);
        bgraBuffer = nullptr;
        bgraBufferSize = 0;
    }
    
    frameWidth = 0;
    frameHeight = 0;
#ifdef Q_OS_WIN
    totalMappedSize = 0;
#endif
    
    FrameSharingLogger::instance().logInfo("FrameSharing cleanup completed");
}

bool FrameSharing::initialize(int width, int height)
{
    QMutexLocker locker(&sendMutex);
    
    FrameSharingLogger::instance().logInfo(
        QString("Initializing FrameSharing: %1x%2").arg(width).arg(height));
    
    // Validate input
    if (width <= 0 || height <= 0) {
        FrameSharingLogger::instance().logError(
            QString("Invalid dimensions: %1x%2").arg(width).arg(height));
        return false;
    }
    
    if (width > 4096 || height > 4096) {
        FrameSharingLogger::instance().logError(
            QString("Dimensions too large: %1x%2 (max 4096x4096)").arg(width).arg(height));
        return false;
    }
    
    // Clean up any existing resources
    if (initialized.load()) {
        FrameSharingLogger::instance().logInfo("Reinitializing - cleaning up previous state");
        cleanup();
    }
    
    frameWidth = width;
    frameHeight = height;
    frameCounter = 0;
    successFrameCounter = 0;
    lastErrorFormat = -1;
    hwFrameSkipCount = 0;
    
#ifdef Q_OS_WIN
    // Calculate sizes
    int stride = width * 4; // BGRA = 4 bytes per pixel
    size_t frameDataSize = (size_t)stride * height;
    totalMappedSize = sizeof(FrameSharingHeader) + frameDataSize;
    
    FrameSharingLogger::instance().logInfo(
        QString("Creating shared memory: header=%1 bytes, frame=%2 bytes, total=%3 bytes")
        .arg(sizeof(FrameSharingHeader)).arg(frameDataSize).arg(totalMappedSize));
    
    // Create or open the file mapping object
    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,    // Use paging file
        nullptr,                 // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Maximum size (high-order DWORD)
        (DWORD)totalMappedSize,  // Maximum size (low-order DWORD)
        L"ChiakiFrameShare"      // Name of mapping object
    );
    
    if (hMapFile == nullptr) {
        DWORD error = GetLastError();
        FrameSharingLogger::instance().logError(
            QString("CreateFileMapping failed with error: %1").arg(error));
        cleanup();
        return false;
    }
    
    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
    FrameSharingLogger::instance().logInfo(
        QString("File mapping created (already existed: %1)").arg(alreadyExists));
    
    // Map the view
    mappedMemory = MapViewOfFile(
        hMapFile,                // Handle to map object
        FILE_MAP_ALL_ACCESS,     // Read/write permission
        0,                       // Offset high
        0,                       // Offset low
        totalMappedSize          // Number of bytes to map
    );
    
    if (mappedMemory == nullptr) {
        DWORD error = GetLastError();
        FrameSharingLogger::instance().logError(
            QString("MapViewOfFile failed with error: %1").arg(error));
        cleanup();
        return false;
    }
    
    FrameSharingLogger::instance().logInfo("Memory mapped successfully");
    
    // Set up pointers
    header = static_cast<FrameSharingHeader*>(mappedMemory);
    frameData = reinterpret_cast<uint8_t*>(mappedMemory) + sizeof(FrameSharingHeader);
    
    // Initialize header
    memset(header, 0, sizeof(FrameSharingHeader));
    header->magic = 0x4B414843; // "CHAK" in little endian
    header->version = 1;
    header->width = width;
    header->height = height;
    header->stride = stride;
    header->format = 0; // BGRA32
    header->timestamp = 0;
    header->frameNumber = 0;
    header->dataSize = (uint32_t)frameDataSize;
    header->ready = 0;
    
    FrameSharingLogger::instance().logInfo("Header initialized");
    
    // Create event for signaling
    hEvent = CreateEventW(
        nullptr,             // Default security
        FALSE,               // Auto-reset event
        FALSE,               // Initially non-signaled
        L"ChiakiFrameEvent"  // Event name
    );
    
    if (hEvent == nullptr) {
        DWORD error = GetLastError();
        FrameSharingLogger::instance().logError(
            QString("CreateEvent failed with error: %1").arg(error));
        cleanup();
        return false;
    }
    
    FrameSharingLogger::instance().logInfo("Event created successfully");
    
#else
    FrameSharingLogger::instance().logWarning("Frame sharing is only supported on Windows");
    return false;
#endif

    // Allocate BGRA buffer for format conversion
    bgraBufferSize = av_image_get_buffer_size(AV_PIX_FMT_BGRA, width, height, 1);
    if (bgraBufferSize <= 0) {
        FrameSharingLogger::instance().logError(
            QString("Failed to calculate buffer size: %1").arg(bgraBufferSize));
        cleanup();
        return false;
    }
    
    bgraBuffer = static_cast<uint8_t*>(av_malloc(bgraBufferSize));
    if (bgraBuffer == nullptr) {
        FrameSharingLogger::instance().logError("Failed to allocate BGRA buffer");
        cleanup();
        return false;
    }
    
    FrameSharingLogger::instance().logInfo(
        QString("BGRA buffer allocated: %1 bytes").arg(bgraBufferSize));
    
    initialized.store(true);
    active.store(true);
    
    FrameSharingLogger::instance().logInfo("========================================");
    FrameSharingLogger::instance().logInfo("  FRAME SHARING INITIALIZED SUCCESSFULLY!");
    FrameSharingLogger::instance().logInfo("========================================");
    FrameSharingLogger::instance().logInfo(QString("  Resolution: %1 x %2").arg(width).arg(height));
    FrameSharingLogger::instance().logInfo(QString("  Frame size: %1 bytes (%2 MB)").arg(bgraBufferSize).arg(bgraBufferSize / 1024.0 / 1024.0, 0, 'f', 2));
    FrameSharingLogger::instance().logInfo("----------------------------------------");
    FrameSharingLogger::instance().logInfo("  C# CONNECTION INFO:");
    FrameSharingLogger::instance().logInfo(QString("    Shared Memory Name: %1").arg(getSharedMemoryName()));
    FrameSharingLogger::instance().logInfo(QString("    Event Name: %1").arg(getEventName()));
    FrameSharingLogger::instance().logInfo("    Pixel Format: BGRA32 (4 bytes per pixel)");
    FrameSharingLogger::instance().logInfo(QString("    Stride: %1 bytes per row").arg(width * 4));
    FrameSharingLogger::instance().logInfo("----------------------------------------");
    FrameSharingLogger::instance().logInfo("  C# CODE EXAMPLE:");
    FrameSharingLogger::instance().logInfo("    var receiver = new ChiakiFrameReceiver();");
    FrameSharingLogger::instance().logInfo("    if (receiver.Connect()) {");
    FrameSharingLogger::instance().logInfo("        var frame = receiver.TryGetFrame();");
    FrameSharingLogger::instance().logInfo("    }");
    FrameSharingLogger::instance().logInfo("========================================");
    
    return true;
}

void FrameSharing::shutdown()
{
    QMutexLocker locker(&sendMutex);
    
    if (!initialized.load()) {
        return;
    }
    
    double successRate = frameCounter > 0 ? (successFrameCounter * 100.0 / frameCounter) : 0;
    
    FrameSharingLogger::instance().logInfo("========================================");
    FrameSharingLogger::instance().logInfo("  FRAME SHARING SESSION ENDED");
    FrameSharingLogger::instance().logInfo("========================================");
    FrameSharingLogger::instance().logInfo(QString("  Total frames processed: %1").arg(frameCounter));
    FrameSharingLogger::instance().logInfo(QString("  Successfully shared: %1").arg(successFrameCounter));
    FrameSharingLogger::instance().logInfo(QString("  HW frames skipped: %1").arg(hwFrameSkipCount));
    FrameSharingLogger::instance().logInfo(QString("  Success rate: %1%").arg(successRate, 0, 'f', 1));
    FrameSharingLogger::instance().logInfo("========================================");
    
    active.store(false);
    initialized.store(false);
    
    cleanup();
}

bool FrameSharing::isHardwareFrame(AVFrame *frame)
{
    if (frame == nullptr) return false;
    
    // Check if frame has hardware context
    if (frame->hw_frames_ctx != nullptr) return true;
    
    // Check known hardware pixel formats
    switch (frame->format) {
        case AV_PIX_FMT_VULKAN:      // 190
        case AV_PIX_FMT_VAAPI:
        case AV_PIX_FMT_VDPAU:
        case AV_PIX_FMT_CUDA:
        case AV_PIX_FMT_D3D11:
        case AV_PIX_FMT_D3D11VA_VLD:
        case AV_PIX_FMT_DXVA2_VLD:
        case AV_PIX_FMT_QSV:
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
    if (name) {
        return QString::fromUtf8(name);
    }
    return QString("unknown(%1)").arg(format);
}

bool FrameSharing::convertFrameToBGRA(AVFrame *srcFrame)
{
    if (srcFrame == nullptr) {
        FrameSharingLogger::instance().logError("convertFrameToBGRA: srcFrame is null");
        return false;
    }
    
    // Hardware frames are handled in qmlmainwindow.cpp before calling this
    // If we still get one here, skip it
    if (isHardwareFrame(srcFrame)) {
        hwFrameSkipCount++;
        return false;
    }
    
    if (srcFrame->width != frameWidth || srcFrame->height != frameHeight) {
        FrameSharingLogger::instance().logWarning(
            QString("Frame size mismatch: expected %1x%2, got %3x%4")
            .arg(frameWidth).arg(frameHeight).arg(srcFrame->width).arg(srcFrame->height));
        return false;
    }
    
    // Check if source frame data is valid
    if (srcFrame->data[0] == nullptr) {
        FrameSharingLogger::instance().logError("Source frame data is null");
        return false;
    }
    
    // Get or create sws context
    swsContext = sws_getCachedContext(
        swsContext,
        srcFrame->width, srcFrame->height, static_cast<AVPixelFormat>(srcFrame->format),
        frameWidth, frameHeight, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (swsContext == nullptr) {
        // Only log once per format to avoid spam
        if (lastErrorFormat != srcFrame->format) {
            lastErrorFormat = srcFrame->format;
            FrameSharingLogger::instance().logError(
                QString("Failed to create sws context for format %1 (%2). "
                        "This pixel format may not be supported for conversion.")
                .arg(srcFrame->format).arg(getPixelFormatName(srcFrame->format)));
        }
        return false;
    }
    
    // Reset error format on success
    if (lastErrorFormat == srcFrame->format) {
        lastErrorFormat = -1;
    }
    
    // Set up destination
    uint8_t *dstData[4] = { bgraBuffer, nullptr, nullptr, nullptr };
    int dstLinesize[4] = { frameWidth * 4, 0, 0, 0 };
    
    // Convert
    int result = sws_scale(
        swsContext,
        srcFrame->data, srcFrame->linesize,
        0, srcFrame->height,
        dstData, dstLinesize
    );
    
    if (result != frameHeight) {
        FrameSharingLogger::instance().logError(
            QString("sws_scale returned %1, expected %2").arg(result).arg(frameHeight));
        return false;
    }
    
    return true;
}

bool FrameSharing::sendFrame(AVFrame *frame)
{
    // Quick check without locking
    if (!active.load() || !initialized.load()) {
        return false;
    }
    
    // Lock mutex - we want all frames, so we wait
    QMutexLocker locker(&sendMutex);
    
    // Double-check after acquiring lock
    if (!active.load() || !initialized.load()) {
        return false;
    }
    
    frameCounter++;
    
#ifdef Q_OS_WIN
    if (mappedMemory == nullptr || header == nullptr || frameData == nullptr) {
        FrameSharingLogger::instance().logError("sendFrame: Invalid memory pointers");
        return false;
    }
    
    // Validate frame
    if (frame == nullptr) {
        FrameSharingLogger::instance().logError("sendFrame: frame is null");
        return false;
    }
    
    if (frame->data[0] == nullptr) {
        // Hardware frames may have null data[0] - this is expected
        if (!isHardwareFrame(frame)) {
            FrameSharingLogger::instance().logError("sendFrame: frame data is null");
        }
        return false;
    }
    
    // Convert frame to BGRA
    if (!convertFrameToBGRA(frame)) {
        // Error already logged (or it's a hardware frame which is expected to fail)
        return false;
    }
    
    // Copy to shared memory
    size_t copySize = (size_t)frameWidth * frameHeight * 4;
    if (copySize > header->dataSize) {
        FrameSharingLogger::instance().logError(
            QString("Frame size %1 exceeds allocated size %2").arg(copySize).arg(header->dataSize));
        return false;
    }
    
    memcpy(frameData, bgraBuffer, copySize);
    
    // Update header
    successFrameCounter++;
    header->timestamp = QDateTime::currentMSecsSinceEpoch();
    header->frameNumber = successFrameCounter;
    header->ready = 1;
    
    // Memory barrier to ensure writes are visible
    MemoryBarrier();
    
    // Signal the event
    if (hEvent != nullptr) {
        SetEvent(hEvent);
    }
    
    // Log first successful frame
    if (successFrameCounter == 1) {
        FrameSharingLogger::instance().logInfo("========================================");
        FrameSharingLogger::instance().logInfo("  FIRST FRAME SHARED SUCCESSFULLY!");
        FrameSharingLogger::instance().logInfo(QString("  Frame format: %1").arg(getPixelFormatName(frame->format)));
        FrameSharingLogger::instance().logInfo(QString("  Frame size: %1x%2").arg(frame->width).arg(frame->height));
        FrameSharingLogger::instance().logInfo("  C# client can now receive frames!");
        FrameSharingLogger::instance().logInfo("========================================");
    }
    
    // Log statistics every 300 successful frames (approximately every 5 seconds at 60fps)
    if (successFrameCounter % 300 == 0) {
        double successRate = frameCounter > 0 ? (successFrameCounter * 100.0 / frameCounter) : 0;
        FrameSharingLogger::instance().logInfo(
            QString("[STATS] Success: %1 | Total: %2 | HW Skipped: %3 | Success Rate: %4%")
            .arg(successFrameCounter).arg(frameCounter).arg(hwFrameSkipCount).arg(successRate, 0, 'f', 1));
    }
    
    return true;
    
#else
    return false;
#endif
}
