# 📡 Urscript API Documentation

## 🎯 نظرة عامة

Urscript API هو واجهة برمجية محلية (Local API) تسمح بالتحكم الكامل في تطبيق Urscript (Chiaki-ng) عبر HTTP. تعمل الـ API على `http://127.0.0.1:5218` بشكل افتراضي.

### المميزات الرئيسية:
- ✅ **التحكم الكامل**: إدارة الأجهزة، الاتصال، الإعدادات
- ✅ **وضعين**: يعمل في الوضع العادي (GUI) والوضع الخفي (Headless)
- ✅ **CORS Enabled**: يدعم الطلبات من المتصفحات
- ✅ **JSON Format**: جميع الطلبات والردود بصيغة JSON
- ✅ **Schema Support**: معلومات كاملة عن القيم المسموحة لكل إعداد

---

## 🚀 البدء السريع

### 1. تشغيل التطبيق

```bash
# الوضع العادي (مع واجهة)
lu.exe

# الوضع الخفي (بدون واجهة - API فقط)
lu.exe --headless

# مع بورت مخصص
lu.exe --headless --api-port 8080
```

### 2. اختبار الاتصال

```bash
# الحصول على معلومات API
curl http://127.0.0.1:5218/

# أو في المتصفح
http://127.0.0.1:5218/
```

---

## 📋 قائمة الـ Endpoints

### معلومات API
- `GET /` - معلومات API والـ endpoints المتاحة

### إدارة الأجهزة
- `GET /hosts` - قائمة الأجهزة المكتشفة والمسجلة
- `POST /register` - تسجيل كونسول جديد
- `POST /wakeup` - إيقاظ الكونسول

### التحكم بالبث
- `POST /connect` - الاتصال بجهاز
- `POST /disconnect` - قطع الاتصال
- `GET /stream/status` - حالة البث الحالي

### الإعدادات
- `GET /settings` - الحصول على جميع الإعدادات
- `PUT /settings` - تحديث الإعدادات
- `GET /settings/video` - إعدادات الفيديو
- `PUT /settings/video` - تحديث إعدادات الفيديو
- `GET /settings/devices` - قائمة الأجهزة الصوتية المتاحة

---

## 📖 تفاصيل الـ Endpoints

### 1. معلومات API

#### `GET /`

**الوصف**: يعيد معلومات عن API والـ endpoints المتاحة

**الرد**:
```json
{
  "name": "Remote Controller API",
  "version": "2.0",
  "endpoints": [
    {
      "method": "GET",
      "path": "/hosts",
      "description": "Get discovered and registered hosts"
    },
    {
      "method": "POST",
      "path": "/register",
      "description": "Register a console"
    },
    ...
  ]
}
```

**مثال**:
```bash
curl http://127.0.0.1:5218/
```

---

### 2. إدارة الأجهزة

#### `GET /hosts`

**الوصف**: يعيد قائمة بجميع الأجهزة (PlayStation) المكتشفة والمسجلة

**الرد**:
```json
{
  "success": true,
  "count": 2,
  "hosts": [
    {
      "name": "PS5-123",
      "address": "192.168.1.100",
      "mac": "aabbccddeeff",
      "ps5": true,
      "state": "ready",
      "registered": true,
      "discovered": true,
      "runningApp": "Game Title",
      "titleId": "CUSA12345"
    },
    {
      "name": "PS4-456",
      "address": "192.168.1.101",
      "mac": "112233445566",
      "ps5": false,
      "state": "standby",
      "registered": true,
      "discovered": false
    }
  ]
}
```

**الحقول**:
- `name`: اسم الجهاز
- `address`: عنوان IP
- `mac`: عنوان MAC
- `ps5`: `true` إذا كان PS5، `false` إذا كان PS4
- `state`: حالة الجهاز (`ready`, `standby`, `busy`)
- `registered`: `true` إذا كان مسجل
- `discovered`: `true` إذا تم اكتشافه تلقائياً
- `runningApp`: اسم التطبيق/اللعبة قيد التشغيل (إن وجد)
- `titleId`: معرف اللعبة (إن وجد)

