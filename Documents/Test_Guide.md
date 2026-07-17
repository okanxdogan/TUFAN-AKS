# TUFAN-AKS Test Rehberi

Bu doküman, TUFAN-AKS projesindeki test altyapısını, test çalıştırma yöntemlerini ve yeni test ekleme kurallarını açıklar.

---

## İçindekiler

- [Genel Bakış](#genel-bakış)
- [Test Ortamları](#test-ortamları)
- [Test Suite'leri](#test-suiteleri)
- [Testleri Çalıştırma](#testleri-çalıştırma)
  - [IDE Üzerinden](#ide-üzerinden)
  - [Komut Satırından](#komut-satırından)
- [Firmware Build ve ESP32'ye Yükleme](#firmware-build-ve-esp32ye-yükleme)
- [Build Mimarisi](#build-mimarisi)
- [Yeni Test Ekleme Rehberi](#yeni-test-ekleme-rehberi)
- [Sorun Giderme](#sorun-giderme)

---

## Genel Bakış

Proje iki ayrı build ortamı kullanır:

| Ortam | Platform | Amaç |
|-------|----------|------|
| `esp32dev` | ESP32 (Xtensa) | Firmware derlemesi ve karta yükleme |
| `native` | Host (macOS/Linux) | Birim testleri — donanım gerektirmez |

Test framework'ü olarak **Unity** kullanılmaktadır. Native testler, ESP-IDF bağımlılıkları yerine **stub/fake** implementasyonlar ile çalışır.

---

## Test Ortamları

### `esp32dev` — Embedded Test Ortamı

- Gerçek ESP32 donanımı üzerinde çalışır
- Firmware'i karta yükler, ardından seri port üzerinden test sonuçlarını okur
- `test_embedded_smoke` suite'ini içerir
- **Board bağlı olmadan çalıştırılamaz**

### `native` — Host Test Ortamı

- Bilgisayar üzerinde doğrudan çalışır (cross-compile yok)
- ESP-IDF API'leri stub'larla taklit edilir (`test/support/idf_stubs/`)
- Donanım gerektirmez, hızlıdır
- Tüm `test_native_*` suite'lerini içerir

---

## Test Suite'leri

### Native Testler (Donanım Gerektirmez)

| Suite | Dosya Sayısı | Test Sayısı | Açıklama |
|-------|:------------:|:-----------:|----------|
| `test_native_vcu_logic` | 9 | 55 | State machine geçişleri, güvenlik eşikleri, reset interlock, akım/voltaj/sıcaklık kontrolleri |
| `test_native_can_parsing` | 5 | 63 | CAN mesaj ayrıştırma: BMS config, BMS live, motor status, motor timeout |
| `test_native_relay` | 7 | 18 | SPI relay sürücü: init sırası, kanal açma/kapama, `allOn`/`allOff` |
| `test_native_telemetry` | 5 | 13 | UART telemetri paketi: format, sıra numarası, `begin`/`reset` |
| `test_native_hmi_helpers` | — | — | HMI ekran yardımcı fonksiyonları: numerik/text güncelleme, `sendEndBytes` |
| `test_native_bms_algo` | 9 | 18 | `computePack()`: SoC haritalaması, LiFePO4 uyarı/kritik eşik sınırları, dengeleme regresyonu, `cellBarFill()` (dolaylı, `buildBmsNextionCommands` üzerinden) |
| **Toplam** | | **167** | |

### Embedded Testler (Board Gerektirir)

| Suite | Açıklama |
|-------|----------|
| `test_embedded_smoke` | CAN, relay, UART telemetri donanım doğrulama (smoke test) |

---

## Testleri Çalıştırma

### IDE Üzerinden

#### Build (✓ düğmesi)
IDE'deki build düğmesi `platformio run` komutunu çalıştırır. `default_envs = esp32dev` ayarı sayesinde **yalnızca firmware derlenir**, native ortam atlanır.

#### Test (🧪 düğmesi)
PlatformIO'nun varsayılan test düğmesi `default_envs` ayarını kullanır ve board gerektiren `test_embedded_smoke`'u çalıştırmaya çalışır.

**Bunun yerine VS Code task kullanın:**

```
Ctrl+Shift+P → Tasks: Run Test Task → PlatformIO: Test (Native)
```

Bu, `.vscode/tasks.json` dosyasında tanımlı olan `platformio test -e native` komutunu çalıştırır.

### Komut Satırından

```bash
# ─── Tüm native testleri çalıştır ───
pio test -e native

# ─── Belirli bir test suite çalıştır ───
pio test -e native -f test_native_vcu_logic
pio test -e native -f test_native_can_parsing
pio test -e native -f test_native_relay
pio test -e native -f test_native_telemetry
pio test -e native -f test_native_hmi_helpers

# ─── Verbose çıktı ile çalıştır ───
pio test -e native -v

# ─── RELAY_ROLES_ASSIGNED=1 varyantı (S1/S2 + flaşör mantığı) ───
# test_roles_* suite'leri yalnız bu ortamda derlenir/çalışır; varsayılan
# `native` ortamı bayrak=0 (tek-bank) regresyonlarını korur.
pio test -e native_roles

# ─── Embedded smoke test (board bağlıyken) ───
pio test -e esp32dev -f test_embedded_smoke
```

> **Not:** `pio` komutu PATH'te yoksa tam yol kullanın:  
> `~/.platformio/penv/bin/pio test -e native`

---

## Firmware Build ve ESP32'ye Yükleme

### Firmware Derleme

```bash
# IDE build düğmesi veya:
pio run -e esp32dev
```

Başarılı build çıktısı:
```
RAM:   [          ]   4.2% (used 13736 bytes from 327680 bytes)
Flash: [==        ]  22.8% (used 239381 bytes from 1048576 bytes)
========================= [SUCCESS] =========================
```

### ESP32'ye Yükleme

ESP32 kartını USB ile bağlayın, ardından:

```bash
pio run -e esp32dev -t upload
```

### Seri Monitor

Yükleme sonrası cihaz çıktısını izlemek için:

```bash
pio device monitor -e esp32dev
```

Baud rate: `115200` (platformio.ini'de `monitor_speed` ile ayarlanmış).

### Yükleme + Monitor (Tek Komut)

```bash
pio run -e esp32dev -t upload -t monitor
```

---

## Build Mimarisi

### Native Test İzolasyon Stratejisi

Native ortam, `src/` klasöründeki production dosyalarını **otomatik olarak derlemez**:

```ini
[env:native]
build_src_filter = -<*>    # src/ altındaki tüm dosyaları hariç tut
```

Bu gereklidir çünkü `src/main.cpp` ve `src/VcuLogic.cpp` gibi dosyalar ESP-IDF API'lerine bağımlıdır (`esp_task_wdt.h`, `freertos/FreeRTOS.h` vb.) ve host derleyicide bu başlıklar bulunmaz.

### Wrapper Dosyası Yaklaşımı

Bir native test suite production koduna ihtiyaç duyduğunda, o suite'in klasörüne bir **wrapper (build) dosyası** eklenir:

```
test/test_native_vcu_logic/
├── vcu_logic_build.cpp       ← #include "VcuLogic.cpp"
├── fake_freertos.cpp         ← FreeRTOS stub'ları
├── fake_relay_manager.cpp    ← RelayManager stub'ı
├── test_main.cpp             ← Test runner
├── test_safety_thresholds.cpp
├── test_reset_interlock.cpp
└── test_state_machine.cpp

test/test_native_telemetry/
├── telemetry_build.cpp       ← #include "Telemetry.cpp"
├── fake_uart.cpp             ← UART stub'ı
├── test_main.cpp
└── test_telemetry_format.cpp

test/test_native_relay/
├── relay_manager_build.cpp   ← #include "RelayManager.cpp"
├── fake_spi.cpp              ← SPI stub'ı
├── test_main.cpp
├── test_init_sequence.cpp
├── test_set_relay_bits.cpp
└── test_all_on_off.cpp
```

**Kurallar:**
- Her wrapper dosyası yalnızca bir `.cpp` dosyasını `#include` eder
- Stub/fake dosyaları ESP-IDF fonksiyonlarının basitleştirilmiş versiyonlarını sağlar
- Bir suite'in sembolü başka bir suite'e **sızmaz** (izolasyon)

### Ortak Stub'lar

`test/support/idf_stubs/` klasörü, birden fazla suite tarafından paylaşılan ESP-IDF stub başlıklarını içerir. `-I test/support/idf_stubs` build flag'i ile native ortam bu stub'ları ESP-IDF başlıkları yerine kullanır.

### `platformio.ini` Yapısı (Özet)

```
[platformio]
default_envs = esp32dev              ← IDE build düğmesi sadece firmware'i derler

[env:esp32dev]
test_ignore = test_native_*          ← Board testleri native suite'leri atlar

[env:native]
build_src_filter = -<*>              ← src/ dosyaları otomatik derlenmez
test_filter = test_native_*          ← Sadece native testleri çalıştırır
lib_ignore = CanManager, Telemetry,  ← ESP-IDF library'leri native'de dışlanır
             DisplayHMI, RelayManager
-D NATIVE_BUILD                      ← Koşullu derleme macro'su
-D VCU_LOGIC_TESTABLE                ← VcuLogic test erişim macro'su
```

---

## Yeni Test Ekleme Rehberi

### 1. Yeni Native Test Suite Oluşturma

```bash
# Klasör oluştur (isim test_native_ ile başlamalı)
mkdir test/test_native_yeni_modul
```

### 2. Test Runner Dosyası

`test/test_native_yeni_modul/test_main.cpp`:

```cpp
#include <unity.h>

// İleri bildirimler
extern void test_ornek_fonksiyon(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_ornek_fonksiyon);
    return UNITY_END();
}
```

### 3. Test Dosyası

`test/test_native_yeni_modul/test_ornek.cpp`:

```cpp
#include <unity.h>
#include "YeniModul.h"

void test_ornek_fonksiyon(void) {
    // Arrange
    int girdi = 42;
    // Act
    int sonuc = yeniModulFonksiyon(girdi);
    // Assert
    TEST_ASSERT_EQUAL_INT(84, sonuc);
}
```

### 4. Production Kodu Gerekiyorsa → Wrapper Ekle

`test/test_native_yeni_modul/yeni_modul_build.cpp`:

```cpp
// lib/YeniModul/YeniModul.cpp'yi bu suite kapsamında derler
#include "YeniModul.cpp"
```

### 5. ESP-IDF Bağımlılığı Varsa → Fake/Stub Ekle

Suite klasöründe ilgili API'nin stub'ını oluşturun:

```cpp
// test/test_native_yeni_modul/fake_gpio.cpp
#include "driver/gpio.h"
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
    return ESP_OK;  // stub
}
```

### 6. Çalıştır ve Doğrula

```bash
pio test -e native -f test_native_yeni_modul
```

---

## Sorun Giderme

### `esp_task_wdt.h: No such file or directory`

**Sebep:** Native ortam ESP-IDF başlıklarını bulamıyor.  
**Çözüm:** `build_src_filter = -<*>` ayarının `[env:native]` altında olduğundan emin olun. Production `.cpp` dosyaları wrapper ile dahil edilmeli.

### `Nothing to build` (native env)

**Sebep:** `pio run -e native` çalıştırıldığında `build_src_filter = -<*>` nedeniyle derleyecek dosya yok.  
**Çözüm:** Bu beklenen davranıştır. Native ortam sadece test için kullanılır: `pio test -e native`

### `*** [upload] Error 2`

**Sebep:** `pio test -e esp32dev` çalıştırılıyor ama ESP32 board bağlı değil.  
**Çözüm:** Board'u USB ile bağlayın veya `pio test -e native` kullanın.

### Flash Size Mismatch Uyarısı

**Sebep:** `sdkconfig.esp32dev` ve board tanımı arasında flash boyutu tutarsızlığı.  
**Çözüm:** `sdkconfig.esp32dev` dosyasında flash boyutunun board ile uyumlu olduğundan emin olun:

```
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
```

Gerçek flash boyutunu doğrulamak için (board bağlıyken):

```bash
esptool.py flash_id
```

### CMake Cache Bozulması

**Sebep:** Build cache'inde bozuk dosyalar.  
**Çözüm:**

```bash
rm -rf .pio/build/esp32dev
pio run -e esp32dev
```

---

## Hızlı Referans Tablosu

| İşlem | Komut |
|-------|-------|
| Firmware derle | `pio run` |
| Firmware yükle | `pio run -e esp32dev -t upload` |
| Tüm native testler | `pio test -e native` |
| Tek suite çalıştır | `pio test -e native -f <suite_adı>` |
| Seri monitor | `pio device monitor` |
| Embedded test (board) | `pio test -e esp32dev` |
| Build cache temizle | `pio run -e esp32dev -t clean` |

---

*Son güncelleme: 28 Nisan 2026*
