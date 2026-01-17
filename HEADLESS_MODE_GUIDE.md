# دليل الوضع الخفي (Headless Mode) - Chiaki-ng

## 📋 ملخص التغييرات

### الملفات الجديدة

#### 1. `gui/include/headlessbackend.h`
- Backend جديد يعمل بدون واجهة رسومية
- يدعم جميع وظائف API
- متوافق مع QmlBackend

#### 2. `gui/src/headlessbackend.cpp`
- تنفيذ HeadlessBackend الكامل
- معالجة الفريمات ونقلها عبر الذاكرة المشتركة فقط
- **لا يوجد rendering أو معالجة صورة** = توفير موارد

### الملفات المعدلة

#### 1. `gui/src/main.cpp`
- ✅ إضافة خيار `--headless` لتشغيل بدون واجهة
- ✅ إضافة خيار `--api-port` لتحديد بورت API
- ✅ استخدام `QT_QPA_PLATFORM=offscreen` في الوضع الخفي

#### 2. `gui/include/apiserver.h` & `gui/src/apiserver.cpp`
- ✅ دعم HeadlessBackend
- ✅ API تعمل في كلا الوضعين (GUI و Headless)
- ✅ إضافة `setHeadlessBackend()` و `isHeadless()`

#### 3. `gui/CMakeLists.txt`
- ✅ إضافة الملفات الجديدة للبناء

---

## 🚀 طريقة الاستخدام

### 1. الوضع الخفي (Headless Mode)

```bash
# تشغيل بدون واجهة - API فقط
chiaki.exe --headless

# مع تحديد بورت مختلف
chiaki.exe --headless --api-port 8080

# مع profile محدد
chiaki.exe --headless --profile "MyProfile"
```

### 2. الوضع العادي (GUI Mode)

```bash
# تشغيل عادي مع واجهة
chiaki.exe
```

---

## 📡 API Endpoints المتاحة

جميع الـ endpoints تعمل على: **http://127.0.0.1:5218** (أو البورت المحدد)

### معلومات API
```
GET  /              → معلومات الـ API والـ endpoints المتاحة
```

### إدارة الأجهزة
```
GET  /hosts         → قائمة الأجهزة المكتشفة والمسجلة
POST /register      → تسجيل كونسول جديد
POST /wakeup        → إيقاظ الكونسول
```

### التحكم بالبث
```
POST /connect       → الاتصال بجهاز
POST /disconnect    → قطع الاتصال
GET  /stream/status → حالة البث الحالي
```

### الإعدادات
```
GET  /settings         → الحصول على جميع الإعدادات
PUT  /settings         → تحديث الإعدادات
GET  /settings/video   → إعدادات الفيديو
PUT  /settings/video   → تحديث إعدادات الفيديو
GET  /settings/devices → الحصول على قائمة الأجهزة الصوتية المتاحة
```

---

## 💡 أمثلة الاستخدام

### 1. الحصول على قائمة الأجهزة الصوتية المتاحة

```bash
curl http://127.0.0.1:5218/settings/devices
```

**الرد:**
```json
{
  "success": true,
  "devices": {
    "input": [
      "Microphone (Realtek Audio)",
      "Line In (Realtek Audio)",
      "Stereo Mix (Realtek Audio)"
    ],
    "output": [
      "Speakers (Realtek Audio)",
      "Headphones (Realtek Audio)",
      "Digital Audio (S/PDIF) (Realtek Audio)"
    ],
    "currentInput": "Auto",
    "currentOutput": "Speakers (Realtek Audio)"
  }
}
```

**الاستخدام:**
- استخدم هذا الـ endpoint لمعرفة الأجهزة الصوتية المتاحة على النظام
- ثم استخدم اسم الجهاز في `PUT /settings` لتحديد `audioInDevice` أو `audioOutDevice`
- استخدم `"Auto"` للسماح للتطبيق باختيار الجهاز تلقائياً

**مثال - تحديث جهاز الصوت:**
```bash
# 1. احصل على قائمة الأجهزة
curl http://127.0.0.1:5218/settings/devices

# 2. حدّث جهاز الصوت باستخدام اسم الجهاز من القائمة
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{
    "audioOutDevice": "Headphones (Realtek Audio)",
    "audioInDevice": "Microphone (Realtek Audio)"
  }'
```

### 2. قائمة الأجهزة (PlayStation)

```bash
curl http://127.0.0.1:5218/hosts
```

**الرد:**
```json
{
  "success": true,
  "count": 1,
  "hosts": [
    {
      "name": "PS5-123",
      "address": "192.168.1.100",
      "mac": "aabbccddeeff",
      "ps5": true,
      "registered": true,
      "state": "ready",
      "discovered": true
    }
  ]
}
```

### 2. الاتصال بجهاز

**باستخدام index:**
```bash
curl -X POST http://127.0.0.1:5218/connect \
  -H "Content-Type: application/json" \
  -d '{"index": 0}'
```

