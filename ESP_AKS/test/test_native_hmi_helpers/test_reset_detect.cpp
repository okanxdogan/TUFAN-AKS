#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "NextionResetDetect.h"

// Nextion Startup event dizisi: 00 00 00 FF FF FF (brown-out/reset işareti).
namespace {
constexpr uint8_t STARTUP[6] = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF};

// Diziyi sırayla besler; yalnızca SON byte'ta true beklenir, öncesinde false.
// Dönüş: dizinin herhangi bir yerinde true görüldü mü (son byte dahil).
bool feedAll(HMI_NextionResetDetect& det, const uint8_t* bytes, size_t len) {
    bool detected = false;
    for (size_t i = 0; i < len; ++i) {
        if (det.HMI_feedByte(bytes[i])) detected = true;
    }
    return detected;
}
}  // namespace

// ---------------------------------------------------------------------------
// Doğru dizi: tam Startup event'i tek seferde → son byte'ta tespit.
// ---------------------------------------------------------------------------
void test_reset_detect_full_sequence(void) {
    HMI_NextionResetDetect det;
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_FALSE(det.HMI_feedByte(STARTUP[i]));
    }
    TEST_ASSERT_TRUE(det.HMI_feedByte(STARTUP[5]));
}

// Tespitten sonra dedektör kendini sıfırlar — ikinci reset de yakalanır.
void test_reset_detect_fires_again_after_detection(void) {
    HMI_NextionResetDetect det;
    TEST_ASSERT_TRUE(feedAll(det, STARTUP, 6));
    TEST_ASSERT_TRUE(feedAll(det, STARTUP, 6));
}

// ---------------------------------------------------------------------------
// Bozuk diziler: eksik 0x00, eksik 0xFF, araya yabancı byte → tespit YOK.
// ---------------------------------------------------------------------------
void test_reset_detect_broken_sequences_do_not_fire(void) {
    {
        // Yalnız iki 0x00 — ön-ek eksik.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0x00, 0xFF, 0xFF, 0xFF};
        TEST_ASSERT_FALSE(feedAll(det, seq, sizeof(seq)));
    }
    {
        // Yalnız iki 0xFF — sonek eksik, ardından yabancı byte.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0x88};
        TEST_ASSERT_FALSE(feedAll(det, seq, sizeof(seq)));
    }
    {
        // Ortasına yabancı byte girmiş dizi.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0x00, 0x00, 0x5A, 0xFF, 0xFF, 0xFF};
        TEST_ASSERT_FALSE(feedAll(det, seq, sizeof(seq)));
    }
    {
        // bkcmd'li modlarda görülebilen "invalid instruction" yanıtı
        // (00 FF FF FF) tek başına ve art arda — ASLA startup sanılmamalı.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0xFF, 0xFF, 0xFF,
                               0x00, 0xFF, 0xFF, 0xFF};
        TEST_ASSERT_FALSE(feedAll(det, seq, sizeof(seq)));
    }
}

// ---------------------------------------------------------------------------
// Parçalı geliş: dizi birden çok uart_read_bytes çağrısına bölünmüş gibi
// ayrı feedAll çağrılarıyla beslenir — durum çağrılar arasında korunmalı.
// ---------------------------------------------------------------------------
void test_reset_detect_fragmented_arrival(void) {
    HMI_NextionResetDetect det;
    const uint8_t part1[] = {0x00, 0x00};
    const uint8_t part2[] = {0x00, 0xFF};
    const uint8_t part3[] = {0xFF, 0xFF};
    TEST_ASSERT_FALSE(feedAll(det, part1, sizeof(part1)));
    TEST_ASSERT_FALSE(feedAll(det, part2, sizeof(part2)));
    TEST_ASSERT_TRUE(feedAll(det, part3, sizeof(part3)));
}

// Kısmi eşleşme geri dönüşleri: fazladan 0x00'lar diziye zarar vermez,
// kırılan eşleşme sonrası yeni 0x00 ön-eki baştan sayılır.
void test_reset_detect_prefix_backtracking(void) {
    {
        // Fazladan öncü sıfırlar: 00 00 00 00 FF FF FF → tespit edilmeli.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
        TEST_ASSERT_TRUE(feedAll(det, seq, sizeof(seq)));
    }
    {
        // Yarıda kesilen sonek + tam yeni dizi: 00 00 00 FF | 00 00 00 FF FF FF.
        HMI_NextionResetDetect det;
        const uint8_t seq[] = {0x00, 0x00, 0x00, 0xFF,
                               0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
        TEST_ASSERT_TRUE(feedAll(det, seq, sizeof(seq)));
    }
}

// ---------------------------------------------------------------------------
// 0x5A komut çerçevesiyle karışma: dokunmatik çerçeve byte'ları (0x5A CMD ~CMD)
// dedektörü yanlış tetiklemez; öncesinde/sonrasında gelen Startup event'i
// yine yakalanır (dedektör çerçeve parser'ına paralel, bağımsız gözlemci).
// ---------------------------------------------------------------------------
void test_reset_detect_mixed_with_touch_frame(void) {
    HMI_NextionResetDetect det;

    // START komut çerçevesi: 0x5A 0x01 0xFE — tespit yok.
    const uint8_t frame[] = {0x5A, 0x01, 0xFE};
    TEST_ASSERT_FALSE(feedAll(det, frame, sizeof(frame)));

    // Hemen ardından gelen Startup event'i yakalanmalı.
    TEST_ASSERT_TRUE(feedAll(det, STARTUP, 6));

    // Startup dizisi yarıda kesilip araya çerçeve girerse tespit yok...
    const uint8_t interrupted[] = {0x00, 0x00, 0x00, 0x5A, 0x01, 0xFE};
    TEST_ASSERT_FALSE(feedAll(det, interrupted, sizeof(interrupted)));
    // ...ama sonradan gelen TAM dizi yine yakalanır.
    TEST_ASSERT_TRUE(feedAll(det, STARTUP, 6));
}

// HMI_reset() aramayı baştan başlatır.
void test_reset_detect_manual_reset_clears_progress(void) {
    HMI_NextionResetDetect det;
    TEST_ASSERT_FALSE(feedAll(det, STARTUP, 5));  // 5/6 eşleşti
    det.HMI_reset();
    // Kalan son 0xFF tek başına tespit üretmemeli.
    TEST_ASSERT_FALSE(det.HMI_feedByte(0xFF));
    // Tam dizi hâlâ çalışır.
    TEST_ASSERT_TRUE(feedAll(det, STARTUP, 6));
}
