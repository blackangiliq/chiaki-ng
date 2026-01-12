// Chiaki Frame Receiver - C# Client v2
// Supports Ring Buffer for zero frame loss at 60fps
// 
// Usage:
//   var receiver = new ChiakiFrameReceiverV2();
//   if (receiver.Connect()) {
//       while (running) {
//           var frame = receiver.GetLatestFrame();
//           if (frame != null) {
//               // Use frame.Data (BGRA format), frame.Width, frame.Height
//           }
//       }
//       receiver.Disconnect();
//   }

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;

namespace ChiakiFrameShare
{
    public static class Constants
    {
        public const int RING_BUFFER_SIZE = 3;
    }

    /// <summary>
    /// Header structure matching the C++ side (Version 2 with Ring Buffer)
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FrameHeaderV2
    {
        public uint Magic;                  // 0x4B414843 ("CHAK")
        public uint Version;                // Protocol version: 2
        public uint Width;                  // Frame width
        public uint Height;                 // Frame height
        public uint Stride;                 // Bytes per row
        public uint Format;                 // 0 = BGRA32
        public uint FrameDataSize;          // Size of ONE frame in bytes
        public uint RingBufferSize;         // Number of frames in ring buffer (3)
        public uint RingBufferFrameOffset;  // Offset to frame data area
        
        // Ring buffer control
        public uint WriteIndex;             // Producer writes here
        public uint ReadIndex;              // Consumer reads here
        public ulong TotalFrames;           // Total frames written
        public ulong Timestamp;             // Latest frame timestamp
        
        // Per-frame data (3 slots)
        public uint FrameReady0;
        public uint FrameReady1;
        public uint FrameReady2;
        public ulong FrameTimestamp0;
        public ulong FrameTimestamp1;
        public ulong FrameTimestamp2;
        public ulong FrameNumber0;
        public ulong FrameNumber1;
        public ulong FrameNumber2;
    }

    /// <summary>
    /// Represents a received video frame
    /// </summary>
    public class VideoFrame
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public int Stride { get; set; }
        public byte[] Data { get; set; }
        public ulong FrameNumber { get; set; }
        public DateTime Timestamp { get; set; }
        public int SlotIndex { get; set; }