**باستخدام nickname:**
```bash
curl -X POST http://127.0.0.1:5218/connect \
  -H "Content-Type: application/json" \
  -d '{"nickname": "PS5-123"}'
```

**باستخدام address:**
```bash
curl -X POST http://127.0.0.1:5218/connect \
  -H "Content-Type: application/json" \
  -d '{"address": "192.168.1.100"}'
```

### 3. قطع الاتصال

```bash
curl -X POST http://127.0.0.1:5218/disconnect
```

### 4. إيقاظ الكونسول

```bash
curl -X POST http://127.0.0.1:5218/wakeup \
  -H "Content-Type: application/json" \
  -d '{"index": 0}'
```

### 5. حالة البث

```bash
curl http://127.0.0.1:5218/stream/status
```

**الرد:**
```json
{
  "success": true,
  "headless": true,
  "streaming": true,
  "connected": true,
  "host": "192.168.1.100",
  "bitrate": 15000,
  "packetLoss": 0.5,
  "muted": false
}
```

### 6. تحديث الإعدادات

```bash
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{
    "localRenderDisabled": true,
    "frameSharingEnabled": true,
    "hideCursor": true
  }'
```

**مثال شامل - تحديث جهاز الصوت:**
```bash
# 1. احصل على قائمة الأجهزة الصوتية المتاحة
curl http://127.0.0.1:5218/settings/devices

# 2. حدّث جهاز الصوت باستخدام اسم الجهاز من القائمة
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{
    "audioOutDevice": "Headphones (Realtek Audio)",
    "audioInDevice": "Microphone (Realtek Audio)",
    "audioVolume": 80
  }'
```

**ملاحظة:** 
- استخدم `"Auto"` للسماح للتطبيق باختيار الجهاز تلقائياً
- يجب استخدام الأسماء الدقيقة للأجهزة كما تظهر في `/settings/devices`

### 7. تسجيل كونسول جديد

```bash
curl -X POST http://127.0.0.1:5218/register \
  -H "Content-Type: application/json" \
  -d '{
    "host": "192.168.1.100",
    "psn_id": "your_psn_id",
    "pin": "12345678",
    "target": "ps5"
  }'
```

---

## 🖼️ نقل الفريمات عبر الذاكرة المشتركة

في الوضع الخفي، الفريمات تُنقل عبر الذاكرة المشتركة فقط (بدون عرض).

### اسم الذاكرة المشتركة
```
ChiakiFrameShare
```

### صيغة الصورة
- **Format**: BGRA (32-bit)
- **Stride**: width * 4 bytes

### في C# - قراءة الفريمات:

```csharp
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;

// فتح الذاكرة المشتركة
var mmf = MemoryMappedFile.OpenExisting("ChiakiFrameShare");
var accessor = mmf.CreateViewAccessor();

// قراءة الـ Header
[StructLayout(LayoutKind.Sequential, Pack = 1)]
struct FrameSharingHeader {
    public uint magic;        // 0x4B414843 "CHAK"
    public uint version;      // 3
    public uint maxWidth;
    public uint maxHeight;
    
    // Buffer 0 metadata
    public uint width0;
    public uint height0;
    public uint stride0;
    public uint dataSize0;
    public ulong timestamp0;
    public ulong frameNumber0;
    
    // Buffer 1 metadata
    public uint width1;
    public uint height1;
    public uint stride1;
    public uint dataSize1;
    public ulong timestamp1;
    public ulong frameNumber1;
    
    // Synchronization
    public int writeBuffer;
    public int readyBuffer;   // -1 = none, 0 or 1 = ready
    
    // Performance counters
    public ulong totalFramesWritten;
    public ulong totalFramesRead;
    public ulong droppedFrames;
}

// قراءة الـ Header
FrameSharingHeader header;
accessor.Read(0, out header);

// معرفة أي buffer جاهز
int readyBuffer = header.readyBuffer;
if (readyBuffer >= 0) {
    // حساب offset للـ buffer
    int bufferIndex = readyBuffer;
    ulong singleBufferSize = (ulong)header.maxWidth * header.maxHeight * 4;
    ulong offset = (ulong)sizeof(FrameSharingHeader) + 
                   ((ulong)bufferIndex * singleBufferSize);
    
    // قراءة البيانات
    uint width = (bufferIndex == 0) ? header.width0 : header.width1;
    uint height = (bufferIndex == 0) ? header.height0 : header.height1;
    uint stride = (bufferIndex == 0) ? header.stride0 : header.stride1;
    
    // قراءة الصورة (BGRA format)
    byte[] imageData = new byte[stride * height];
    accessor.ReadArray((long)offset, imageData, 0, imageData.Length);
    
    // الآن imageData يحتوي على الصورة!
    // يمكنك تحويلها إلى Bitmap أو Image
}
```

### في C++ - قراءة الفريمات:

