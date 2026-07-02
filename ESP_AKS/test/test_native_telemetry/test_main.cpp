#include <unity.h>

// Faz 4 — Telemetry formatlama testleri (v2 protokolü)
extern void test_no_write_before_begin(void);
extern void test_first_packet_has_v1_seq0_prefix(void);
extern void test_sequence_increments(void);
extern void test_begin_resets_sequence(void);
extern void test_packet_ends_with_crlf(void);
extern void test_negative_torque_is_formatted(void);
extern void test_negative_current_is_formatted(void);
extern void test_negative_temperature_is_formatted(void);
extern void test_motor_valid_renders_as_one(void);
extern void test_motor_timeout_renders_as_one(void);
extern void test_bms_valid_renders_as_one(void);
extern void test_full_format_with_distinct_values(void);
extern void test_two_packets_have_separator(void);
// v2 yeni alanlar
extern void test_ts_ms_is_encoded(void);
extern void test_spd_x10_is_encoded(void);
// rpmToSpeedKmhX10 birim testleri
extern void test_rpm_to_speed_zero(void);
extern void test_rpm_to_speed_typical(void);
extern void test_rpm_to_speed_clamp(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_no_write_before_begin);
    RUN_TEST(test_first_packet_has_v1_seq0_prefix);
    RUN_TEST(test_sequence_increments);
    RUN_TEST(test_begin_resets_sequence);
    RUN_TEST(test_packet_ends_with_crlf);
    RUN_TEST(test_negative_torque_is_formatted);
    RUN_TEST(test_negative_current_is_formatted);
    RUN_TEST(test_negative_temperature_is_formatted);
    RUN_TEST(test_motor_valid_renders_as_one);
    RUN_TEST(test_motor_timeout_renders_as_one);
    RUN_TEST(test_bms_valid_renders_as_one);
    RUN_TEST(test_full_format_with_distinct_values);
    RUN_TEST(test_two_packets_have_separator);
    RUN_TEST(test_ts_ms_is_encoded);
    RUN_TEST(test_spd_x10_is_encoded);
    RUN_TEST(test_rpm_to_speed_zero);
    RUN_TEST(test_rpm_to_speed_typical);
    RUN_TEST(test_rpm_to_speed_clamp);

    return UNITY_END();
}