**مثال**:
```bash
curl http://127.0.0.1:5218/hosts
```

---

#### `POST /register`

**الوصف**: تسجيل كونسول جديد

**الطلب**:
```json
{
  "host": "192.168.1.100",
  "psn_id": "your_psn_id",
  "pin": "12345678",
  "console_pin": "",
  "target": "ps5"
}
```

**الحقول المطلوبة**:
- `host`: عنوان IP للكونسول (أو `"255.255.255.255"` للبث العام)
- `psn_id`: معرف PSN الخاص بك
- `pin`: رمز PIN المكون من 8 أرقام (يظهر على شاشة الكونسول)
- `console_pin`: (اختياري) رمز PIN إضافي للكونسول
- `target`: نوع الكونسول (`"ps4"`, `"ps4_7"`, `"ps4_75"`, `"ps4_8"`, `"ps5"`)

**الرد**:
```json
{
  "success": true,
  "message": "Registration process started"
}
```

**أو في حالة الخطأ**:
```json
{
  "success": false,
  "error": "Missing required field: host"
}
```

**مثال**:
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

#### `POST /wakeup`

**الوصف**: إيقاظ كونسول من وضع الاستعداد

**الطلب**:
```json
{
  "index": 0
}
```

**أو**:
```json
{
  "nickname": "PS5-123"
}
```

**أو**:
```json
{
  "address": "192.168.1.100"
}
```

**الرد**:
```json
{
  "success": true,
  "message": "Wakeup signal sent"
}
```

**مثال**:
```bash
curl -X POST http://127.0.0.1:5218/wakeup \
  -H "Content-Type: application/json" \
  -d '{"index": 0}'
```

---

### 3. التحكم بالبث

#### `POST /connect`

**الوصف**: الاتصال بجهاز PlayStation وبدء البث

**الطلب**:
```json
{
  "index": 0
}
```

**أو**:
```json
{
  "nickname": "PS5-123"
}
```

**أو**:
```json
{
  "address": "192.168.1.100"
}
```

**الرد**:
```json
{
  "success": true,
  "message": "Connection initiated",
  "index": 0
}
```

**مثال**:
```bash
curl -X POST http://127.0.0.1:5218/connect \
  -H "Content-Type: application/json" \
  -d '{"index": 0}'
```

---

#### `POST /disconnect`

**الوصف**: قطع الاتصال وإنهاء البث الحالي

**الرد**:
```json
{
  "success": true,
  "message": "Disconnect requested"
}
```

**مثال**:
```bash
curl -X POST http://127.0.0.1:5218/disconnect
```

---

#### `GET /stream/status`

**الوصف**: الحصول على حالة البث الحالية

**الرد**:
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

**الحقول**:
- `headless`: `true` إذا كان في الوضع الخفي
- `streaming`: `true` إذا كان البث نشطاً
- `connected`: `true` إذا كان متصل
- `host`: عنوان IP للجهاز المتصل
- `bitrate`: معدل البت الحالي (bps)
- `packetLoss`: نسبة فقدان الحزم (0.0 - 1.0)
- `muted`: `true` إذا كان الصوت معطلاً

**مثال**:
```bash
curl http://127.0.0.1:5218/stream/status
```

---

### 4. الإعدادات

#### `GET /settings`

**الوصف**: الحصول على جميع الإعدادات مع قيمها الحالية ومعلومات Schema

