# AKS Board Bring-Up Checklist

Bu doküman, ESP-AKS kodunun harici breadboard/ESP32'den, araçtaki **gerçek AKS anakartına** (dahili ESP32, dahili TWAI, TJA1050) taşınması sırasında donanım ve yazılımın doğrulanması için hazırlanmıştır.

## 1. Donanım Pin Bağlantıları (SystemConfig.h Referansı)

| Birim | ESP32 Pini (GPIO) | Kart/Şema Etiketi | Açıklama |
| :--- | :--- | :--- | :--- |
| **CAN_TX (TWAI)** | `GPIO_NUM_5` | TX | ESP32'nin dâhili CAN kontrolcüsü TX çıkışı, TJA1050'ye gider. |
| **CAN_RX (TWAI)** | `GPIO_NUM_4` | RX | ESP32'nin dâhili CAN kontrolcüsü RX girişi, TJA1050'den gelir. |
| **HMI_TX (Nextion)** | `GPIO_NUM_33` | screen_RX | ESP32 TX → Nextion Ekran RX |
| **HMI_RX (Nextion)** | `GPIO_NUM_32` | screen_TX | Nextion Ekran TX → ESP32 RX |
| **LORA_TX** | `GPIO_NUM_16` | LR_RXD | ESP32 TX → LoRa RX (UART2) |
| **LORA_RX** | `GPIO_NUM_17` | LR_TXD | LoRa TX → ESP32 RX (UART2) |

> ⚠️ **UYARI (Nextion):** Şemada `screen_RX` / `screen_TX` etiketleri kafa karıştırıcı olabilir (ESP açısından mı, konnektör açısından mı?). Eğer ekrana veri gitmiyorsa, kodda GPIO32 ve GPIO33'ü yer değiştirip deneyin.

## 2. Yazılım Yapılandırması ve Auto-Baudrate

- Koddan eski `MCP2515` veya harici SPI-CAN kütüphanesi tamamen kaldırıldı; sistem ESP-IDF'in resmi **TWAI** (Two-Wire Automotive Interface) donanım sürücüsü üzerinde çalışıyor.
- `CanManager::begin()` içinde **Auto-Baudrate Detection** döngüsü mevcuttur. Kod sırasıyla `500kbps`, `125kbps` ve `250kbps` hızlarını dener. CAN bus üzerindeki cihazlardan sinyal yakaladığı (1 saniye içinde geçerli paket okuduğu) hızı kilitler.

## 3. Beklenen İlk Log Çıktısı (Boot Sequence)

Seri portu (UART0 - 115200 baud) bilgisayara bağlayıp kartı başlattığınızda aşağıdakilere benzer loglar akmalıdır:

```text
I (xx) DisplayHMI: Initialized on UART1 (TX=IO33, RX=IO32)
I (xx) CanManager: CAN bitrate auto-detect deneniyor: 500kbps
I (xx) CanManager: CAN bitrate BASARIYLA bulundu: 500kbps  <-- VEYA 125kbps
...
D (xx) CanManager: LB-E000: packV=790 deciV (79.0 V)
D (xx) CanManager: LB-E001: temp1=25 C, temp2=24 C
```

## 4. Sorun Giderme (Troubleshooting)

Eğer `CAN bitrate auto-detect basarisiz oldu! Fallback: 500kbps` görüyorsanız ve sonrasında hiç `LB-E000` logu gelmiyorsa:

1. **Fiziksel Hata:** TJA1050'nin beslemesi (5V) var mı? TJA1050, 3.3V ile çalışmaz (RX/TX 3.3V tolere eder ama VCC 5V olmalıdır).
2. **Kablo & Terminasyon:** CAN_H ve CAN_L kabloları BMS'e doğru mu bağlı? Hattın iki ucunda 120 ohm terminasyon direnci (toplam eşdeğer ~60 ohm) var mı? Multimetreyle CAN kapalıyken direnç ölçün.
3. **Pin Tersliği:** `CAN_TX (5)` ve `CAN_RX (4)` fiziksel olarak transceiver'ın yanlış pinlerine gidiyor olabilir mi?
4. **BMS Uykuda:** BMS güç tuşu/anahtarı açık mı? Şarj/Deşarj hattında bir işlem yapılmadığı için BMS uyku modunda olabilir.
5. **Nextion Ekranında Değerler Yok:** CAN'den log geliyor ama ekranda `--` veya `0` varsa; J8 soketindeki TX/RX (32/33) kablolarını çaprazlayın (Rx-Tx yer değiştirin).
