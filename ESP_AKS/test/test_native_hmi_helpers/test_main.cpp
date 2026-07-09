#include <unity.h>

// Faz 5 — HMI helpers (saf metin + change-cache UART yazıcılar)

// State text
extern void test_state_text_init(void);
extern void test_state_text_idle(void);
extern void test_state_text_ready(void);
extern void test_state_text_drive(void);
extern void test_state_text_emergency_stop(void);
extern void test_state_text_fault(void);
extern void test_state_text_unknown_falls_back(void);

// Error format
extern void test_error_format_zero(void);
extern void test_error_format_full_byte(void);
extern void test_error_format_specific_value(void);
extern void test_error_format_uppercase_hex(void);
extern void test_error_format_zero_buffer_size_no_crash(void);
extern void test_error_format_truncates_to_small_buffer(void);

// Validity / contactor
extern void test_validity_valid_no_timeout(void);
extern void test_validity_invalid_no_timeout(void);
extern void test_validity_timeout_overrides_invalid(void);
extern void test_validity_timeout_overrides_valid(void);
extern void test_contactor_closed(void);
extern void test_contactor_open(void);

// Change cache
extern void test_numeric_same_value_no_force_skips_write(void);
extern void test_numeric_changed_value_writes_command(void);
extern void test_numeric_force_writes_even_when_unchanged(void);
extern void test_numeric_negative_value_formatted(void);
extern void test_text_same_value_no_force_skips_write(void);
extern void test_text_changed_value_writes_command(void);
extern void test_text_force_writes_even_when_unchanged(void);
extern void test_text_terminated_with_end_bytes(void);
extern void test_sendEndBytes_writes_three_ff(void);

// "Veri yok" gösterimi (UNVERIFIED SOC/sıcaklık sentinelleri)
extern void test_battery_unverified_source_returns_no_data(void);
extern void test_temp_unverified_source_returns_no_data(void);
extern void test_production_source_verified_flags_are_true(void);
extern void test_battery_invalid_bms_returns_no_data(void);
extern void test_temp_invalid_bms_returns_no_data(void);
extern void test_battery_verified_valid_converts_hundredths_to_percent(void);
extern void test_battery_verified_valid_clamps_above_100(void);
extern void test_temp_verified_valid_passes_through(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_state_text_init);
    RUN_TEST(test_state_text_idle);
    RUN_TEST(test_state_text_ready);
    RUN_TEST(test_state_text_drive);
    RUN_TEST(test_state_text_emergency_stop);
    RUN_TEST(test_state_text_fault);
    RUN_TEST(test_state_text_unknown_falls_back);

    RUN_TEST(test_error_format_zero);
    RUN_TEST(test_error_format_full_byte);
    RUN_TEST(test_error_format_specific_value);
    RUN_TEST(test_error_format_uppercase_hex);
    RUN_TEST(test_error_format_zero_buffer_size_no_crash);
    RUN_TEST(test_error_format_truncates_to_small_buffer);

    RUN_TEST(test_validity_valid_no_timeout);
    RUN_TEST(test_validity_invalid_no_timeout);
    RUN_TEST(test_validity_timeout_overrides_invalid);
    RUN_TEST(test_validity_timeout_overrides_valid);
    RUN_TEST(test_contactor_closed);
    RUN_TEST(test_contactor_open);

    RUN_TEST(test_numeric_same_value_no_force_skips_write);
    RUN_TEST(test_numeric_changed_value_writes_command);
    RUN_TEST(test_numeric_force_writes_even_when_unchanged);
    RUN_TEST(test_numeric_negative_value_formatted);
    RUN_TEST(test_text_same_value_no_force_skips_write);
    RUN_TEST(test_text_changed_value_writes_command);
    RUN_TEST(test_text_force_writes_even_when_unchanged);
    RUN_TEST(test_text_terminated_with_end_bytes);
    RUN_TEST(test_sendEndBytes_writes_three_ff);

    RUN_TEST(test_battery_unverified_source_returns_no_data);
    RUN_TEST(test_temp_unverified_source_returns_no_data);
    RUN_TEST(test_production_source_verified_flags_are_true);
    RUN_TEST(test_battery_invalid_bms_returns_no_data);
    RUN_TEST(test_temp_invalid_bms_returns_no_data);
    RUN_TEST(test_battery_verified_valid_converts_hundredths_to_percent);
    RUN_TEST(test_battery_verified_valid_clamps_above_100);
    RUN_TEST(test_temp_verified_valid_passes_through);

    return UNITY_END();
}