**الرد**:
```json
{
  "success": true,
  "general": {
    "hardwareDecoder": "auto",
    "hideCursor": false,
    "windowType": "selected_resolution",
    "placeboPreset": "default",
    "frameSharingEnabled": true,
    "localRenderDisabled": false,
    "showStreamStats": false,
    "audioOutDevice": "Auto",
    "audioInDevice": "Auto",
    "audioVolume": 100,
    "audioBufferSize": 0,
    "audioVideoDisabled": 0,
    "wifiDroppedNotif": 10,
    "discoveryEnabled": true,
    "keyboardEnabled": true,
    "mouseTouchEnabled": false,
    "dpadTouchEnabled": false,
    "buttonsByPosition": false,
    "allowJoystickBackgroundEvents": true,
    "rumbleHapticsIntensity": "normal",
    "hapticOverride": 1.0,
    "streamerMode": false,
    "automaticConnect": true,
    "startMicUnmuted": false,
    "fullscreenDoubleClickEnabled": false,
    "displayTargetContrast": 1000,
    "displayTargetPeak": 1000,
    "displayTargetTrc": 1,
    "displayTargetPrim": 1,
    "customResolutionWidth": 0,
    "customResolutionHeight": 0,
    "zoomFactor": 1.0,
    "packetLossMax": 0.1,
    "logVerbose": false
  },
  "video": {
    "ps5_local": {
      "resolution": 1080,
      "fps": 60,
      "bitrate": 15000,
      "codec": "h265"
    },
    "ps5_remote": {
      "resolution": 1080,
      "fps": 60,
      "bitrate": 15000,
      "codec": "h265"
    },
    "ps4_local": {
      "resolution": 720,
      "fps": 60,
      "bitrate": 10000
    },
    "ps4_remote": {
      "resolution": 720,
      "fps": 60,
      "bitrate": 10000
    }
  },
  "schema": {
    "general": {
      "windowType": {
        "type": "string",
        "allowedValues": ["selected_resolution", "custom_resolution", "fullscreen", "zoom", "stretch"]
      },
      "placeboPreset": {
        "type": "string",
        "allowedValues": ["fast", "default", "high_quality", "custom"]
      },
      "rumbleHapticsIntensity": {
        "type": "string",
        "allowedValues": ["off", "very_weak", "weak", "normal", "strong", "very_strong"]
      },
      "audioOutDevice": {
        "type": "string",
        "description": "Use GET /settings/devices to get available devices"
      },
      "audioInDevice": {
        "type": "string",
        "description": "Use GET /settings/devices to get available devices"
      },
      "audioVolume": {
        "type": "integer",
        "min": 0,
        "max": 128
      },
      "audioVideoDisabled": {
        "type": "integer",
        "allowedValues": [0, 1, 2, 3],
        "description": "0=Both Enabled, 1=Audio Disabled, 2=Video Disabled, 3=Both Disabled"
      },
      ...
    }
  }
}
```

**مثال**:
```bash
curl http://127.0.0.1:5218/settings
```

---

#### `PUT /settings`

**الوصف**: تحديث الإعدادات العامة

**الطلب**:
```json
{
  "audioOutDevice": "Speakers (Realtek Audio)",
  "audioInDevice": "Microphone (Realtek Audio)",
  "audioVolume": 80,
  "hideCursor": true,
  "windowType": "fullscreen",
  "rumbleHapticsIntensity": "strong"
}
```

**الرد**:
```json
{
  "success": true,
  "updated": [
    "audioOutDevice",
    "audioInDevice",
    "audioVolume",
    "hideCursor",
    "windowType",
    "rumbleHapticsIntensity"
  ]
}
```

**جميع الإعدادات المتاحة**:

##### الإعدادات الأساسية
- `hardwareDecoder` (string): `"auto"`, `"none"`, `"d3d11va"`, `"dxva2"`, `"videotoolbox"`, `"vaapi"`, `"vulkan"`
- `hideCursor` (boolean): إخفاء المؤشر
- `windowType` (string): `"selected_resolution"`, `"custom_resolution"`, `"fullscreen"`, `"zoom"`, `"stretch"`
- `placeboPreset` (string): `"fast"`, `"default"`, `"high_quality"`, `"custom"`

