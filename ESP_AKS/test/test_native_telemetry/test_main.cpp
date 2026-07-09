#include <unity.h>

// Faz 4 — Telemetry formatlama testleri (v2 protokolü)
extern void test_no_write_before_begin(void);
extern void test_first_packet_has_v1_seq0_prefix(void);
extern void test_sequence_increments(void);
extern void test_begin_resets_sequence(void);
extern void test_packet_ends_with_crlf(void);
extern void test_motor_voltage_is_formatted(void);
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
extern void test_rpm_to_speed_clamp_at_rpm_20000(void);
extern void test_rpm_to_speed_clamp_just_above_threshold_rpm(void);
extern void test_rpm_to_speed_no_clamp_just_below_threshold_rpm(void);
extern void test_impl_hand_calc_motor_rpm_with_gear_ratio(void);
extern void test_impl_hand_calc_different_wheel_and_gear_ratio(void);
extern void test_impl_motor_rpm_is_wheel_rpm_skips_gear_ratio(void);
extern void test_impl_motor_rpm_false_applies_gear_ratio(void);
extern void test_impl_applies_clamp(void);

// TelemetrySanitize (UKS aralik-disi alan sanitizasyonu) birim testleri
extern void test_sanitize_system_state_valid_passthrough(void);
extern void test_sanitize_system_state_zero_becomes_fault(void);
extern void test_sanitize_system_state_five_becomes_fault(void);
extern void test_sanitize_soc_within_range_passthrough(void);
extern void test_sanitize_soc_at_max_passthrough(void);
extern void test_sanitize_soc_above_max_clamped(void);
extern void test_sanitize_current_int32_min_shifted(void);
extern void test_sanitize_current_int32_min_plus_one_unchanged(void);
extern void test_sanitize_current_normal_passthrough(void);
extern void test_sanitize_for_uplink_passthrough_when_all_valid(void);
extern void test_sanitize_for_uplink_corrects_invalid_system_state(void);
extern void test_sanitize_for_uplink_corrects_soc_and_current_together(void);

// Replay sanitize-sırası (S4) ve seq semantiği (madde 4) testleri
extern void test_replay_output_sanitizes_corrupted_system_state(void);
extern void test_replay_output_sanitizes_zero_system_state(void);
extern void test_replay_then_live_seq_is_sequential_and_monotonic(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_no_write_before_begin);
    RUN_TEST(test_first_packet_has_v1_seq0_prefix);
    RUN_TEST(test_sequence_increments);
    RUN_TEST(test_begin_resets_sequence);
    RUN_TEST(test_packet_ends_with_crlf);
    RUN_TEST(test_motor_voltage_is_formatted);
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
    RUN_TEST(test_rpm_to_speed_clamp_at_rpm_20000);
    RUN_TEST(test_rpm_to_speed_clamp_just_above_threshold_rpm);
    RUN_TEST(test_rpm_to_speed_no_clamp_just_below_threshold_rpm);
    RUN_TEST(test_impl_hand_calc_motor_rpm_with_gear_ratio);
    RUN_TEST(test_impl_hand_calc_different_wheel_and_gear_ratio);
    RUN_TEST(test_impl_motor_rpm_is_wheel_rpm_skips_gear_ratio);
    RUN_TEST(test_impl_motor_rpm_false_applies_gear_ratio);
    RUN_TEST(test_impl_applies_clamp);

    RUN_TEST(test_sanitize_system_state_valid_passthrough);
    RUN_TEST(test_sanitize_system_state_zero_becomes_fault);
    RUN_TEST(test_sanitize_system_state_five_becomes_fault);
    RUN_TEST(test_sanitize_soc_within_range_passthrough);
    RUN_TEST(test_sanitize_soc_at_max_passthrough);
    RUN_TEST(test_sanitize_soc_above_max_clamped);
    RUN_TEST(test_sanitize_current_int32_min_shifted);
    RUN_TEST(test_sanitize_current_int32_min_plus_one_unchanged);
    RUN_TEST(test_sanitize_current_normal_passthrough);
    RUN_TEST(test_sanitize_for_uplink_passthrough_when_all_valid);
    RUN_TEST(test_sanitize_for_uplink_corrects_invalid_system_state);
    RUN_TEST(test_sanitize_for_uplink_corrects_soc_and_current_together);

    RUN_TEST(test_replay_output_sanitizes_corrupted_system_state);
    RUN_TEST(test_replay_output_sanitizes_zero_system_state);
    RUN_TEST(test_replay_then_live_seq_is_sequential_and_monotonic);

    return UNITY_END();
}
