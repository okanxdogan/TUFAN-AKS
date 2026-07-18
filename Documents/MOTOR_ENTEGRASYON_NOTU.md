# Motor Sürücüsü Entegrasyon Notu (G2)

> Kapsam: Bu belge, motor sürücüsü araca **entegre edilmeden önceki** durumu
> dürüstçe belgeler ve entegrasyon günü yapılacakları listeler. Sürücü geldiğinde
> `MOTOR_DRIVER_PRESENT` bayrağı `1` yapılınca iskelet devreye girer.

## 1. G2 riski (mevcut durum, dürüst özet)

Motor sürücüsü **henüz hazır değil ve araca bağlı değil**. Bu yüzden:

- Hiçbir gerçek torque komutu gönderilmiyor (`CanManager::sendTorqueCommand`
  bayrak 0 iken frame üretmez, yalnız bir kez uyarı loglar; bayrak 1 olsa
  bile frame içeriği henüz DOĞRULANMADIĞI için gönderim yine gerçekleşmez —
  aşağıya bkz.).
- E-STOP / FAULT durumunda `VcuLogic::handleEmergencyStop` /`handleFault`
  güvenli kapanış sırasını (sıfır-tork → bekle → kontaktör aç) **çağrı olarak
  kurar**, ama sıfır-tork adımı bayrak 0'da gerçek frame üretmediğinden
  kontaktörler **yük altında açılabilir**.
- `VCU_CONTACTOR_OPEN_DELAY_MS = 20 ms` **semboliktir** — gerçek tork sönüm
  süresine göre kalibre edilmemiştir.
- **2026-07-13 GÜNCELLEME (thread-safety hazırlığı, madde 4 çözüldü):**
  `CanManager::sendTorqueCommand` artık VCU task'inden çağrıldığında
  `twai_transmit`'i ASLA doğrudan çağırmaz — istek bir
  `TorqueRequestQueue`'ya (`lib/CanManager/TorqueRequestQueue.h`, saf/atomic,
  FreeRTOS/twai bağımsız) yazılır; gerçek gönderim CAN task döngüsünde
  (`processRxMessages()` içinden, her tik) `drainTorqueQueue()` ile çekilip
  yapılacak. Bu, tasarımı ŞİMDİDEN doğru task'e bağlıyor; frame İÇERİĞİ hâlâ
  TODO (aşağıya bkz.) — motor sürücü spec'i gelmeden `drainTorqueQueue()`
  içindeki gövde derlenmez bile (bkz. madde 3'teki `#error` guard'ı).

**Saha riski:** Motor tork üretirken kontaktör açılırsa **ark, kontak kaynaması
ve regen aşırı gerilimi** oluşabilir. Bu risk yalnızca motor sürücüsü entegre
edildiğinde gerçek olur (şu an sürücü yokken tehlike oluşmaz).

## 2. `MOTOR_DRIVER_PRESENT` bayrağının kapsadığı yerler

Bayrak `include/SystemConfig.h` içinde tanımlı (varsayılan `0`). Bayrak `1`
yapıldığında etkilenen tüm noktalar:

| Yer | Bayrak 0 (mevcut) | Bayrak 1 (entegrasyon) |
|-----|-------------------|------------------------|
| `VcuLogic.h::isReadyEntryPermitted` (P1 READY interlock) | motor verisi READY girişini bloklamaz | ek şart: `TEL_motorDataValid == true` |
| `VcuLogic.cpp::readyRejectReason` | `motorDataValid` reddedilme nedeni değil | `motorDataValid=0` READY reddi nedeni olur |
| `CanManager.cpp::sendTorqueCommand` | frame YOK, bir kez uyarı loglar, `false` döner, kuyruğa yazmaz | İsteği `TorqueRequestQueue`'ya yazar (`true` döner) — gerçek `twai_transmit` CAN task'inde `drainTorqueQueue()` ile yapılır; frame içeriği **TODO** (`#error` guard `MOTOR_TORQUE_FRAME_DEFINED` tanımlanana kadar derlemeyi engeller) |
| `lib/CanManager/MotorTorque.h::frameEnabled()` | `false` | `true` |
| `test/test_native_ready_motor/` | — | flag=1 derlemesiyle predicate + gate testleri |

