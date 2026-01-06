// Chiaki Frame Receiver - C# Client
// This class reads video frames shared by Chiaki via Memory Mapped Files
// 
// Usage:
//   var receiver = new ChiakiFrameReceiver();
//   if (receiver.Connect()) {
//       while (running) {
//           if (receiver.WaitForFrame(100)) {
//               var frame = receiver.GetCurrentFrame();
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
    /// <summary>
    /// Header structure matching the C++ side (must be exactly the same layout)
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FrameHeader
    {
        public uint Magic;          // 0x4B414843 ("CHAK")
        public uint Version;        // Protocol version: 1
        public uint Width;          // Frame width
        public uint Height;         // Frame height
        public uint Stride;         // Bytes per row
        public uint Format;         // 0 = BGRA32
        public ulong Timestamp;     // Frame timestamp (ms since epoch)
        public ulong FrameNumber;   // Sequential frame number
        public uint DataSize;       // Size of frame data
        public uint Ready;          // 1 = new frame, 0 = consumed
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

        /// <summary>
        /// Convert to System.Drawing.Bitmap (for WinForms/GDI+)
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
                // Copy row by row to handle stride differences
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
    /// Receives video frames from Chiaki via shared memory
    /// </summary>
    public class ChiakiFrameReceiver : IDisposable
    {
        private const string SharedMemoryName = "ChiakiFrameShare";
        private const string EventName = "ChiakiFrameEvent";
        private const uint ExpectedMagic = 0x4B414843; // "CHAK"
        private const uint ExpectedVersion = 1;

        private MemoryMappedFile _mappedFile;
        private MemoryMappedViewAccessor _accessor;
        private EventWaitHandle _frameEvent;
        private bool _connected;
        private readonly object _lock = new object();
        
        private int _headerSize;
        private ulong _lastFrameNumber;
        private VideoFrame _currentFrame;

        // Statistics
        public ulong TotalFramesReceived { get; private set; }
        public ulong DroppedFrames { get; private set; }
        public bool IsConnected => _connected;

        // Events
        public event EventHandler<VideoFrame> FrameReceived;
        public event EventHandler<string> Error;
        public event EventHandler Connected;
        public event EventHandler Disconnected;

        public ChiakiFrameReceiver()
        {
            _headerSize = Marshal.SizeOf<FrameHeader>();
        }

        /// <summary>
        /// Connect to Chiaki's shared memory
        /// </summary>
        /// <returns>True if connected successfully</returns>
        public bool Connect()
        {
            lock (_lock)
            {
                if (_connected)
                    return true;

                try
                {
                    // Try to open existing shared memory
                    _mappedFile = MemoryMappedFile.OpenExisting(
                        SharedMemoryName,
                        MemoryMappedFileRights.Read);

                    // Create accessor for reading
                    _accessor = _mappedFile.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);

                    // Try to open the event
                    try
                    {
                        _frameEvent = EventWaitHandle.OpenExisting(EventName);
                    }
                    catch (WaitHandleCannotBeOpenedException)
                    {
                        // Event doesn't exist yet, we'll poll instead
                        _frameEvent = null;
                        LogInfo("Event not found, will use polling mode");
                    }

                    // Validate header
                    var header = ReadHeader();
                    if (header.Magic != ExpectedMagic)
                    {
                        throw new InvalidDataException(
                            $"Invalid magic number: 0x{header.Magic:X8}, expected 0x{ExpectedMagic:X8}");
                    }

                    if (header.Version != ExpectedVersion)
                    {
                        throw new InvalidDataException(
                            $"Unsupported version: {header.Version}, expected {ExpectedVersion}");
                    }

                    _connected = true;
                    TotalFramesReceived = 0;
                    DroppedFrames = 0;
                    _lastFrameNumber = 0;

                    LogInfo($"Connected to Chiaki frame share. Resolution: {header.Width}x{header.Height}");
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
                LogInfo("Disconnected from Chiaki frame share");
            }
        }

        /// <summary>
        /// Wait for a new frame to be available
        /// </summary>
        /// <param name="timeoutMs">Timeout in milliseconds (-1 for infinite)</param>
        /// <returns>True if a new frame is available</returns>
        public bool WaitForFrame(int timeoutMs = -1)
        {
            if (!_connected)
                return false;

            try
            {
                if (_frameEvent != null)
                {
                    // Wait on event
                    return _frameEvent.WaitOne(timeoutMs);
                }
                else
                {
                    // Polling mode
                    var startTime = DateTime.UtcNow;
                    while (true)
                    {
                        var header = ReadHeader();
                        if (header.Ready != 0 && header.FrameNumber > _lastFrameNumber)
                            return true;

                        if (timeoutMs >= 0)
                        {
                            var elapsed = (DateTime.UtcNow - startTime).TotalMilliseconds;
                            if (elapsed >= timeoutMs)
                                return false;
                        }

                        Thread.Sleep(1); // Small sleep to avoid busy-waiting
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
        /// Try to get the current frame (non-blocking)
        /// </summary>
        /// <returns>VideoFrame if available, null otherwise</returns>
        public VideoFrame TryGetFrame()
        {
            if (!_connected)
                return null;

            try
            {
                var header = ReadHeader();

                // Check if new frame is ready
                if (header.Ready == 0)
                    return null;

                if (header.FrameNumber <= _lastFrameNumber)
                    return null;

                // Calculate dropped frames
                if (_lastFrameNumber > 0 && header.FrameNumber > _lastFrameNumber + 1)
                {
                    var dropped = header.FrameNumber - _lastFrameNumber - 1;
                    DroppedFrames += dropped;
                }

                // Read frame data
                var frameData = new byte[header.DataSize];
                _accessor.ReadArray(_headerSize, frameData, 0, (int)header.DataSize);

                _lastFrameNumber = header.FrameNumber;
                TotalFramesReceived++;

                _currentFrame = new VideoFrame
                {
                    Width = (int)header.Width,
                    Height = (int)header.Height,
                    Stride = (int)header.Stride,
                    Data = frameData,
                    FrameNumber = header.FrameNumber,
                    Timestamp = DateTimeOffset.FromUnixTimeMilliseconds((long)header.Timestamp).DateTime
                };

                FrameReceived?.Invoke(this, _currentFrame);
                return _currentFrame;
            }
            catch (Exception ex)
            {
                LogError($"Error reading frame: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Get the current frame (call after WaitForFrame returns true)
        /// </summary>
        public VideoFrame GetCurrentFrame()
        {
            return TryGetFrame();
        }

        /// <summary>
        /// Get frame information without reading data
        /// </summary>
        public FrameHeader GetFrameInfo()
        {
            if (!_connected)
                return default;

            return ReadHeader();
        }

        private FrameHeader ReadHeader()
        {
            FrameHeader header;
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
            Console.WriteLine("Chiaki Frame Receiver - C# Client");
            Console.WriteLine("==================================");
            Console.WriteLine("Make sure Chiaki is running and streaming!");
            Console.WriteLine("Press Ctrl+C to exit\n");

            using (var receiver = new ChiakiFrameReceiver())
            {
                // Set up events
                receiver.Error += (s, e) => Console.WriteLine($"Error: {e}");
                receiver.Connected += (s, e) => Console.WriteLine("Connected!");
                receiver.Disconnected += (s, e) => Console.WriteLine("Disconnected!");

                // Try to connect
                Console.WriteLine("Attempting to connect to Chiaki...");
                
                int retryCount = 0;
                while (!receiver.Connect() && retryCount < 30)
                {
                    Console.WriteLine($"Waiting for Chiaki to start streaming... (attempt {++retryCount}/30)");
                    Thread.Sleep(1000);
                }

                if (!receiver.IsConnected)
                {
                    Console.WriteLine("Failed to connect to Chiaki. Make sure it's streaming.");
                    return;
                }

                // Main loop
                var lastStatTime = DateTime.UtcNow;
                var framesThisSecond = 0;

                Console.WriteLine("\nReceiving frames... (Press Ctrl+C to stop)\n");

                while (true)
                {
                    if (receiver.WaitForFrame(100))
                    {
                        var frame = receiver.GetCurrentFrame();
                        if (frame != null)
                        {
                            framesThisSecond++;

                            // Print stats every second
                            if ((DateTime.UtcNow - lastStatTime).TotalSeconds >= 1)
                            {
                                Console.WriteLine($"FPS: {framesThisSecond} | " +
                                    $"Resolution: {frame.Width}x{frame.Height} | " +
                                    $"Frame#: {frame.FrameNumber} | " +
                                    $"Total: {receiver.TotalFramesReceived} | " +
                                    $"Dropped: {receiver.DroppedFrames}");
                                
                                framesThisSecond = 0;
                                lastStatTime = DateTime.UtcNow;
                            }

                            // Example: Save a frame every 300 frames (about 5 seconds at 60fps)
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
                                    Console.WriteLine($"Failed to save frame: {ex.Message}");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