```cpp
#include <windows.h>
#include <iostream>

HANDLE hMapFile = OpenFileMappingW(
    FILE_MAP_READ,
    FALSE,
    L"ChiakiFrameShare"
);

if (hMapFile) {
    void* pBuf = MapViewOfFile(
        hMapFile,
        FILE_MAP_READ,
        0, 0, 0
    );
    
    if (pBuf) {
        FrameSharingHeader* header = (FrameSharingHeader*)pBuf;
        
        int readyBuffer = header->readyBuffer;
        if (readyBuffer >= 0) {
            // حساب offset
            size_t singleBufferSize = header->maxWidth * header->maxHeight * 4;
            size_t offset = sizeof(FrameSharingHeader) + 
                           (readyBuffer * singleBufferSize);
            
            uint8_t* imageData = (uint8_t*)pBuf + offset;
            uint32_t width = (readyBuffer == 0) ? header->width0 : header->width1;
            uint32_t height = (readyBuffer == 0) ? header->height0 : header->height1;
            
            // الآن imageData يحتوي على الصورة BGRA
        }
        
        UnmapViewOfFile(pBuf);
    }
    
    CloseHandle(hMapFile);
}
```

---

## 📊 مقارنة بين الوضعين

| الميزة | الوضع العادي | الوضع الخفي |
|--------|--------------|--------------|
| واجهة رسومية | ✅ | ❌ |
| API Server | ✅ | ✅ |
| API أثناء البث | ❌ (مشكلة) | ✅ |
| نقل الفريمات | ✅ | ✅ |
| معالجة العرض | ✅ | ❌ (توفير موارد) |
| استهلاك الموارد | عالي | منخفض جداً |
| استهلاك GPU | عالي | صفر |
| استهلاك CPU | عالي | منخفض |

---

## 🔧 مثال كامل - C# Console App

```csharp
using System;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

class Program {
    static async Task Main() {
        var client = new HttpClient();
        var baseUrl = "http://127.0.0.1:5218";
        
        try {
            // 1. قائمة الأجهزة
            Console.WriteLine("جاري جلب قائمة الأجهزة...");
            var hostsResponse = await client.GetStringAsync($"{baseUrl}/hosts");
            Console.WriteLine("Hosts: " + hostsResponse);
            
            // 2. الاتصال
            Console.WriteLine("جاري الاتصال...");
            var connectData = new {
                index = 0
            };
            var connectJson = JsonSerializer.Serialize(connectData);
            var connectContent = new StringContent(connectJson, Encoding.UTF8, "application/json");
            var connectResponse = await client.PostAsync($"{baseUrl}/connect", connectContent);
            Console.WriteLine("Connection: " + await connectResponse.Content.ReadAsStringAsync());
            
            // 3. انتظار قليلاً
            await Task.Delay(2000);
            
            // 4. حالة البث
            Console.WriteLine("جاري التحقق من حالة البث...");
            var statusResponse = await client.GetStringAsync($"{baseUrl}/stream/status");
            Console.WriteLine("Status: " + statusResponse);
            
            // 5. قراءة الفريمات من الذاكرة المشتركة
            // ... (استخدم الكود أعلاه)
            
            // 6. قطع الاتصال
            Console.WriteLine("جاري قطع الاتصال...");
            await client.PostAsync($"{baseUrl}/disconnect", null);
            
        } catch (Exception ex) {
            Console.WriteLine("خطأ: " + ex.Message);
        }
    }
}
```

---

## 📝 ملاحظات مهمة

1. ✅ **في الوضع الخفي**: لا توجد نافذة مرئية على الإطلاق
2. ✅ **API تعمل دائماً**: حتى أثناء البث النشط
3. ✅ **الفريمات**: تُنقل عبر الذاكرة المشتركة فقط (بدون rendering)
4. ✅ **توفير الموارد**: لا يوجد استهلاك GPU أو معالجة عرض
5. ✅ **التحكم الكامل**: يمكن التحكم بكل شيء عبر API

---

## 🐛 استكشاف الأخطاء

### المشكلة: API لا تعمل
**الحل**: تأكد أن Chiaki يعمل في الوضع الخفي:
```bash
chiaki.exe --headless
```

### المشكلة: لا يمكن قراءة الفريمات
**الحل**: تأكد من تفعيل Frame Sharing:
```bash
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{"frameSharingEnabled": true}'
```

### المشكلة: البورت مشغول
**الحل**: استخدم بورت مختلف:
```bash
chiaki.exe --headless --api-port 8080
```

---

## 📚 مراجع إضافية

- API Documentation: `http://127.0.0.1:5218/`
- Frame Sharing Header: موجود في `gui/include/framesharing.h`
- Source Code: `gui/src/headlessbackend.cpp`

---

**تم التطوير بواسطة**: Urscript Team  
**التاريخ**: 2026-01-14  
**الإصدار**: 1.9.9