## 3. Entegrasyon günü yapılacaklar (checklist)

1. **`sendTorqueCommand`/`drainTorqueQueue` frame formatı** (`CanManager.cpp`,
   `drainTorqueQueue()` içindeki `#if MOTOR_DRIVER_PRESENT` bloğu): motor
   sürücü spec'inden CAN ID, DLC ve byte düzenini **doğrula ve UYDURMADAN**
   doldur; `twai_transmit` çağrısını aç. `CAN_ID_TORQUE_CMD` şu an
   `SystemConfig.h`'de yorumda — doğrulanınca aç. Format tamamlanınca
   `SystemConfig.h`'ye `#define MOTOR_TORQUE_FRAME_DEFINED 1` ekle (aksi
   halde `MOTOR_DRIVER_PRESENT=1` yapıldığında derleme `#error` ile
   BİLEREK kırılır — bkz. madde 3 altındaki not, thread-safety kalemi ÇÖZÜLDÜ).
2. **Delay kalibrasyonu:** `VCU_CONTACTOR_OPEN_DELAY_MS`'i (SystemConfig.h)
   gerçek tork sönüm süresine göre ayarla. Motor RPM/akımının sıfır-tork
   komutundan sonra ne kadar sürede düştüğünü ölç; delay bu süreden büyük
   olsun. **EK (kuyruklama sonrası):** ölçülecek süre artık yalnızca
   "sıfır-tork komutundan motor tepkisine" değil, "VCU'nun `requestZeroTorque`
   çağrısından (t=0) motorun GERÇEKTEN sıfır tork uygulamasına" kadar geçen
   süre olmalı — bu, CAN task'inin `drainTorqueQueue()`'yu bir sonraki tik'te
   çekme gecikmesini de (CAN task döngü periyodu kadar, en kötü durum) İÇERİR.
   Delay'i yalnızca motor tork sönüm süresine göre değil, bu ek gecikmeyi de
   ekleyerek kalibre et.
3. **E-STOP altında düşüş doğrulaması:** E-STOP tetikle, sıfır-tork komutu
   sonrası **motor RPM ve akımının kontaktör açılmadan ÖNCE düştüğünü**
   osiloskop/CAN log ile doğrula. Bu doğrulama yapılmadan sahaya/piste çıkma.