##### إعدادات Frame Sharing
- `frameSharingEnabled` (boolean): تفعيل مشاركة الفريمات عبر الذاكرة المشتركة
- `localRenderDisabled` (boolean): تعطيل العرض المحلي (للوضع الخفي)
- `showStreamStats` (boolean): عرض إحصائيات البث

##### إعدادات الصوت
- `audioOutDevice` (string): جهاز الصوت الخرج (استخدم `GET /settings/devices` للحصول على القائمة)
- `audioInDevice` (string): جهاز الصوت الدخل (استخدم `GET /settings/devices` للحصول على القائمة)
- `audioVolume` (integer): مستوى الصوت (0-128)
- `audioBufferSize` (integer): حجم الـ buffer (0 = تلقائي)
- `audioVideoDisabled` (integer): `0`=كلاهما مفعل، `1`=تعطيل الصوت، `2`=تعطيل الفيديو، `3`=تعطيل كلاهما

##### إعدادات الشبكة
- `wifiDroppedNotif` (integer): عتبة إشعار انقطاع WiFi (0-100)
- `discoveryEnabled` (boolean): تفعيل الاكتشاف التلقائي

##### إعدادات التحكم
- `keyboardEnabled` (boolean): تفعيل لوحة المفاتيح
- `mouseTouchEnabled` (boolean): تفعيل اللمس بالماوس
- `dpadTouchEnabled` (boolean): تفعيل اللمس للـ D-Pad
- `buttonsByPosition` (boolean): الأزرار حسب الموضع
- `allowJoystickBackgroundEvents` (boolean): السماح بأحداث الـ joystick في الخلفية

##### إعدادات الاهتزاز
- `rumbleHapticsIntensity` (string): `"off"`, `"very_weak"`, `"weak"`, `"normal"`, `"strong"`, `"very_strong"`
- `hapticOverride` (number): قيمة تجاوز الاهتزاز (0.0 - 1.0)

##### إعدادات البث
- `streamerMode` (boolean): وضع البث المباشر
- `automaticConnect` (boolean): الاتصال التلقائي
- `startMicUnmuted` (boolean): بدء الميكروفون غير معطل
- `fullscreenDoubleClickEnabled` (boolean): تفعيل النقر المزدوج للشاشة الكاملة

##### إعدادات العرض
- `displayTargetContrast` (integer): تباين العرض المستهدف
- `displayTargetPeak` (integer): ذروة العرض المستهدفة
- `displayTargetTrc` (integer): منحنى نقل اللون
- `displayTargetPrim` (integer): مساحة الألوان الأولية
- `customResolutionWidth` (integer): عرض الدقة المخصصة
- `customResolutionHeight` (integer): ارتفاع الدقة المخصصة
- `zoomFactor` (number): عامل التكبير (0.1 - 10.0)
- `packetLossMax` (number): الحد الأقصى لفقدان الحزم (0.0 - 1.0)

##### إعدادات السجل
- `logVerbose` (boolean): تفعيل السجل المفصل

**مثال**:
```bash
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{
    "audioOutDevice": "Headphones (Realtek Audio)",
    "audioVolume": 80,
    "hideCursor": true
  }'
```

---

#### `GET /settings/video`

**الوصف**: الحصول على إعدادات الفيديو فقط

**الرد**:
```json
{
  "success": true,
  "ps5": {
    "local": {
      "resolution": "1080p",
      "fps": "60",
      "bitrate": 15000,
      "codec": "h265"
    },
    "remote": {
      "resolution": "1080p",
      "fps": "60",
      "bitrate": 15000,
      "codec": "h265"
    }
  },
  "ps4": {
    "local": {
      "resolution": "720p",
      "fps": "60",
      "bitrate": 10000
    },
    "remote": {
      "resolution": "720p",
      "fps": "60",
      "bitrate": 10000
    }
  }
}
```

