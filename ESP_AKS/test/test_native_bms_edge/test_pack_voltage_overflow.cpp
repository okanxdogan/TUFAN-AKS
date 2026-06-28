#include <unity.h>

#include <climits>

#include "bms_edge_helpers.h"

// ===========================================================================
// KRİTİK EDGE CASE: PAKET TOPLAM GERİLİMİ TAŞMASI
// ===========================================================================
// 24 hücre * 4200 mV = 100800 mV. uint16 max = 65535. Toplam uint16'ya SIĞMAZ.
// BmsComputed.packVoltageMv ŞU AN uint16 (bkz. lib/BmsAlgo/BmsComputed.h).
// Bu testler hem doğru (int32) toplamı hem de uint16 ile yaşanacak TAŞMAYI
// kanıtlar. Rapor: Rol 2 tipi uint32 veya deciV'e çevirmeli.
//
// Sadece BmsModel.h (BmsPackData) + test-local doğrulayıcılara bağlıdır.
// ===========================================================================

using namespace bmsedge;

void test_pack_sum_exceeds_uint16_range(void) {
    // Tüm hücreler tavanda: gerçek toplam uint16 aralığını AŞAR.
    BmsPackData p = makeUniform(4200, 25, 0, true);
    int32_t trueSum = sumPackVoltageMv(p);
    TEST_ASSERT_EQUAL_INT32(100800, trueSum);
    TEST_ASSERT_TRUE(trueSum > 65535);  // uint16'ya sığmaz — KRİTİK
}

void test_pack_sum_uint16_wraps_around(void) {
    // Aynı paket uint16 ile toplanırsa SARMA (wrap) olur: 100800 - 65536 = 35264.
    // Bu, ekranda gerçek ~100 V yerine ~35 V gösterilmesi demektir — TEHLİKELİ.
    BmsPackData p = makeUniform(4200, 25, 0, true);
    uint16_t wrapped = sumPackVoltageMvAsUint16(p);
    TEST_ASSERT_EQUAL_UINT16(35264, wrapped);  // 100800 mod 65536
    // Sarılmış değer, doğru değerden KÜÇÜK — sessiz veri bozulması kanıtı.
    TEST_ASSERT_TRUE(wrapped < sumPackVoltageMv(p));
}

void test_pack_sum_overflow_threshold_boundary(void) {
    // uint16 sınırı (65535) ne zaman aşılır? 65535 / 24 ≈ 2730.6 mV ortalama.
    // Hücreler 2731 mV iken toplam = 65544 > 65535 (taşma başlar).
    BmsPackData p = makeUniform(2731, 25, 0, true);
    TEST_ASSERT_EQUAL_INT32(65544, sumPackVoltageMv(p));
    TEST_ASSERT_TRUE(sumPackVoltageMv(p) > 65535);
    // 2730 mV'de ise henüz sığar: 24*2730 = 65520 < 65535.
    BmsPackData p2 = makeUniform(2730, 25, 0, true);
    TEST_ASSERT_EQUAL_INT32(65520, sumPackVoltageMv(p2));
    TEST_ASSERT_TRUE(sumPackVoltageMv(p2) <= 65535);
}

void test_pack_sum_nominal_fits_but_near_limit(void) {
    // Nominal 3650 mV: 24*3650 = 87600 — gerçek pakette bile uint16 TAŞAR.
    // Yani taşma yalnızca aşırı durumda değil, NORMAL çalışmada da gerçekleşir.
    BmsPackData p = makeNominal();
    TEST_ASSERT_EQUAL_INT32(87600, sumPackVoltageMv(p));
    TEST_ASSERT_TRUE(sumPackVoltageMv(p) > 65535);  // normal şarjda bile taşar!
}

void test_pack_sum_deciv_representation_fits_uint16(void) {
    // ÖNERİLEN ÇÖZÜM: deciV (0.1 V) ile temsil. 100800 mV = 1008 deciV.
    // 1008 << 65535 — uint16 fazlasıyla yeter. (Telemetry zaten deciV kullanır.)
    BmsPackData p = makeUniform(4200, 25, 0, true);
    int32_t deciV = sumPackVoltageMv(p) / 100;  // mV -> deciV
    TEST_ASSERT_EQUAL_INT32(1008, deciV);
    TEST_ASSERT_TRUE(deciV <= 65535);
}
