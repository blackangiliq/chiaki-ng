// Chiaki Frame Receiver - Simple & Fast C# Client v2.0
// Usage:
//   var receiver = new ChiakiFrameReceiver();
//   receiver.Connect();
//   while (running) {
//       var frame = receiver.GetFrame(); // Returns null if no new frame
//       if (frame != null) { /* use frame.Data (BGRA) */ }
//   }

using System;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;

namespace ChiakiFrameShare
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FrameHeader
    {
        public uint Magic;          // 0x4B414843 ("CHAK")
        public uint Version;        // 2
        public uint Width;
        public uint Height;
        public uint Stride;         // width * 4
        public uint Format;         // 0 = BGRA32
        public ulong Timestamp;
        public ulong FrameNumber;
        public uint DataSize;
        public uint Ready;          // 1 = new frame
    }

    public class VideoFrame
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public byte[] Data { get; set; }
        public ulong FrameNumber { get; set; }
    }

    public class ChiakiFrameReceiver : IDisposable
    {
        private MemoryMappedFile _file;
        private MemoryMappedViewAccessor _view;
        private EventWaitHandle _event;
        private ulong _lastFrame;
        private static readonly int HeaderSize = Marshal.SizeOf<FrameHeader>();

        public bool IsConnected { get; private set; }
        public ulong FramesReceived { get; private set; }

        public bool Connect()
        {
            if (IsConnected) return true;

            try
            {
                _file = MemoryMappedFile.OpenExisting("ChiakiFrameShare", MemoryMappedFileRights.Read);
                _view = _file.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
                
                try { _event = EventWaitHandle.OpenExisting("ChiakiFrameEvent"); }
                catch { _event = null; } // Will poll if event not available

                var hdr = ReadHeader();
                if (hdr.Magic != 0x4B414843 || hdr.Version != 2)
                {
                    Disconnect();
                    return false;
                }

                IsConnected = true;
                Console.WriteLine($"[Chiaki] Connected: {hdr.Width}x{hdr.Height}");
                return true;
            }
            catch
            {
                Disconnect();
                return false;
            }
        }

        public void Disconnect()
        {
            _event?.Dispose(); _event = null;
            _view?.Dispose(); _view = null;
            _file?.Dispose(); _file = null;
            IsConnected = false;
        }

        /// <summary>Wait for new frame (blocking)</summary>
        public bool WaitForFrame(int timeoutMs = 100)
        {
            if (!IsConnected) return false;
            if (_event != null) return _event.WaitOne(timeoutMs);
            
            // Polling fallback
            var start = DateTime.UtcNow;
            while ((DateTime.UtcNow - start).TotalMilliseconds < timeoutMs)
            {
                var h = ReadHeader();
                if (h.Ready != 0 && h.FrameNumber > _lastFrame) return true;
                Thread.Sleep(1);
            }
            return false;
        }

        /// <summary>Get latest frame (non-blocking, returns null if no new frame)</summary>
        public VideoFrame GetFrame()
        {
            if (!IsConnected) return null;

            var hdr = ReadHeader();
            if (hdr.Ready == 0 || hdr.FrameNumber <= _lastFrame) return null;

            var data = new byte[hdr.DataSize];
            _view.ReadArray(HeaderSize, data, 0, (int)hdr.DataSize);

            _lastFrame = hdr.FrameNumber;
            FramesReceived++;

            return new VideoFrame
            {
                Width = (int)hdr.Width,
                Height = (int)hdr.Height,
                Data = data,
                FrameNumber = hdr.FrameNumber
            };
        }

        private FrameHeader ReadHeader()
        {
            _view.Read(0, out FrameHeader h);
            return h;
        }

        public void Dispose() => Disconnect();
    }

    // Test program
    public class Program
    {
        public static void Main()
        {
            Console.WriteLine("Chiaki Frame Receiver v2.0");
            Console.WriteLine("Waiting for Chiaki to start streaming...\n");

            using var receiver = new ChiakiFrameReceiver();
            
            // Wait for connection
            while (!receiver.Connect()) Thread.Sleep(1000);

            // Receive frames
            int fps = 0;
            var lastSec = DateTime.UtcNow;

            while (true)
            {
                if (receiver.WaitForFrame(100))
                {
                    var frame = receiver.GetFrame();
                    if (frame != null)
                    {
                        fps++;
                        if ((DateTime.UtcNow - lastSec).TotalSeconds >= 1)
                        {
                            Console.WriteLine($"FPS: {fps} | {frame.Width}x{frame.Height} | Frame: {frame.FrameNumber}");
                            fps = 0;
                            lastSec = DateTime.UtcNow;
                        }
                    }
                }
            }
        }
    }
}
