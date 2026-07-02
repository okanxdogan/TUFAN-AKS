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

    return UNITY_END();
}
