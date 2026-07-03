#include <unity.h>

#include <cstring>

#include "OfflineBuffer.h"
#include "Telemetry.h"
#include "TelemetrySanitize.h"
#include "fake_uart.h"

// ---------------------------------------------------------------------------
// S4 (sanitize sırası): OfflineBuffer'a doğrudan (CanManager'ı bypass ederek)
// kasıtlı bozuk sysState (7, aralık 1..4 dışı) ile bir paket konur. Replay
// yolu — peek -> sanitizeForUplink -> sendStatus — canlı yolla AYNI ortak
// kapıdan geçer (bkz. src/main.cpp vTask_LoRa_UKS). Çıktıda sysState alanı
// FAULT(4) olarak görünmelidir.
// ---------------------------------------------------------------------------
void test_replay_output_sanitizes_corrupted_system_state(void) {
    fake_uart_reset();
    ob_reset();

    TelemetryData corrupted = {};
    corrupted.TEL_bmsSystemState = 7;  // UKS'in 1..4 aralığı dışında
    ob_push(corrupted);

    Telemetry tel;
    tel.begin();

    TelemetryData replay = {};
    TEST_ASSERT_TRUE(ob_peek(replay));
    tel.sendStatus(TelemetrySanitize::sanitizeForUplink(replay));
    TEST_ASSERT_TRUE(ob_drop_front());

    const char* expected =
        "TEL,2,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0\r\n";
    TEST_ASSERT_EQUAL_STRING(expected, fake_uart_get_buffer());
    TEST_ASSERT_TRUE(ob_is_empty());
}

// ---------------------------------------------------------------------------
// S4: sysState=0 (diğer aralık-dışı uç) da aynı şekilde FAULT(4)'e döner.
// ---------------------------------------------------------------------------
void test_replay_output_sanitizes_zero_system_state(void) {
    fake_uart_reset();
    ob_reset();

    TelemetryData corrupted = {};
    corrupted.TEL_bmsSystemState = 0;
    ob_push(corrupted);

    Telemetry tel;
    tel.begin();

    TelemetryData replay = {};
    TEST_ASSERT_TRUE(ob_peek(replay));
    tel.sendStatus(TelemetrySanitize::sanitizeForUplink(replay));
    TEST_ASSERT_TRUE(ob_drop_front());

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",4,0,0,0,0,0,0\r\n"));
}

// ---------------------------------------------------------------------------
// Madde 4 (seq semantiği): seq yalnızca gerçek TX anında (sendStatus
// içinde) artar. 3 buffered paket replay edilip ardından 1 canlı paket
// gönderildiğinde seq 0,1,2,3 olarak ARDIŞIK ve MONOTON artmalı —
// TUFAN-Monitor'ün new-boot tespiti (seq artar, ts geriye gider = replay)
// bu davranışa dayanır. peek/drop_front akışı bunu bozmamalı.
// ---------------------------------------------------------------------------
void test_replay_then_live_seq_is_sequential_and_monotonic(void) {
    fake_uart_reset();
    ob_reset();

    for (int i = 0; i < 3; i++) {
        TelemetryData d = {};
        d.TEL_timestampMs = (uint32_t)(i * 1000);  // eski ts'ler (kesinti dönemi)
        ob_push(d);
    }

    Telemetry tel;
    tel.begin();

    // Replay: 3 buffered paket, TX sırasına göre ardışık yeni seq alır.
    TelemetryData replay = {};
    while (ob_peek(replay)) {
        tel.sendStatus(TelemetrySanitize::sanitizeForUplink(replay));
        TEST_ASSERT_TRUE(ob_drop_front());
    }

    // Canlı paket: replay'den hemen sonraki seq'i alır.
    TelemetryData live = {};
    live.TEL_timestampMs = 999999u;  // güncel ts — replay'lerden ileride
    tel.sendStatus(TelemetrySanitize::sanitizeForUplink(live));

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,0,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,1,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,2,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,2,3,"));

    // seq 0,1,2 sırasıyla ts=0,1000,2000 taşımalı (replay sırası korunur).
    // Payload sonu "...,bmsValid,tsMs,spdX10\r\n" -> bmsValid=0 sabit.
    TEST_ASSERT_NOT_NULL(strstr(buf, ",0,0,0\r\n"));       // seq0: ts=0
    TEST_ASSERT_NOT_NULL(strstr(buf, ",0,1000,0\r\n"));    // seq1: ts=1000
    TEST_ASSERT_NOT_NULL(strstr(buf, ",0,2000,0\r\n"));    // seq2: ts=2000
    TEST_ASSERT_NOT_NULL(strstr(buf, ",0,999999,0\r\n"));  // seq3 (canlı): ts=999999
}
