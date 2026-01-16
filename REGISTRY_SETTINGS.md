# موقع حفظ الإعدادات في الريجستري (Windows Registry)

## 📍 الموقع في الريجستري

### بدون Profile (الافتراضي)
```
HKEY_CURRENT_USER\Software\Lucifer\LuciferStore
```

### مع Profile محدد
```
HKEY_CURRENT_USER\Software\Lucifer\LuciferStore-{ProfileName}
```

مثال:
```
HKEY_CURRENT_USER\Software\Lucifer\LuciferStore-MyProfile
```

---

## 🔑 القيم المستخدمة

### Organization Name
```
"Lucifer"
```

### Application Name
```
"LuciferStore"
```

**المصدر**: `gui/src/main.cpp`
```cpp
QGuiApplication::setOrganizationName("Lucifer");
QGuiApplication::setApplicationName("LuciferStore");
```

---

## 📂 هيكل الإعدادات في الريجستري

### الإعدادات العامة
```
HKEY_CURRENT_USER\Software\Lucifer\LuciferStore\
  ├── settings\
  │   ├── audio_video_disabled (int) - تعطيل الصوت/الفيديو (0=لا شيء, 1=صوت, 2=فيديو, 3=كلاهما)
  │   ├── auto_discovery (bool) - الاكتشاف التلقائي للأجهزة
  │   ├── hide_cursor (bool) - إخفاء المؤشر
  │   ├── hw_decoder (string) - محرك فك التشفير (مثل "d3d11va", "none")
  │   ├── local_render_disabled (bool) - تعطيل العرض المحلي
  │   ├── frame_sharing_enabled (bool) - تفعيل مشاركة الفريمات
  │   ├── show_stream_stats (bool) - عرض إحصائيات البث
  │   ├── streamer_mode (bool) - وضع البث المباشر
  │   ├── buttons_by_pos (bool) - ترتيب الأزرار حسب الموضع
  │   ├── allow_joystick_background_events (bool) - السماح بأحداث الجويستك في الخلفية
  │   ├── start_mic_unmuted (bool) - بدء المايك بدون كتم
  │   ├── automatic_connect (bool) - الاتصال التلقائي (افتراضي: true) - يتصل تلقائياً بالجهاز المحدد عند بدء البرنامج
  │   ├── fullscreen_doubleclick (bool) - التبديل للشاشة الكاملة بالنقر المزدوج
  │   ├── haptic_override (float) - تعديل الاهتزاز (افتراضي: 1.0)
  │   ├── log_verbose (bool) - تسجيل مفصل
  │   ├── remote_play_ask (bool) - السؤال عن Remote Play
  │   ├── add_steam_shortcut_ask (bool) - السؤال عن إضافة اختصار Steam
  │   ├── rumble_haptics_intensity (string) - شدة الاهتزاز ("off", "very_weak", "weak", "normal", "strong", "very_strong")
  │   │
  │   ├── resolution_local_ps4 (string) - دقة PS4 محلي ("360p", "540p", "720p", "1080p")
  │   ├── resolution_remote_ps4 (string) - دقة PS4 عن بُعد
  │   ├── resolution_local_ps5 (string) - دقة PS5 محلي
  │   ├── resolution_remote_ps5 (string) - دقة PS5 عن بُعد
  │   │
  │   ├── fps_local_ps4 (int) - إطارات PS4 محلي (0=تلقائي, 30, 60)
  │   ├── fps_remote_ps4 (int) - إطارات PS4 عن بُعد
  │   ├── fps_local_ps5 (int) - إطارات PS5 محلي
  │   ├── fps_remote_ps5 (int) - إطارات PS5 عن بُعد
  │   │
  │   ├── bitrate_local_ps4 (uint) - معدل البت PS4 محلي (0=تلقائي)
  │   ├── bitrate_remote_ps4 (uint) - معدل البت PS4 عن بُعد
  │   ├── bitrate_local_ps5 (uint) - معدل البت PS5 محلي (افتراضي: 88000)
  │   ├── bitrate_remote_ps5 (uint) - معدل البت PS5 عن بُعد (افتراضي: 86000)
  │   │
  │   ├── codec_ps4 (string) - كودك PS4 ("h264", "h265")
  │   ├── codec_local_ps5 (string) - كودك PS5 محلي
  │   ├── codec_remote_ps5 (string) - كودك PS5 عن بُعد
  │   │
  │   ├── decoder (string) - فك التشفير ("ffmpeg", "pi")
  │   ├── placebo_preset (string) - إعداد Placebo ("fast", "default", "high_quality", "custom")
  │   ├── window_type (string) - نوع النافذة ("selected_resolution", "custom_resolution", "fullscreen", "zoom", "stretch")
  │   ├── custom_resolution_width (uint) - عرض مخصص (افتراضي: 1920)
  │   ├── custom_resolution_length (uint) - ارتفاع مخصص (افتراضي: 1080)
  │   ├── zoom_factor (float) - عامل التكبير (افتراضي: -1)
  │   ├── packet_loss_max (float) - أقصى فقدان حزم (افتراضي: 0.05)
  │   │
  │   ├── audio_volume (int) - مستوى الصوت (افتراضي: SDL_MIX_MAXVOLUME)
  │   ├── audio_buffer_size (uint) - حجم بفر الصوت (0=تلقائي)
  │   ├── audio_out_device (string) - جهاز إخراج الصوت
  │   ├── audio_in_device (string) - جهاز إدخال الصوت (المايك)
  │   ├── wifi_dropped_notif_percent (uint) - نسبة إشعار انقطاع WiFi (افتراضي: 3)
  │   │
  │   ├── keyboard_enabled (bool) - تفعيل لوحة المفاتيح
  │   ├── mouse_touch_enabled (bool) - تفعيل الماوس كـ Touch
  │   ├── dpad_touch_enabled (bool) - تفعيل D-Pad Touch
  │   ├── dpad_touch_increment (uint) - زيادة D-Pad Touch (افتراضي: 30)
  │   ├── dpad_touch_shortcut1 (uint) - اختصار D-Pad Touch 1 (افتراضي: 9)
  │   ├── dpad_touch_shortcut2 (uint) - اختصار D-Pad Touch 2 (افتراضي: 10)
  │   ├── dpad_touch_shortcut3 (uint) - اختصار D-Pad Touch 3 (افتراضي: 7)
  │   ├── dpad_touch_shortcut4 (uint) - اختصار D-Pad Touch 4 (افتراضي: 0)
  │   │
  │   ├── stream_menu_enabled (bool) - تفعيل قائمة البث
  │   ├── stream_menu_shortcut1 (uint) - اختصار قائمة البث 1 (افتراضي: 9)
  │   ├── stream_menu_shortcut2 (uint) - اختصار قائمة البث 2 (افتراضي: 10)
  │   ├── stream_menu_shortcut3 (uint) - اختصار قائمة البث 3 (افتراضي: 11)
  │   ├── stream_menu_shortcut4 (uint) - اختصار قائمة البث 4 (افتراضي: 12)
  │   │
  │   ├── enable_speech_processing (bool) - تفعيل معالجة الصوت (Speex)
  │   ├── noise_suppress_level (int) - مستوى إزالة الضوضاء (افتراضي: 6)
  │   ├── echo_suppress_level (int) - مستوى إزالة الصدى (افتراضي: 30)
  │   │
  │   ├── psn_auth_token (string) - توكن مصادقة PSN
  │   ├── psn_refresh_token (string) - توكن تحديث PSN
  │   ├── psn_auth_token_expiry (string) - تاريخ انتهاء توكن PSN
  │   ├── psn_account_id (string) - معرف حساب PSN
  │   │
  │   ├── disconnect_action (string) - إجراء قطع الاتصال ("always_nothing", "always_sleep", "ask")
  │   ├── suspend_action (string) - إجراء السكون ("nothing", "sleep")
  │   │
  │   ├── display_target_contrast (int) - تباين الشاشة المستهدفة
  │   ├── display_target_peak (int) - ذروة الشاشة المستهدفة
  │   ├── display_target_trc (int) - TRC الشاشة المستهدفة
  │   ├── display_target_prim (int) - Primaries الشاشة المستهدفة
  │   │
  │   ├── auto_connect_mac (binary) - MAC address للجهاز المطلوب الاتصال به تلقائياً (6 bytes) - يُستخدم مع automatic_connect
  │   ├── geometry (rect) - هندسة النافذة الرئيسية
  │   ├── stream_geometry (rect) - هندسة نافذة البث
  │   │
  │   └── gyro_inverted (bool) - عكس الجيروسكوب (Steam Deck)
  │   └── steamdeck_haptics (bool) - تفعيل الاهتزاز Steam Deck
  │
  ├── registered_hosts\
  │   └── [0], [1], [2]... (مصفوفة من الأجهزة المسجلة)
  │       ├── server_nickname (string) - اسم الجهاز
  │       ├── server_mac (string) - MAC address
  │       ├── target (int) - نوع الجهاز (800=PS4_7, 900=PS4_75, 1000=PS4_8, 1000100=PS5)
  │       ├── rp_regist_key (binary) - مفتاح التسجيل
  │       └── rp_key (binary) - مفتاح RP
  │
  ├── hidden_hosts\
  │   └── [0], [1], [2]... (مصفوفة من الأجهزة المخفية)
  │
  ├── manual_hosts\
  │   └── [0], [1], [2]... (مصفوفة من الأجهزة اليدوية)
  │
  ├── controller_mappings\
  │   └── (مفاتيح VID:PID -> mappings)
  │
  ├── profiles\
  │   └── [0], [1], [2]... (قائمة الملفات الشخصية)
  │
  └── version (int) - إصدار الإعدادات (افتراضي: 2)
```