        /// <summary>
        /// Convert to System.Drawing.Bitmap
        /// </summary>
        public Bitmap ToBitmap()
        {
            var bitmap = new Bitmap(Width, Height, PixelFormat.Format32bppArgb);
            var bmpData = bitmap.LockBits(
                new Rectangle(0, 0, Width, Height),
                ImageLockMode.WriteOnly,
                PixelFormat.Format32bppArgb);

            try
            {
                for (int y = 0; y < Height; y++)
                {
                    Marshal.Copy(Data, y * Stride, 
                        IntPtr.Add(bmpData.Scan0, y * bmpData.Stride), 
                        Math.Min(Stride, bmpData.Stride));
                }
            }
            finally
            {
                bitmap.UnlockBits(bmpData);
            }

            return bitmap;
        }
    }

    /// <summary>
    /// Fast frame receiver with Ring Buffer support (Version 2)
    /// </summary>
    public class ChiakiFrameReceiverV2 : IDisposable
    {
        private const string SharedMemoryName = "ChiakiFrameShare";
        private const string EventName = "ChiakiFrameEvent";
        private const uint ExpectedMagic = 0x4B414843; // "CHAK"
        private const uint ExpectedVersion = 2;

        private MemoryMappedFile _mappedFile;
        private MemoryMappedViewAccessor _accessor;
        private EventWaitHandle _frameEvent;
        private bool _connected;
        private readonly object _lock = new object();
        
        private int _headerSize;
        private uint _lastReadIndex;
        private ulong _lastFrameNumber;
        private uint _frameDataSize;
        private uint _ringBufferFrameOffset;

        // Statistics
        public ulong TotalFramesReceived { get; private set; }
        public ulong DroppedFrames { get; private set; }
        public bool IsConnected => _connected;
        public double AverageFps { get; private set; }

        // Events
        public event EventHandler<VideoFrame> FrameReceived;
        public event EventHandler<string> Error;
        public event EventHandler Connected;
        public event EventHandler Disconnected;

        private DateTime _fpsStartTime;
        private int _fpsFrameCount;

        public ChiakiFrameReceiverV2()
        {
            _headerSize = Marshal.SizeOf<FrameHeaderV2>();
        }

        /// <summary>
        /// Connect to Chiaki's shared memory
        /// </summary>
        public bool Connect()
        {
            lock (_lock)
            {
                if (_connected)
                    return true;

                try
                {
                    _mappedFile = MemoryMappedFile.OpenExisting(
                        SharedMemoryName,
                        MemoryMappedFileRights.Read);

                    _accessor = _mappedFile.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);

                    try
                    {
                        _frameEvent = EventWaitHandle.OpenExisting(EventName);
                    }
                    catch (WaitHandleCannotBeOpenedException)
                    {
                        _frameEvent = null;
                        LogInfo("Event not found, using polling mode");
                    }

                    var header = ReadHeader();
                    if (header.Magic != ExpectedMagic)
                    {
                        throw new InvalidDataException(
                            $"Invalid magic: 0x{header.Magic:X8}, expected 0x{ExpectedMagic:X8}");
                    }

                    if (header.Version != ExpectedVersion)
                    {
                        // Try fallback to v1
                        if (header.Version == 1)
                        {
                            LogInfo("Detected v1 protocol, consider updating Chiaki");
                        }
                        else
                        {
                            throw new InvalidDataException(
                                $"Unsupported version: {header.Version}, expected {ExpectedVersion}");
                        }
                    }

                    _frameDataSize = header.FrameDataSize;
                    _ringBufferFrameOffset = header.RingBufferFrameOffset;
                    _lastReadIndex = 0;
                    _lastFrameNumber = 0;

                    _connected = true;
                    TotalFramesReceived = 0;
                    DroppedFrames = 0;
                    _fpsStartTime = DateTime.UtcNow;
                    _fpsFrameCount = 0;

                    LogInfo($"Connected! Resolution: {header.Width}x{header.Height}, Ring Buffer: {header.RingBufferSize}");
                    Connected?.Invoke(this, EventArgs.Empty);

                    return true;
                }
                catch (FileNotFoundException)
                {
                    LogError("Chiaki is not running or frame sharing is not active");
                    return false;
                }
                catch (Exception ex)
                {
                    LogError($"Failed to connect: {ex.Message}");
                    Cleanup();
                    return false;
                }
            }
        }

        /// <summary>
        /// Disconnect from shared memory
        /// </summary>
        public void Disconnect()
        {
            lock (_lock)
            {
                if (!_connected)
                    return;

                Cleanup();
                _connected = false;
                Disconnected?.Invoke(this, EventArgs.Empty);
                LogInfo("Disconnected");
            }
        }

        /// <summary>
        /// Wait for a new frame (blocking)
        /// </summary>
        public bool WaitForFrame(int timeoutMs = -1)
        {
            if (!_connected)
                return false;

            try
            {
                if (_frameEvent != null)
                {
                    return _frameEvent.WaitOne(timeoutMs);
                }
                else
                {
                    // Polling mode
                    var startTime = DateTime.UtcNow;
                    while (true)
                    {
                        var header = ReadHeader();
                        if (HasNewFrame(header))
                            return true;

                        if (timeoutMs >= 0)
                        {
                            var elapsed = (DateTime.UtcNow - startTime).TotalMilliseconds;
                            if (elapsed >= timeoutMs)
                                return false;
                        }

                        Thread.Sleep(1);
                    }
                }
            }
            catch (Exception ex)
            {
                LogError($"Error waiting for frame: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Get the latest frame (non-blocking, gets newest available)
        /// </summary>
        public VideoFrame GetLatestFrame()
        {
            if (!_connected)
                return null;

            try
            {
                var header = ReadHeader();
                
                // Find the latest ready frame
                int latestSlot = -1;
                ulong latestNumber = _lastFrameNumber;
                
                uint[] ready = { header.FrameReady0, header.FrameReady1, header.FrameReady2 };
                ulong[] numbers = { header.FrameNumber0, header.FrameNumber1, header.FrameNumber2 };
                ulong[] timestamps = { header.FrameTimestamp0, header.FrameTimestamp1, header.FrameTimestamp2 };
                
                for (int i = 0; i < Constants.RING_BUFFER_SIZE; i++)
                {
                    if (ready[i] != 0 && numbers[i] > latestNumber)
                    {
                        latestNumber = numbers[i];
                        latestSlot = i;
                    }
                }
                
                if (latestSlot < 0)
                    return null;

                // Calculate dropped frames
                if (_lastFrameNumber > 0 && latestNumber > _lastFrameNumber + 1)
                {
                    DroppedFrames += latestNumber - _lastFrameNumber - 1;
                }

                // Read frame data
                long frameOffset = _ringBufferFrameOffset + (latestSlot * _frameDataSize);
                var frameData = new byte[_frameDataSize];
                _accessor.ReadArray(frameOffset, frameData, 0, (int)_frameDataSize);

                _lastFrameNumber = latestNumber;
                _lastReadIndex = (uint)latestSlot;
                TotalFramesReceived++;

                // Update FPS
                _fpsFrameCount++;
                var elapsed = (DateTime.UtcNow - _fpsStartTime).TotalSeconds;
                if (elapsed >= 1.0)
                {
                    AverageFps = _fpsFrameCount / elapsed;
                    _fpsFrameCount = 0;
                    _fpsStartTime = DateTime.UtcNow;
                }

                var frame = new VideoFrame
                {
                    Width = (int)header.Width,
                    Height = (int)header.Height,
                    Stride = (int)header.Stride,
                    Data = frameData,
                    FrameNumber = latestNumber,
                    Timestamp = DateTimeOffset.FromUnixTimeMilliseconds((long)timestamps[latestSlot]).DateTime,
                    SlotIndex = latestSlot
                };

                FrameReceived?.Invoke(this, frame);
                return frame;
            }
            catch (Exception ex)
            {
                LogError($"Error reading frame: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Get all new frames since last read (for processing every frame)
        /// </summary>
        public VideoFrame[] GetAllNewFrames()
        {
            if (!_connected)
                return Array.Empty<VideoFrame>();

            try
            {
                var header = ReadHeader();
                var frames = new System.Collections.Generic.List<VideoFrame>();
                
                uint[] ready = { header.FrameReady0, header.FrameReady1, header.FrameReady2 };
                ulong[] numbers = { header.FrameNumber0, header.FrameNumber1, header.FrameNumber2 };
                ulong[] timestamps = { header.FrameTimestamp0, header.FrameTimestamp1, header.FrameTimestamp2 };
                
                // Find all new frames
                var newFrames = new System.Collections.Generic.List<(int slot, ulong number, ulong timestamp)>();
                for (int i = 0; i < Constants.RING_BUFFER_SIZE; i++)
                {
                    if (ready[i] != 0 && numbers[i] > _lastFrameNumber)
                    {
                        newFrames.Add((i, numbers[i], timestamps[i]));
                    }
                }
                
                // Sort by frame number
                newFrames.Sort((a, b) => a.number.CompareTo(b.number));
                
                foreach (var (slot, number, timestamp) in newFrames)
                {
                    long frameOffset = _ringBufferFrameOffset + (slot * _frameDataSize);
                    var frameData = new byte[_frameDataSize];
                    _accessor.ReadArray(frameOffset, frameData, 0, (int)_frameDataSize);
                    
                    frames.Add(new VideoFrame
                    {
                        Width = (int)header.Width,
                        Height = (int)header.Height,
                        Stride = (int)header.Stride,
                        Data = frameData,
                        FrameNumber = number,
                        Timestamp = DateTimeOffset.FromUnixTimeMilliseconds((long)timestamp).DateTime,
                        SlotIndex = slot
                    });
                    
                    _lastFrameNumber = number;
                    TotalFramesReceived++;
                }
                
                return frames.ToArray();
            }
            catch (Exception ex)
            {
                LogError($"Error reading frames: {ex.Message}");
                return Array.Empty<VideoFrame>();
            }
        }

        /// <summary>
        /// Get frame info without reading data
        /// </summary>
        public FrameHeaderV2 GetFrameInfo()
        {
            if (!_connected)
                return default;

            return ReadHeader();
        }

        private bool HasNewFrame(FrameHeaderV2 header)
        {
            return header.TotalFrames > _lastFrameNumber;
        }

        private FrameHeaderV2 ReadHeader()
        {
            FrameHeaderV2 header;
            _accessor.Read(0, out header);
            return header;
        }

        private void Cleanup()
        {
            _frameEvent?.Dispose();
            _frameEvent = null;

            _accessor?.Dispose();
            _accessor = null;

            _mappedFile?.Dispose();
            _mappedFile = null;
        }

        private void LogInfo(string message)
        {
            Console.WriteLine($"[ChiakiFrameReceiver] {message}");
        }

        private void LogError(string message)
        {
            Console.WriteLine($"[ChiakiFrameReceiver] ERROR: {message}");
            Error?.Invoke(this, message);
        }

        public void Dispose()
        {
            Disconnect();
        }
    }

    /// <summary>
    /// Example usage and test program
    /// </summary>
    public class Program
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("Chiaki Frame Receiver v2 - C# Client");
            Console.WriteLine("=====================================");
            Console.WriteLine("Ring Buffer: 3 frames (zero loss at 60fps)");
            Console.WriteLine("Hardware Decode: Supported (D3D11/DXVA2)");
            Console.WriteLine("\nMake sure Chiaki is running and streaming!");
            Console.WriteLine("Press Ctrl+C to exit\n");

            using (var receiver = new ChiakiFrameReceiverV2())
            {
                receiver.Error += (s, e) => Console.WriteLine($"Error: {e}");
                receiver.Connected += (s, e) => Console.WriteLine("Connected!");
                receiver.Disconnected += (s, e) => Console.WriteLine("Disconnected!");

                Console.WriteLine("Connecting to Chiaki...");
                
                int retryCount = 0;
                while (!receiver.Connect() && retryCount < 30)
                {
                    Console.WriteLine($"Waiting for Chiaki... ({++retryCount}/30)");
                    Thread.Sleep(1000);
                }

                if (!receiver.IsConnected)
                {
                    Console.WriteLine("Failed to connect. Make sure Chiaki is streaming.");
                    return;
                }

                var lastStatTime = DateTime.UtcNow;
                var framesThisSecond = 0;

                Console.WriteLine("\nReceiving frames... (Ctrl+C to stop)\n");

                while (true)
                {
                    if (receiver.WaitForFrame(100))
                    {
                        var frame = receiver.GetLatestFrame();
                        if (frame != null)
                        {
                            framesThisSecond++;

                            if ((DateTime.UtcNow - lastStatTime).TotalSeconds >= 1)
                            {
                                Console.WriteLine($"FPS: {framesThisSecond} | " +
                                    $"Avg: {receiver.AverageFps:F1} | " +
                                    $"Size: {frame.Width}x{frame.Height} | " +
                                    $"Frame#: {frame.FrameNumber} | " +
                                    $"Slot: {frame.SlotIndex} | " +
                                    $"Total: {receiver.TotalFramesReceived} | " +
                                    $"Dropped: {receiver.DroppedFrames}");
                                
                                framesThisSecond = 0;
                                lastStatTime = DateTime.UtcNow;
                            }

                            // Save screenshot every 5 seconds
                            if (frame.FrameNumber % 300 == 0)
                            {
                                try
                                {
                                    using (var bitmap = frame.ToBitmap())
                                    {
                                        var filename = $"frame_{frame.FrameNumber}.png";
                                        bitmap.Save(filename, ImageFormat.Png);
                                        Console.WriteLine($"Saved: {filename}");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"Save failed: {ex.Message}");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