**القيم المسموحة**:
- `resolution`: `"360p"`, `"540p"`, `"720p"`, `"1080p"`
- `fps`: `"30"`, `"60"`
- `bitrate`: عدد صحيح (عادة 5000-50000)
- `codec`: `"h264"`, `"h265"` (PS5 فقط)

**مثال**:
```bash
curl http://127.0.0.1:5218/settings/video
```

---

#### `PUT /settings/video`

**الوصف**: تحديث إعدادات الفيديو

**الطلب**:
```json
{
  "ps5_local": {
    "resolution": "1080p",
    "fps": "60",
    "bitrate": 20000,
    "codec": "h265"
  },
  "ps5_remote": {
    "resolution": "720p",
    "fps": "60",
    "bitrate": 12000,
    "codec": "h264"
  },
  "ps4_local": {
    "resolution": "720p",
    "fps": "60",
    "bitrate": 10000
  },
  "ps4_remote": {
    "resolution": "540p",
    "fps": "30",
    "bitrate": 8000
  }
}
```

**الرد**:
```json
{
  "success": true,
  "updated": [
    "ps5_local.resolution",
    "ps5_local.bitrate",
    "ps4_remote.resolution",
    "ps4_remote.fps"
  ]
}
```

**مثال**:
```bash
curl -X PUT http://127.0.0.1:5218/settings/video \
  -H "Content-Type: application/json" \
  -d '{
    "ps5_local": {
      "resolution": "1080p",
      "fps": "60",
      "bitrate": 20000,
      "codec": "h265"
    }
  }'
```

---

#### `GET /settings/devices`

**الوصف**: الحصول على قائمة الأجهزة الصوتية المتاحة على النظام

**الرد**:
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

**الحقول**:
- `input`: قائمة أجهزة الصوت الدخل المتاحة
- `output`: قائمة أجهزة الصوت الخرج المتاحة
- `currentInput`: الجهاز الحالي للدخل (`"Auto"` أو اسم الجهاز)
- `currentOutput`: الجهاز الحالي للخرج (`"Auto"` أو اسم الجهاز)

**الاستخدام**:
1. استدعي هذا الـ endpoint للحصول على قائمة الأجهزة
2. استخدم اسم الجهاز الدقيق في `PUT /settings` لتحديد `audioInDevice` أو `audioOutDevice`
3. استخدم `"Auto"` للسماح للتطبيق باختيار الجهاز تلقائياً

**مثال**:
```bash
curl http://127.0.0.1:5218/settings/devices
```

**مثال - تحديث جهاز الصوت**:
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

---

## 💡 أمثلة الاستخدام الكاملة

### مثال 1: سيناريو كامل - الاتصال والتحكم

```bash
# 1. الحصول على قائمة الأجهزة
curl http://127.0.0.1:5218/hosts

# 2. الاتصال بجهاز PS5
curl -X POST http://127.0.0.1:5218/connect \
  -H "Content-Type: application/json" \
  -d '{"index": 0}'

# 3. التحقق من حالة البث
curl http://127.0.0.1:5218/stream/status

# 4. تحديث إعدادات الصوت
curl -X PUT http://127.0.0.1:5218/settings \
  -H "Content-Type: application/json" \
  -d '{
    "audioVolume": 90,
    "audioOutDevice": "Headphones (Realtek Audio)"
  }'

# 5. قطع الاتصال
curl -X POST http://127.0.0.1:5218/disconnect
```

---

### مثال 2: استخدام JavaScript (Browser)