---

### إعدادات Placebo (في مفتاح منفصل)
```
HKEY_CURRENT_USER\Software\Lucifer\pl_render_params\
  ├── placebo_settings\
  │   ├── version (string) - إصدار إعدادات Placebo
  │   │
  │   ├── upscaler (string) - مقياس التكبير ("none", "nearest", "bilinear", "oversample", "bicubic", "gaussian", "catmull_rom", "lanczos", "ewa_lanczos", "ewa_lanczos_sharp", "ewa_lanczos4_sharpest")
  │   ├── plane_upscaler (string) - مقياس التكبير للطائرة
  │   ├── downscaler (string) - مقياس التصغير ("none", "box", "hermite", "bilinear", "bicubic", "gaussian", "catmull_rom", "mitchell", "lanczos")
  │   ├── plane_downscaler (string) - مقياس التصغير للطائرة
  │   ├── frame_mixer (string) - خلاط الإطارات ("none", "oversample", "hermite", "linear", "cubic")
  │   ├── antiringing_strength (float) - قوة إزالة الحلقات (افتراضي: 0.0)
  │   │
  │   ├── deband (string) - تفعيل Deband ("yes", "no")
  │   ├── deband_preset (string) - إعداد Deband ("none", "default")
  │   ├── deband_iterations (int) - تكرارات Deband (افتراضي: 1)
  │   ├── deband_threshold (float) - عتبة Deband (افتراضي: 3.0)
  │   ├── deband_radius (float) - نصف قطر Deband (افتراضي: 16.0)
  │   ├── deband_grain (float) - حبيبات Deband (افتراضي: 4.0)
  │   │
  │   ├── sigmoid (string) - تفعيل Sigmoid ("yes", "no")
  │   ├── sigmoid_preset (string) - إعداد Sigmoid ("none", "default")
  │   ├── sigmoid_center (float) - مركز Sigmoid (افتراضي: 0.75)
  │   ├── sigmoid_slope (float) - منحدر Sigmoid (افتراضي: 6.5)
  │   │
  │   ├── color_adjustment (string) - تفعيل تعديل الألوان ("yes", "no")
  │   ├── color_adjustment_preset (string) - إعداد تعديل الألوان ("none", "neutral")
  │   ├── brightness (float) - السطوع (افتراضي: 0.0)
  │   ├── contrast (float) - التباين (افتراضي: 1.0)
  │   ├── saturation (float) - التشبع (افتراضي: 1.0)
  │   ├── hue (float) - الصبغة (افتراضي: 0.0)
  │   ├── gamma (float) - جاما (افتراضي: 1.0)
  │   ├── temperature (float) - درجة الحرارة (افتراضي: 0.0)
  │   │
  │   ├── peak_detect (string) - تفعيل اكتشاف الذروة ("yes", "no")
  │   ├── peak_detect_preset (string) - إعداد اكتشاف الذروة ("none", "default", "high_quality")
  │   ├── peak_smoothing_period (float) - فترة تنعيم الذروة (افتراضي: 20.0)
  │   ├── scene_threshold_low (float) - عتبة المشهد المنخفضة (افتراضي: 1.0)
  │   ├── scene_threshold_high (float) - عتبة المشهد العالية (افتراضي: 3.0)
  │   ├── peak_percentile (float) - نسبة مئوية للذروة (افتراضي: 100.0)
  │   ├── black_cutoff (float) - قطع الأسود (افتراضي: 1.0)
  │   ├── allow_delayed_peak (string) - السماح بالذروة المتأخرة ("yes", "no")
  │   │
  │   ├── color_map (string) - تفعيل خريطة الألوان ("yes", "no")
  │   ├── color_map_preset (string) - إعداد خريطة الألوان ("none", "default", "high_quality")
  │   ├── gamut_mapping (string) - دالة تعيين Gamut ("clip", "perceptual", "soft_clip", "relative", "saturation", "absolute", "desaturate", "darken", "highlight", "linear")
  │   ├── perceptual_deadzone (float) - المنطقة الميتة الإدراكية (افتراضي: 0.30)
  │   ├── perceptual_strength (float) - قوة الإدراك (افتراضي: 0.80)
  │   ├── colorimetric_gamma (float) - جاما قياس الألوان (افتراضي: 1.80)
  │   ├── softclip_knee (float) - ركبة Soft Clip (افتراضي: 0.70)
  │   ├── softclip_desat (float) - إزالة التشبع Soft Clip (افتراضي: 0.35)
  │   ├── lut3d_size_I (int) - حجم LUT 3D I (افتراضي: 48)
  │   ├── lut3d_size_C (int) - حجم LUT 3D C (افتراضي: 32)
  │   ├── lut3d_size_h (int) - حجم LUT 3D h (افتراضي: 256)
  │   ├── lut3d_tricubic (string) - تفعيل Tricubic LUT 3D ("yes", "no")
  │   ├── gamut_expansion (string) - تفعيل توسيع Gamut ("yes", "no")
  │   │
  │   ├── tone_mapping (string) - دالة Tone Mapping ("clip", "spline", "st2094_40", "st2094_10", "bt2390", "bt2446a", "reinhard", "mobius", "hable", "gamma", "linear", "linear_light")
  │   ├── tone_mapping_knee_adaptation (float) - تكيف الركبة
  │   ├── tone_mapping_knee_minimum (float) - الحد الأدنى للركبة
  │   ├── tone_mapping_knee_maximum (float) - الحد الأقصى للركبة
  │   ├── tone_mapping_knee_default (float) - الركبة الافتراضية
  │   ├── tone_mapping_knee_offset (float) - إزاحة الركبة
  │   ├── tone_mapping_slope_tuning (float) - ضبط المنحدر
  │   ├── tone_mapping_slope_offset (float) - إزاحة المنحدر
  │   ├── tone_mapping_spline_contrast (float) - تباين Spline
  │   ├── tone_mapping_reinhard_contrast (float) - تباين Reinhard
  │   ├── tone_mapping_linear_knee (float) - ركبة Linear
  │   ├── tone_mapping_exposure (float) - التعرض
  │   ├── inverse_tone_mapping (string) - تفعيل Inverse Tone Mapping ("yes", "no")
  │   ├── tone_mapping_metadata (string) - بيانات Tone Mapping ("any", "none", "hdr10", "hdr10_plus", "cie_y")
  │   ├── tone_mapping_tone_lut_size (int) - حجم LUT Tone
  │   ├── tone_mapping_contrast_recovery (float) - استعادة التباين
  │   └── tone_mapping_contrast_smoothness (float) - نعومة التباين
```

