// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_FRAMESHARING_H
#define CHIAKI_FRAMESHARING_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Configuration for 720p @ 60fps optimized
#define FRAME_SHARING_RING_BUFFER_SIZE 3    // Triple buffer for smooth streaming
#define FRAME_SHARING_MAX_WIDTH 1280
#define FRAME_SHARING_MAX_HEIGHT 720

// Frame header structure - shared between C++ and C#
// Version 2: Added ring buffer support
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;              // Magic number: 0x4B414843 ("CHAK")
    uint32_t version;            // Protocol version: 2
    uint32_t width;              // Frame width in pixels
    uint32_t height;             // Frame height in pixels
    uint32_t stride;             // Bytes per row
    uint32_t format;             // Pixel format: 0=BGRA32
    uint32_t frameDataSize;      // Size of ONE frame in bytes
    uint32_t ringBufferSize;     // Number of frames in ring buffer (3)
    uint32_t ringBufferFrameOffset; // Offset to frame data area
    
    // Ring buffer control (atomic operations)
    volatile uint32_t writeIndex;    // Producer writes here (0, 1, 2, 0, 1, 2...)
    volatile uint32_t readIndex;     // Consumer reads here
    volatile uint64_t totalFrames;   // Total frames written
    volatile uint64_t timestamp;     // Latest frame timestamp (ms since epoch)
    
    // Per-frame ready flags
    volatile uint32_t frameReady[FRAME_SHARING_RING_BUFFER_SIZE];
    volatile uint64_t frameTimestamp[FRAME_SHARING_RING_BUFFER_SIZE];
    volatile uint64_t frameNumber[FRAME_SHARING_RING_BUFFER_SIZE];
};
#pragma pack(pop)

class FrameSharingLogger {
public:
    static FrameSharingLogger& instance();
    void log(const QString& level, const QString& message);
    void logError(const QString& message) { log("ERROR", message); }
    void logWarning(const QString& message) { log("WARN", message); }
    void logInfo(const QString& message) { log("INFO", message); }
    void logDebug(const QString& message) { log("DEBUG", message); }
    QString getLogFilePath() const { return logFilePath; }

private:
    FrameSharingLogger();
    ~FrameSharingLogger();
    QMutex mutex;
    QFile logFile;
    QString logFilePath;
};

class FrameSharing : public QObject
{
    Q_OBJECT

public:
    static FrameSharing& instance();
    
    // Initialize frame sharing - call when stream starts
    bool initialize(int width, int height);
    
    // Shutdown frame sharing - call when stream ends
    void shutdown();
    
    // Send a frame to the shared memory (supports both HW and SW frames)
    bool sendFrame(AVFrame *frame);
    
    // Check if frame sharing is active
    bool isActive() const { return active.load(); }
    
    // Get the shared memory name for C# client
    static QString getSharedMemoryName() { return "ChiakiFrameShare"; }
    static QString getEventName() { return "ChiakiFrameEvent"; }

private:
    FrameSharing();
    ~FrameSharing();
    
    // Disable copy
    FrameSharing(const FrameSharing&) = delete;
    FrameSharing& operator=(const FrameSharing&) = delete;
    
    // Transfer hardware frame to software frame
    bool transferHardwareFrame(AVFrame *hwFrame, AVFrame *swFrame);
    
    // Convert frame to BGRA format
    bool convertFrameToBGRA(AVFrame *srcFrame, uint8_t *dstBuffer);
    
    // Check if frame is hardware-accelerated
    bool isHardwareFrame(AVFrame *frame);
    
    // Get pixel format name for logging
    QString getPixelFormatName(int format);
    
    // Clean up resources safely
    void cleanup();
    
    // Get pointer to frame data in ring buffer
    uint8_t* getFrameSlot(int index);
    
    std::atomic<bool> active;
    std::atomic<bool> initialized;
    int frameWidth;
    int frameHeight;
    uint64_t frameCounter;
    uint64_t successFrameCounter;
    uint64_t hwFrameCounter;
    int lastErrorFormat;
    
    QMutex sendMutex;
    
#ifdef Q_OS_WIN
    HANDLE hMapFile;
    HANDLE hEvent;
    void *mappedMemory;
    FrameSharingHeader *header;
    uint8_t *frameDataArea;
    size_t totalMappedSize;
    size_t singleFrameSize;
#endif

    // For hardware frame transfer
    AVFrame *swFrame;
    
    // For format conversion
    SwsContext *swsContext;
};

// Macro for safe frame sharing - use this in presentFrame
#define SAFE_SHARE_FRAME(frame) \
    do { \
        try { \
            if (FrameSharing::instance().isActive()) { \
                FrameSharing::instance().sendFrame(frame); \
            } \
        } catch (const std::exception& e) { \
            FrameSharingLogger::instance().logError(QString("Exception in frame sharing: %1").arg(e.what())); \
        } catch (...) { \
            FrameSharingLogger::instance().logError("Unknown exception in frame sharing"); \
        } \
    } while(0)

#endif // CHIAKI_FRAMESHARING_H