```javascript
const API_BASE = 'http://127.0.0.1:5218';

// الحصول على قائمة الأجهزة
async function getHosts() {
  const response = await fetch(`${API_BASE}/hosts`);
  const data = await response.json();
  console.log('Hosts:', data);
  return data;
}

// الاتصال بجهاز
async function connect(index) {
  const response = await fetch(`${API_BASE}/connect`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ index })
  });
  const data = await response.json();
  console.log('Connection:', data);
  return data;
}

// الحصول على حالة البث
async function getStreamStatus() {
  const response = await fetch(`${API_BASE}/stream/status`);
  const data = await response.json();
  console.log('Status:', data);
  return data;
}

// تحديث الإعدادات
async function updateSettings(settings) {
  const response = await fetch(`${API_BASE}/settings`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(settings)
  });
  const data = await response.json();
  console.log('Updated:', data);
  return data;
}

// استخدام
(async () => {
  const hosts = await getHosts();
  if (hosts.count > 0) {
    await connect(0);
    await updateSettings({ audioVolume: 80 });
    setInterval(getStreamStatus, 1000);
  }
})();
```

---

### مثال 3: استخدام Python

```python
import requests
import json

API_BASE = "http://127.0.0.1:5218"

# الحصول على قائمة الأجهزة
def get_hosts():
    response = requests.get(f"{API_BASE}/hosts")
    return response.json()

# الاتصال بجهاز
def connect(index):
    response = requests.post(
        f"{API_BASE}/connect",
        json={"index": index}
    )
    return response.json()

# الحصول على حالة البث
def get_stream_status():
    response = requests.get(f"{API_BASE}/stream/status")
    return response.json()

# تحديث الإعدادات
def update_settings(settings):
    response = requests.put(
        f"{API_BASE}/settings",
        json=settings
    )
    return response.json()

# الحصول على الأجهزة الصوتية
def get_audio_devices():
    response = requests.get(f"{API_BASE}/settings/devices")
    return response.json()

# استخدام
if __name__ == "__main__":
    # الحصول على الأجهزة
    hosts = get_hosts()
    print(f"Found {hosts['count']} hosts")
    
    if hosts['count'] > 0:
        # الاتصال
        result = connect(0)
        print(f"Connection: {result}")
        
        # الحصول على الأجهزة الصوتية
        devices = get_audio_devices()
        print(f"Audio devices: {devices}")
        
        # تحديث الإعدادات
        update_settings({
            "audioVolume": 80,
            "audioOutDevice": devices["devices"]["output"][0] if devices["devices"]["output"] else "Auto"
        })
        
        # مراقبة حالة البث
        import time
        while True:
            status = get_stream_status()
            print(f"Streaming: {status['streaming']}, Bitrate: {status.get('bitrate', 0)}")
            time.sleep(1)
```

---

### مثال 4: استخدام C# (HttpClient)

```csharp
using System;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

class UrscriptAPI
{
    private static readonly HttpClient client = new HttpClient();
    private const string API_BASE = "http://127.0.0.1:5218";

    public static async Task<JsonDocument> GetHosts()
    {
        var response = await client.GetStringAsync($"{API_BASE}/hosts");
        return JsonDocument.Parse(response);
    }

    public static async Task<JsonDocument> Connect(int index)
    {
        var json = JsonSerializer.Serialize(new { index });
        var content = new StringContent(json, Encoding.UTF8, "application/json");
        var response = await client.PostAsync($"{API_BASE}/connect", content);
        var responseText = await response.Content.ReadAsStringAsync();
        return JsonDocument.Parse(responseText);
    }

    public static async Task<JsonDocument> GetStreamStatus()
    {
        var response = await client.GetStringAsync($"{API_BASE}/stream/status");
        return JsonDocument.Parse(response);
    }

    public static async Task<JsonDocument> UpdateSettings(object settings)
    {
        var json = JsonSerializer.Serialize(settings);
        var content = new StringContent(json, Encoding.UTF8, "application/json");
        var response = await client.PutAsync($"{API_BASE}/settings", content);
        var responseText = await response.Content.ReadAsStringAsync();
        return JsonDocument.Parse(responseText);
    }

    public static async Task<JsonDocument> GetAudioDevices()
    {
        var response = await client.GetStringAsync($"{API_BASE}/settings/devices");
        return JsonDocument.Parse(response);
    }
}

// استخدام
class Program
{
    static async Task Main()
    {
        var hosts = await UrscriptAPI.GetHosts();
        Console.WriteLine($"Found {hosts.RootElement.GetProperty("count").GetInt32()} hosts");

        if (hosts.RootElement.GetProperty("count").GetInt32() > 0)
        {
            var connectResult = await UrscriptAPI.Connect(0);
            Console.WriteLine($"Connection: {connectResult.RootElement.GetProperty("success")}");

            var devices = await UrscriptAPI.GetAudioDevices();
            var outputDevices = devices.RootElement.GetProperty("devices").GetProperty("output");
            
            await UrscriptAPI.UpdateSettings(new
            {
                audioVolume = 80,
                audioOutDevice = outputDevices[0].GetString()
            });
        }
    }
}
```