---

## 🔍 أمثلة على القيم

### الإعدادات الأساسية
```
settings/audio_video_disabled = 0
settings/auto_discovery = 1 (true)
settings/hide_cursor = 1 (true)
settings/hardware_decoder = "d3d11va"
settings/local_render_disabled = 1 (true)
settings/frame_sharing_enabled = 1 (true)
settings/show_stream_stats = 0 (false)
settings/automatic_connect = 1 (true)
```

### إعدادات الاتصال التلقائي (Auto Connect)
```
settings/automatic_connect = 1 (true)  - تفعيل الاتصال التلقائي
settings/auto_connect_mac = [binary]    - MAC address للجهاز (6 bytes)
```

**ملاحظة**: عند تفعيل `automatic_connect`، سيحاول البرنامج الاتصال تلقائياً بالجهاز المحدد في `auto_connect_mac` عند بدء التشغيل.

### الأجهزة المسجلة
```
registered_hosts/0/server_nickname = "PS5-123"
registered_hosts/0/server_mac = "aabbccddeeff"
registered_hosts/0/target = 1000100 (PS5)
registered_hosts/0/rp_regist_key = (binary data)
registered_hosts/0/rp_key = (binary data)
```

### إعدادات الفيديو
```
settings/resolution_local_ps5 = 1080
settings/resolution_remote_ps5 = 1080
settings/fps_local_ps5 = 60
settings/fps_remote_ps5 = 60
settings/bitrate_local_ps5 = 20000
settings/bitrate_remote_ps5 = 15000
settings/codec_local_ps5 = 1 (H265)
settings/codec_remote_ps5 = 1 (H265)
```

