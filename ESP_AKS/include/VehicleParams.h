#pragma once

// Tekerlek çapı (metre). Kaynak: mekanik ekip — yüklü araçta (sürücü +
// ekipman ile) yuvarlanma çevresi bantla ölçülüp D = çevre / pi ile
// hesaplanır (basınçla şişmiş lastik yarıçapı geometrik ölçümden farklı
// olabileceğinden yuvarlanma çevresi tercih edilir).
// TEYİTLİ — mekanik ekip, 2026-07: dış çap 56 cm.
#define WHEEL_DIAMETER_M 0.56f

// Dişli oranı (motor mili devri / teker devri, birimsiz). Kaynak:
// güç aktarma ekibi — zincir/dişli diş sayıları oranından (Z_teker /
// Z_motor) ya da datasheet'ten alınır.
// TEYİTLİ — mekanik ekip, 2026-07: direkt tahrik, oran 1.
#define GEAR_RATIO 1.0f

// Motor sürücüsünün CAN 0x200 (CAN_ID_MOTOR_STATUS) frame'inde
// raporladığı RPM alanı motor MİLİ hızı mı yoksa zaten TEKER hızı mı?
// 0 = motor mili (GEAR_RATIO'ya bölünerek teker hızına çevrilir).
// 1 = zaten teker hızı (GEAR_RATIO bölmesi ATLANIR).
// TEYİTLİ — GR=1 direkt tahrik: motor rpm = teker rpm, 2026-07.
#define MOTOR_RPM_IS_WHEEL_RPM 1

// Yukarıdaki 3 parametre gerçek ölçüm/dokümanla doğrulanıp bu dosyada
// güncellendiğinde 1 yapılır. 0 iken derleme #warning verir ve boot'ta
// bir kez ESP_LOGE basılır (bkz. src/main.cpp) — derleme KIRILMAZ.
#define VEHICLE_PARAMS_CONFIRMED 1

#if !VEHICLE_PARAMS_CONFIRMED
#warning "ARAC PARAMETRELERI TEYITSIZ — hiz/enerji verisi gecersiz (VehicleParams.h)"
#endif
