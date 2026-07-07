# Motor Sürücüsü Entegrasyon Notu (G2)

> Kapsam: Bu belge, motor sürücüsü araca **entegre edilmeden önceki** durumu
> dürüstçe belgeler ve entegrasyon günü yapılacakları listeler. Sürücü geldiğinde
> `MOTOR_DRIVER_PRESENT` bayrağı `1` yapılınca iskelet devreye girer.

## 1. G2 riski (mevcut durum, dürüst özet)

Motor sürücüsü **henüz hazır değil ve araca bağlı değil**. Bu yüzden:

- Hiçbir gerçek torque komutu gönderilmiyor (`CanManager::sendTorqueCommand`
  bayrak 0 iken frame üretmez, yalnız bir kez uyarı loglar).
- E-STOP / FAULT durumunda `VcuLogic::handleEmergencyStop` /`handleFault`
  güvenli kapanış sırasını (sıfır-tork → bekle → kontaktör aç) **çağrı olarak
  kurar**, ama sıfır-tork adımı bayrak 0'da gerçek frame üretmediğinden
  kontaktörler **yük altında açılabilir**.
- `VCU_CONTACTOR_OPEN_DELAY_MS = 20 ms` **semboliktir** — gerçek tork sönüm
  süresine göre kalibre edilmemiştir.

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
| `CanManager.cpp::sendTorqueCommand` | frame YOK, bir kez uyarı loglar, `false` döner | **TODO** gerçek frame + `twai_transmit` |
| `lib/CanManager/MotorTorque.h::frameEnabled()` | `false` | `true` |
| `test/test_native_ready_motor/` | — | flag=1 derlemesiyle predicate + gate testleri |

## 3. Entegrasyon günü yapılacaklar (checklist)

1. **`sendTorqueCommand` frame formatı** (`CanManager.cpp`, `#if
   MOTOR_DRIVER_PRESENT` bloğu): motor sürücü spec'inden CAN ID, DLC ve byte
   düzenini **doğrula ve UYDURMADAN** doldur; `twai_transmit` çağrısını aç.
   `CAN_ID_TORQUE_CMD` şu an `SystemConfig.h`'de yorumda — doğrulanınca aç.
2. **Delay kalibrasyonu:** `VCU_CONTACTOR_OPEN_DELAY_MS`'i (SystemConfig.h)
   gerçek tork sönüm süresine göre ayarla. Motor RPM/akımının sıfır-tork
   komutundan sonra ne kadar sürede düştüğünü ölç; delay bu süreden büyük olsun.
3. **E-STOP altında düşüş doğrulaması:** E-STOP tetikle, sıfır-tork komutu
   sonrası **motor RPM ve akımının kontaktör açılmadan ÖNCE düştüğünü**
   osiloskop/CAN log ile doğrula. Bu doğrulama yapılmadan sahaya/piste çıkma.
4. **Torque'un CAN task'i dışından gönderimi (thread-safety):** Sıfır-tork
   isteği VcuLogic (VCU task) → sink → `CanManager::sendTorqueCommand` (CAN
   task'ine ait örnek) yolundan gelir. Bayrak 1'de gerçek `twai_transmit`
   çalışırken bu **task'ler arası erişimin** güvenli olduğundan emin ol
   (ör. torque isteğini CAN task'ine kuyrukla, ya da uygun kilit).
5. **DC-link kapasitesi / precharge kararı:** Motor sürücüsünün DC-link
   kondansatör kapasitesini kontrol et. **Bataryada precharge devresi YOK**;
   risk yük tarafındaki (motor sürücü) kondansatörden gelir ve motor sürücü
   entegre edilmeden **sorun oluşmaz**. Anlamlı bir DC-link kapasitesi varsa,
   **precharge (donanım devresi) + sıralı kapatma (yazılım)** kararı donanım
   ekibiyle **birlikte** verilecek. (Bu projede precharge rolü tanımlı değildir;
   bkz. SystemConfig.h röle kanal tablosu.)

## 4. İlgili dosyalar

- `include/SystemConfig.h` — `MOTOR_DRIVER_PRESENT`, `VCU_CONTACTOR_OPEN_DELAY_MS`
- `lib/CanManager/CanManager.cpp` / `.h` — `sendTorqueCommand`
- `lib/CanManager/MotorTorque.h` — saf frame-gate (`frameEnabled()`)
- `src/VcuLogic.cpp` — `handleEmergencyStop` / `handleFault` kapanış sırası,
  `requestZeroTorque`, torque sink hook
- `src/main.cpp` — `CAN_torqueSink` köprüsü, `VcuLogic::setTorqueSink`
- `test/test_native_vcu_logic/` — E-STOP/FAULT çağrı sırası testleri