---

## 🛠️ كيفية الوصول للريجستري

### 1. استخدام Regedit (واجهة رسومية)

1. اضغط `Win + R`
2. اكتب `regedit` واضغط Enter
3. انتقل إلى:
   ```
   HKEY_CURRENT_USER\Software\Lucifer\LuciferStore
   ```

### 2. استخدام PowerShell

```powershell
# قراءة قيمة
Get-ItemProperty -Path "HKCU:\Software\Lucifer\LuciferStore\settings" -Name "hide_cursor"

# كتابة قيمة
Set-ItemProperty -Path "HKCU:\Software\Lucifer\LuciferStore\settings" -Name "hide_cursor" -Value 1

# قراءة جميع الإعدادات
Get-ItemProperty -Path "HKCU:\Software\Lucifer\LuciferStore\settings"

# تفعيل الاتصال التلقائي
Set-ItemProperty -Path "HKCU:\Software\Lucifer\LuciferStore\settings" -Name "automatic_connect" -Value 1

# تعيين MAC address للاتصال التلقائي (مثال: "aabbccddeeff" كـ hex bytes)
$macBytes = [byte[]](0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff)
Set-ItemProperty -Path "HKCU:\Software\Lucifer\LuciferStore\settings" -Name "auto_connect_mac" -Value $macBytes -Type Binary
```

