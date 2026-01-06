# Chiaki Frame Receiver - C# Client

هذا المشروع يتيح لك استقبال الفريمات من Chiaki عبر Memory Mapped Files.

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

// إنشاء المستقبل
var receiver = new ChiakiFrameReceiver();

// الاتصال بـ Chiaki
if (receiver.Connect())
{
    Console.WriteLine("متصل بنجاح!");
    
    while (true)
    {
        // انتظار فريم جديد (timeout بالميلي ثانية)
        if (receiver.WaitForFrame(100))
        {
            var frame = receiver.GetCurrentFrame();
            
            if (frame != null)
            {
                // استخدام البيانات
                Console.WriteLine($"Frame: {frame.Width}x{frame.Height}");
                
                // تحويل إلى Bitmap
                using (var bitmap = frame.ToBitmap())
                {
                    // استخدام الـ bitmap
                    bitmap.Save("screenshot.png");
                }
            }
        }
    }
}

receiver.Disconnect();
```

## بنية البيانات

### FrameHeader
```csharp
public struct FrameHeader
{
    public uint Magic;          // 0x4B414843 ("CHAK")
    public uint Version;        // 1
    public uint Width;          // عرض الفريم
    public uint Height;         // ارتفاع الفريم
    public uint Stride;         // بايتات لكل صف
    public uint Format;         // 0 = BGRA32
    public ulong Timestamp;     // وقت الفريم
    public ulong FrameNumber;   // رقم الفريم التسلسلي
    public uint DataSize;       // حجم البيانات
    public uint Ready;          // 1 = فريم جاهز
}
```

### VideoFrame
```csharp
public class VideoFrame
{
    public int Width { get; set; }
    public int Height { get; set; }
    public int Stride { get; set; }
    public byte[] Data { get; set; }      // بيانات BGRA
    public ulong FrameNumber { get; set; }
    public DateTime Timestamp { get; set; }
    
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

## ملف السجل (Log)

عند حدوث أي مشكلة، تحقق من ملف السجل في Chiaki:
```
%APPDATA%/chiaki-ng/framesharing.log
```

## ملاحظات مهمة

1. **يجب تشغيل Chiaki أولاً** وبدء البث قبل تشغيل برنامج C#
2. **Frame Sharing يبدأ تلقائياً** عند بدء البث
3. **الفريمات بصيغة BGRA32** (Blue, Green, Red, Alpha) - 4 بايت لكل بكسل
4. **لا يوجد نسخ إضافي** - البيانات تُقرأ مباشرة من الذاكرة المشتركة
5. **آمن للاستخدام مع الـ threads** - يمكنك القراءة من thread منفصل

## استكشاف الأخطاء

### "Chiaki is not running"
- تأكد من أن Chiaki يعمل ومتصل بـ PlayStation
- تأكد من بدء البث (يجب أن ترى الفيديو في Chiaki)

### الفريمات متقطعة
- تحقق من سرعة المعالجة في برنامجك
- استخدم `receiver.DroppedFrames` لمعرفة عدد الفريمات المفقودة

### حجم الذاكرة
- الذاكرة المشتركة تستخدم حوالي (Width × Height × 4) + 64 بايت
- مثال: 1920×1080 = ~8 MB