4. ~~**Torque'un CAN task'i dışından gönderimi (thread-safety):**~~ **ÇÖZÜLDÜ
   (2026-07-13).** Sıfır-tork isteği VcuLogic (VCU task) → sink →
   `CanManager::sendTorqueCommand` (CAN task'ine ait örnek) yolundan gelir;
   artık `sendTorqueCommand` isteği yalnızca bir `TorqueRequestQueue`'ya
   (atomic, kilitsiz) yazar — gerçek `twai_transmit` HER ZAMAN CAN task
   döngüsünden (`processRxMessages()` → `drainTorqueQueue()`) çağrılır.
   Native test: `test/test_native_vcu_logic/test_state_machine.cpp`
   `test_estop_zero_torque_reaches_can_queue_before_contactor_open` — E-STOP
   sırasında isteğin kuyruğa kontaktör açılmadan ÖNCE ulaştığını VE
   kuyruklamanın bu sırayı bozmadığını doğrular. Kalan iş: yalnızca madde 1
   (frame İÇERİĞİ) ve madde 2'deki ek gecikme kalibrasyonu.
5. **DC-link kapasitesi / precharge kararı:** Motor sürücüsünün DC-link
   kondansatör kapasitesini kontrol et. **Bataryada precharge devresi YOK**;
   risk yük tarafındaki (motor sürücü) kondansatörden gelir ve motor sürücü
   entegre edilmeden **sorun oluşmaz**. Anlamlı bir DC-link kapasitesi varsa,
   **precharge (donanım devresi) + sıralı kapatma (yazılım)** kararı donanım
   ekibiyle **birlikte** verilecek. (Bu projede precharge rolü tanımlı değildir;
   bkz. SystemConfig.h röle kanal tablosu.)

## 4. İlgili dosyalar

- `include/SystemConfig.h` — `MOTOR_DRIVER_PRESENT`, `VCU_CONTACTOR_OPEN_DELAY_MS`,
  (entegrasyon günü eklenecek) `MOTOR_TORQUE_FRAME_DEFINED`
- `lib/CanManager/CanManager.cpp` / `.h` — `sendTorqueCommand`,
  `drainTorqueQueue` (CAN task döngüsü, `processRxMessages()` içinden
  çağrılır), dosya başındaki `#error` guard'ı
- `lib/CanManager/TorqueRequestQueue.h` — SAF (atomic, FreeRTOS/twai
  bağımsız) VCU task → CAN task tork isteği kuyruğu; native testlerde
  bağımsız test edilir
- `lib/CanManager/MotorTorque.h` — saf frame-gate (`frameEnabled()`)
- `src/VcuLogic.cpp` — `handleEmergencyStop` / `handleFault` kapanış sırası,
  `requestZeroTorque`, torque sink hook
- `src/main.cpp` — `CAN_torqueSink` köprüsü, `VcuLogic::setTorqueSink`
- `test/test_native_vcu_logic/` — E-STOP/FAULT çağrı sırası testleri +
  `test_torque_request_queue.cpp` (kuyruk unit testleri) +
  `test_estop_zero_torque_reaches_can_queue_before_contactor_open`
  (kuyruklamanın E-STOP sırasını bozmadığının entegrasyon testi)

---

## 5. Hız Sensörü CAN Entegrasyonu (2026-07-17)

> Kapsam: Hall-effect hız sensörü ünitesi (esp32-canbus-speed-sensor) artık
> AKS kartına **doğrudan CAN hattı üzerinden** bağlıdır. Geçici test düzeneğinde
> kullanılan 2. bilgisayar (motor-surucu-test reposu, ESP32 + MCP2515 alıcı)
> **devre dışı bırakılmıştır** — artık gerekli değildir.

**Mevcut topoloji:**
```
[Hall sensör ünitesi (ESP32+MCP2515+2 mıknatıs)]
     --- CAN_H / CAN_L (500 kbps, STD 11-bit, ID 0x200) ---
[AKS kartı (ESP32 + TJA1050 TWAI)]
     --- UART1 (9600 baud) ---
[Nextion HMI Ekranı]
```

**Sensör tarafı:** `CAN_MSG_ID = 0x200` (`CAN_ID_MOTOR_STATUS` ile eşleştirildi).
Frame formatı: `data[0:1]` = RPM (big-endian uint16), `data[2:7]` = 0x00, DLC=8,
100 ms periyot (10 Hz). `data[7]` bit0 (isRunning) = 0 bırakıldı — motor sürücüsü
entegre değil (`MOTOR_DRIVER_PRESENT=0`), bu bit VCU karar mantığını ETKİLEMEZ.

**AKS tarafı:** `CanManager::processRxMessages()` → `handleMotorStatus()` →
`CanParse::parseMotorStatus()` → `TEL_motorRpm` → `rpmToSpeedKmhX10()`
(`VehicleParams.h`: D=0.56 m, GR=1.0, direkt tahrik) → `TEL_speedKmhX10` →
`vTask_HMI_Display` (EMA filtre) → Nextion `speed.val=...`. Zincir doğrulandı,
native test eklendi (`test_hall_sensor_rpm850_parse_and_speed` vb.).