### 3. استخدام C# (.NET)

```csharp
using Microsoft.Win32;

// قراءة قيمة
string keyPath = @"Software\Lucifer\LuciferStore";
RegistryKey key = Registry.CurrentUser.OpenSubKey(keyPath + @"\settings");
if (key != null) {
    object value = key.GetValue("hide_cursor");
    Console.WriteLine($"hide_cursor = {value}");
    key.Close();
}

// كتابة قيمة
RegistryKey writeKey = Registry.CurrentUser.CreateSubKey(keyPath + @"\settings");
writeKey.SetValue("hide_cursor", 1, RegistryValueKind.DWord);
writeKey.Close();

// تفعيل الاتصال التلقائي
RegistryKey autoConnectKey = Registry.CurrentUser.CreateSubKey(keyPath + @"\settings");
autoConnectKey.SetValue("automatic_connect", 1, RegistryValueKind.DWord);

// تعيين MAC address للاتصال التلقائي (مثال: "aabbccddeeff")
byte[] macBytes = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
autoConnectKey.SetValue("auto_connect_mac", macBytes, RegistryValueKind.Binary);
autoConnectKey.Close();
```

### 4. استخدام C++ (Win32 API)

```cpp
#include <windows.h>

HKEY hKey;
LONG result = RegOpenKeyEx(
    HKEY_CURRENT_USER,
    L"Software\\Lucifer\\LuciferStore\\settings",
    0,
    KEY_READ,
    &hKey
);

if (result == ERROR_SUCCESS) {
    DWORD value;
    DWORD dataSize = sizeof(DWORD);
    result = RegQueryValueEx(
        hKey,
        L"hide_cursor",
        NULL,
        NULL,
        (LPBYTE)&value,
        &dataSize
    );
    
    if (result == ERROR_SUCCESS) {
        printf("hide_cursor = %d\n", value);
    }
    
    RegCloseKey(hKey);
}
```

