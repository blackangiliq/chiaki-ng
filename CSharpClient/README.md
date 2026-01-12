# Chiaki Frame Receiver - C# Client v2

هذا المشروع يتيح لك استقبال الفريمات من Chiaki عبر Memory Mapped Files.

## الجديد في النسخة 2

- ✅ **Ring Buffer** (3 فريمات) - لا فقدان للفريمات عند 60fps
- ✅ **دعم Hardware Decoding** (D3D11/DXVA2) - الفريمات تُنقل من GPU
- ✅ **أداء محسّن** - كتابة مباشرة للذاكرة المشتركة
- ✅ **FPS Tracking** - متابعة سرعة الإطارات

## المتطلبات

- .NET 6.0 أو أحدث
- Windows (لأن Memory Mapped Files مُنفذة لـ Windows فقط في Chiaki)

## طريقة الاستخدام

### 1. تشغيل البرنامج التجريبي

```bash
cd CSharpClient
dotnet run
```

### 2. استخدام المكتبة في مشروعك

```csharp
using ChiakiFrameShare;

// إنشاء المستقبل (النسخة 2)
var receiver = new ChiakiFrameReceiverV2();

// الاتصال بـ Chiaki
if (receiver.Connect())
{
    Console.WriteLine("متصل بنجاح!");
    
    while (true)
    {
        // الطريقة 1: الحصول على أحدث فريم (الأسرع)
        var frame = receiver.GetLatestFrame();
        
        // الطريقة 2: الحصول على كل الفريمات الجديدة (لمعالجة كل فريم)
        // var frames = receiver.GetAllNewFrames();
        
        if (frame != null)
        {
            Console.WriteLine($"Frame: {frame.Width}x{frame.Height} | FPS: {receiver.AverageFps:F1}");
            
            // تحويل إلى Bitmap
            using (var bitmap = frame.ToBitmap())
            {
                bitmap.Save("screenshot.png");
            }
        }
        
        Thread.Sleep(1); // تجنب busy-waiting
    }
}

receiver.Disconnect();
```

## بنية البيانات

### FrameHeaderV2 (النسخة 2)
```csharp
public struct FrameHeaderV2
{
    public uint Magic;                  // 0x4B414843 ("CHAK")
    public uint Version;                // 2
    public uint Width;                  // عرض الفريم
    public uint Height;                 // ارتفاع الفريم
    public uint Stride;                 // بايتات لكل صف
    public uint Format;                 // 0 = BGRA32
    public uint FrameDataSize;          // حجم فريم واحد
    public uint RingBufferSize;         // عدد الفريمات في Ring Buffer (3)
    public uint RingBufferFrameOffset;  // موقع بيانات الفريمات
    
    // Ring Buffer Control
    public uint WriteIndex;             // مؤشر الكتابة
    public uint ReadIndex;              // مؤشر القراءة
    public ulong TotalFrames;           // إجمالي الفريمات
    public ulong Timestamp;             // وقت آخر فريم
    
    // Per-frame data (3 slots)
    public uint FrameReady0, FrameReady1, FrameReady2;
    public ulong FrameTimestamp0, FrameTimestamp1, FrameTimestamp2;
    public ulong FrameNumber0, FrameNumber1, FrameNumber2;
}
```

### VideoFrame
```csharp
public class VideoFrame
{
    public int Width { get; set; }
    public int Height { get; set; }
    public int Stride { get; set; }
    public byte[] Data { get; set; }        // بيانات BGRA
    public ulong FrameNumber { get; set; }
    public DateTime Timestamp { get; set; }
    public int SlotIndex { get; set; }      // موقع في Ring Buffer
    
    public Bitmap ToBitmap();  // تحويل إلى Bitmap
}
```

## الأحداث (Events)

```csharp
receiver.FrameReceived += (sender, frame) => {
    // يُستدعى عند استلام كل فريم
};

receiver.Error += (sender, message) => {
    // يُستدعى عند حدوث خطأ
};

receiver.Connected += (sender, e) => {
    // يُستدعى عند الاتصال
};

receiver.Disconnected += (sender, e) => {
    // يُستدعى عند قطع الاتصال
};
```

## الإحصائيات

```csharp
Console.WriteLine($"Total Frames: {receiver.TotalFramesReceived}");
Console.WriteLine($"Dropped Frames: {receiver.DroppedFrames}");
Console.WriteLine($"Average FPS: {receiver.AverageFps:F1}");
```

## ملف السجل (Log)

عند حدوث أي مشكلة، تحقق من ملف السجل في Chiaki:
```
%APPDATA%/chiaki-ng/framesharing.log
```

## إعدادات Chiaki الموصى بها

للحصول على أفضل أداء مع Frame Sharing:

| الإعداد | القيمة |
|---------|--------|
| Hardware Decoder | d3d11va أو Auto |
| Resolution | 720p (للسرعة) أو 1080p |
| FPS | 60fps |
| Codec | H265 (أفضل جودة) |

## ملاحظات مهمة

1. **يجب تشغيل Chiaki أولاً** وبدء البث قبل تشغيل برنامج C#
2. **Frame Sharing يبدأ تلقائياً** عند بدء البث
3. **Hardware Frames مدعومة** - D3D11/DXVA2 يعملان بشكل كامل
4. **Ring Buffer** - 3 فريمات للحماية من فقدان البيانات
5. **الفريمات بصيغة BGRA32** (Blue, Green, Red, Alpha) - 4 بايت لكل بكسل

## استكشاف الأخطاء

### "Chiaki is not running"
- تأكد من أن Chiaki يعمل ومتصل بـ PlayStation
- تأكد من بدء البث (يجب أن ترى الفيديو في Chiaki)

### "Unsupported version"
- تأكد من أنك تستخدم نسخة Chiaki المحدثة مع Frame Sharing v2

### الفريمات متقطعة
- استخدم `GetLatestFrame()` بدلاً من `GetAllNewFrames()`
- تحقق من `receiver.DroppedFrames` لمعرفة عدد الفريمات المفقودة

### حجم الذاكرة
- الذاكرة المشتركة: Header + (Width × Height × 4 × 3) bytes
- مثال 720p: ~11 MB
- مثال 1080p: ~25 MB
