#pragma once

// Araç geometrisi / donanım parametreleri — TEK doğruluk kaynağı.
//
// 9.2.c.i / 9.4.b.iii: araç hızı (km/h) zorunlu telemetri verisidir ve
// teknik kontrolde ANLIK doğruluğu denetlenir. 9.2.f: izleme merkezi
// kaydında kalan_enerji_Wh kolonu zorunludur. Aşağıdaki placeholder'lar
// gerçek değerler gelene kadar hız VE enerji hesabını YANLIŞ üretir —
// bu dosyanın amacı bu durumu build çıktısında ve boot logunda GÖRÜNÜR
// kılmak, sessizce yarışa taşınmasını engellemektir.
//
// DEĞERLER GELDİĞİNDE: aşağıdaki 3 sabiti güncelle + VEHICLE_PARAMS_CONFIRMED=1
// yap + `pio test -e native` koştur — BAŞKA HİÇBİR DEĞİŞİKLİK GEREKMEZ
// (rpmToSpeedKmhX10 ve enerji zinciri bu dosyayı okur).

// Tekerlek çapı (metre). Kaynak: mekanik ekip — yüklü araçta (sürücü +
// ekipman ile) yuvarlanma çevresi bantla ölçülüp D = çevre / pi ile
// hesaplanır (basınçla şişmiş lastik yarıçapı geometrik ölçümden farklı
// olabileceğinden yuvarlanma çevresi tercih edilir).
// PLACEHOLDER — henüz ölçülmedi.
#define WHEEL_DIAMETER_M 0.5f

// Dişli oranı (motor mili devri / teker devri, birimsiz). Kaynak:
// güç aktarma ekibi — zincir/dişli diş sayıları oranından (Z_teker /
// Z_motor) ya da datasheet'ten alınır.
// PLACEHOLDER — henüz teyit edilmedi.
#define GEAR_RATIO 1.0f

// Motor sürücüsünün CAN 0x200 (CAN_ID_MOTOR_STATUS) frame'inde
// raporladığı RPM alanı motor MİLİ hızı mı yoksa zaten TEKER hızı mı?
// 0 = motor mili (GEAR_RATIO'ya bölünerek teker hızına çevrilir — mevcut
//     varsayım, motor sürücüsü CAN dokümanı henüz teyit edilmedi).
// 1 = zaten teker hızı (GEAR_RATIO bölmesi ATLANIR).
// Kaynak: motor sürücüsü CAN dokümanı ve/veya standlı test (bilinen bir
// teker hızında RPM okunup GEAR_RATIO ile tutarlılığı kontrol edilir).
// PLACEHOLDER — henüz teyit edilmedi.
#define MOTOR_RPM_IS_WHEEL_RPM 0

// Yukarıdaki 3 parametre gerçek ölçüm/dokümanla doğrulanıp bu dosyada
// güncellendiğinde 1 yapılır. 0 iken derleme #warning verir ve boot'ta
// bir kez ESP_LOGE basılır (bkz. src/main.cpp) — derleme KIRILMAZ, saha/
// bench testleri placeholder'larla mümkün kalır.
#define VEHICLE_PARAMS_CONFIRMED 0

#if !VEHICLE_PARAMS_CONFIRMED
#warning "ARAC PARAMETRELERI TEYITSIZ — hiz/enerji verisi gecersiz (VehicleParams.h)"
#endif