---

## 📝 ملاحظات مهمة

1. **QSettings في Windows**: يحفظ تلقائياً في الريجستري تحت `HKEY_CURRENT_USER\Software\[Organization]\[Application]`

2. **Profiles**: إذا استخدمت profile، سيتم إنشاء مفتاح منفصل:
   ```
   LuciferStore-{ProfileName}
   ```

3. **البيانات الثنائية**: بعض القيم مثل `rp_regist_key` و `rp_key` تُحفظ كـ binary data

4. **الإصدار**: يوجد مفتاح `version` في الجذر لتتبع إصدار الإعدادات

5. **Placebo Settings**: تُحفظ في مفتاح منفصل:
   ```
   HKEY_CURRENT_USER\Software\Lucifer\pl_render_params
   ```

6. **Auto Connect**: 
   - عند تفعيل `automatic_connect = true`، سيحاول البرنامج الاتصال تلقائياً بالجهاز المحدد في `auto_connect_mac`
   - `auto_connect_mac` يجب أن يكون MAC address صحيح (6 bytes) لجهاز مسجل مسبقاً
   - يعمل مع PSN hosts أيضاً (يستخدم nickname بدلاً من MAC)
   - يتم الاتصال بعد انتظار الاتصال بالإنترنت (PSN_INTERNET_WAIT_SECONDS)

---

## 🔐 الأمان

⚠️ **تحذير**: الإعدادات تحتوي على:
- مفاتيح التسجيل (rp_regist_key, rp_key)
- معلومات الأجهزة
- إعدادات الاتصال

**لا تشارك هذه الإعدادات مع أحد!**

---

## 📚 مراجع

- **المصدر**: `gui/src/settings.cpp`
- **QSettings Documentation**: https://doc.qt.io/qt-6/qsettings.html
- **Windows Registry**: https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry

---

**تم التحديث**: 2026-01-14
