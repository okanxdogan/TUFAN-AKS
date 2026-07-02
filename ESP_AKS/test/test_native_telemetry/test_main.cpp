#include <unity.h>

// TEKNOFEST-compliant telemetry format tests
extern void test_no_write_before_begin(void);
extern void test_semicolon_separator(void);
extern void test_field_order(void);
extern void test_speed_placeholder_zero(void);
extern void test_energy_placeholder_zero(void);
extern void test_packet_ends_with_crlf(void);
extern void test_negative_temperature_is_formatted(void);
extern void test_full_format_with_distinct_values(void);
extern void test_two_packets_have_separator(void);
extern void test_begin_resets_state(void);
extern void test_timestamp_encoded_first_field(void);
extern void test_pack_voltage_encoded(void);
// rpmToSpeedKmhX10 unit tests
extern void test_rpm_to_speed_zero(void);
extern void test_rpm_to_speed_typical(void);
extern void test_rpm_to_speed_clamp(void);
extern void test_rpm_to_speed_clamp_at_rpm_20000(void);
extern void test_rpm_to_speed_clamp_just_above_threshold_rpm(void);
extern void test_rpm_to_speed_no_clamp_just_below_threshold_rpm(void);

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

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_no_write_before_begin);
    RUN_TEST(test_semicolon_separator);
    RUN_TEST(test_field_order);
    RUN_TEST(test_speed_placeholder_zero);
    RUN_TEST(test_energy_placeholder_zero);
    RUN_TEST(test_packet_ends_with_crlf);
    RUN_TEST(test_negative_temperature_is_formatted);
    RUN_TEST(test_full_format_with_distinct_values);
    RUN_TEST(test_two_packets_have_separator);
    RUN_TEST(test_begin_resets_state);
    RUN_TEST(test_timestamp_encoded_first_field);
    RUN_TEST(test_pack_voltage_encoded);
    RUN_TEST(test_rpm_to_speed_zero);
    RUN_TEST(test_rpm_to_speed_typical);
    RUN_TEST(test_rpm_to_speed_clamp);
    RUN_TEST(test_rpm_to_speed_clamp_at_rpm_20000);
    RUN_TEST(test_rpm_to_speed_clamp_just_above_threshold_rpm);
    RUN_TEST(test_rpm_to_speed_no_clamp_just_below_threshold_rpm);

    RUN_TEST(test_sanitize_system_state_valid_passthrough);
    RUN_TEST(test_sanitize_system_state_zero_becomes_fault);
    RUN_TEST(test_sanitize_system_state_five_becomes_fault);
    RUN_TEST(test_sanitize_soc_within_range_passthrough);
    RUN_TEST(test_sanitize_soc_at_max_passthrough);
    RUN_TEST(test_sanitize_soc_above_max_clamped);
    RUN_TEST(test_sanitize_current_int32_min_shifted);
    RUN_TEST(test_sanitize_current_int32_min_plus_one_unchanged);
    RUN_TEST(test_sanitize_current_normal_passthrough);

    return UNITY_END();
}
