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
#include <libswscale/swscale.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Frame header structure - shared between C++ and C#
#pragma pack(push, 1)
struct FrameSharingHeader {
    uint32_t magic;          // Magic number to verify valid data: 0x4B414843 ("CHAK")
    uint32_t version;        // Protocol version: 1
    uint32_t width;          // Frame width in pixels
    uint32_t height;         // Frame height in pixels
    uint32_t stride;         // Bytes per row (may include padding)
    uint32_t format;         // Pixel format: 0=BGRA32
    uint64_t timestamp;      // Frame timestamp (milliseconds since epoch)
    uint64_t frameNumber;    // Sequential frame number
    uint32_t dataSize;       // Size of frame data in bytes
    uint32_t ready;          // 1 = new frame ready, 0 = frame consumed
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
    // Returns true on success, false on failure (check log for details)
    bool initialize(int width, int height);
    
    // Shutdown frame sharing - call when stream ends
    void shutdown();
    
    // Send a frame to the shared memory
    // Returns true on success, false on failure (never crashes, just logs and returns)
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
    
    // Convert frame DIRECTLY to BGRA in destination buffer (zero-copy)
    bool convertFrameToBGRA(AVFrame *srcFrame, uint8_t *destBuffer);
    
    // Check if frame is hardware-accelerated (Vulkan, VAAPI, etc.)
    bool isHardwareFrame(AVFrame *frame);
    
    // Get pixel format name for logging
    QString getPixelFormatName(int format);
    
    // Clean up resources safely
    void cleanup();
    
    std::atomic<bool> active;
    std::atomic<bool> initialized;
    int frameWidth;
    int frameHeight;
    uint64_t frameCounter;
    uint64_t successFrameCounter;
    int lastErrorFormat;
    uint64_t hwFrameSkipCount;
    
    QMutex sendMutex;
    
#ifdef Q_OS_WIN
    HANDLE hMapFile;
    HANDLE hEvent;
    void *mappedMemory;
    FrameSharingHeader *header;
    uint8_t *frameData;
    size_t totalMappedSize;
#endif

    SwsContext *swsContext;
    uint8_t *bgraBuffer;
    int bgraBufferSize;
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