---

## 🔧 معالجة الأخطاء

### رموز الحالة HTTP

- `200 OK`: الطلب نجح
- `400 Bad Request`: طلب غير صحيح (JSON غير صالح، حقول مفقودة)
- `404 Not Found`: الـ endpoint غير موجود
- `500 Internal Server Error`: خطأ في الخادم

### تنسيق الخطأ

جميع الأخطاء تعيد JSON بهذا الشكل:

```json
{
  "success": false,
  "error": "Error message here",
  "status": 400
}
```

### أخطاء شائعة

#### 1. "Host not found"
**السبب**: لم يتم العثور على الجهاز بالـ index أو nickname أو address المحدد

**الحل**: استخدم `GET /hosts` للتحقق من الأجهزة المتاحة

#### 2. "Invalid JSON"
**السبب**: JSON غير صالح في الطلب

**الحل**: تحقق من صحة JSON باستخدام validator

#### 3. "Missing required field: X"
**السبب**: حقل مطلوب مفقود في الطلب

**الحل**: تأكد من إرسال جميع الحقول المطلوبة

#### 4. Connection Refused
**السبب**: التطبيق غير قيد التشغيل أو البورت غير صحيح

**الحل**: تأكد من تشغيل `lu.exe` (أو `lu.exe --headless`)

---

## 📝 ملاحظات مهمة

1. **CORS**: الـ API يدعم CORS ويمكن استخدامه من المتصفحات
2. **Local Only**: الـ API يعمل فقط على `127.0.0.1` (localhost) لأسباب أمنية
3. **JSON Only**: جميع الطلبات والردود بصيغة JSON
4. **Case Sensitive**: أسماء الحقول حساسة لحالة الأحرف
5. **Schema**: استخدم `GET /settings` للحصول على Schema كامل للإعدادات
6. **Audio Devices**: استخدم `GET /settings/devices` للحصول على أسماء الأجهزة الصوتية الدقيقة
7. **Auto Device**: استخدم `"Auto"` أو `""` للسماح بالتحديد التلقائي للأجهزة الصوتية

---

## 🔗 روابط مفيدة

- **Headless Mode Guide**: راجع `HEADLESS_MODE_GUIDE.md` لمزيد من التفاصيل
- **Frame Sharing**: راجع `HEADLESS_MODE_GUIDE.md` لتفاصيل نقل الفريمات عبر الذاكرة المشتركة
- **Source Code**: `gui/src/apiserver.cpp` - كود الـ API

---

## 📞 الدعم

للمزيد من المعلومات أو المساعدة، راجع:
- **GitHub Repository**: [chiaki-ng](https://github.com/streetpea/chiaki-ng)
- **Documentation**: `HEADLESS_MODE_GUIDE.md`

---

**تم التطوير بواسطة**: Urscript Team  
**التاريخ**: 2026-01-14  
**الإصدار**: 2.0  
**API Version**: 2.0
