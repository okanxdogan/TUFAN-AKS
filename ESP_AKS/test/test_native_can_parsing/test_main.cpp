#include <unity.h>

// Faz 3 — CAN parsing testleri.
// Tanımlar test_motor_status_parse.cpp, test_bms_config_parse.cpp,
// test_bms_live_parse.cpp, test_motor_timeout.cpp ve test_autobaud_policy.cpp
// içindedir.

// Motor status (MSTest/mock_motor doğrulanmış 8-byte payload)
extern void test_motor_status_dlc_too_short(void);
extern void test_motor_status_dlc_7_too_short(void);
extern void test_motor_status_dlc_8_valid(void);
extern void test_motor_status_rpm_big_endian(void);
extern void test_motor_status_rpm_zero(void);
extern void test_motor_status_rpm_max(void);
extern void test_motor_status_voltage_parsing(void);
extern void test_motor_status_error_flags_byte7(void);
extern void test_motor_status_motor_running_flag(void);
extern void test_motor_status_motor_stopped_flag(void);
extern void test_motor_status_invalid_does_not_modify_out(void);

// Lithium Balance c-BMS — CAN ID 0xE000 (DOĞRULANDI: akım/packV/SoC1/SoC2)
// Tanımlar test_bms_config_parse.cpp içindedir.
extern void test_e000_dlc_too_short(void);
extern void test_e000_parsing_nominal(void);
extern void test_e000_parsing_negative_current(void);
extern void test_e000_current_scale_round_trip(void);
extern void test_e000_current_13A_trips_critical_threshold(void);
extern void test_e000_preserves_other_fields(void);

// Lithium Balance c-BMS — CAN ID 0xE001 sıcaklık (DOĞRULANDI)
extern void test_e001_dlc_too_short(void);
extern void test_e001_parsing_temps(void);

// checkPackVoltageFault — saf pack voltajı eşik kontrolü (DOĞRULANMIŞ sinyal)
extern void test_packv_fault_599_is_undervoltage(void);
extern void test_packv_fault_600_boundary_is_undervoltage(void);
extern void test_packv_fault_790_is_ok(void);
extern void test_packv_fault_601_875_band_is_ok(void);
extern void test_packv_fault_876_boundary_is_overvoltage(void);
extern void test_packv_fault_877_is_overvoltage(void);

// Charger komut frame'i — CAN ID 0x1806E5F4 (DOĞRULANDI)
extern void test_charger_dlc_too_short(void);
extern void test_charger_session2_frame(void);
extern void test_charger_big_endian_order(void);
extern void test_charger_dlc_4_minimum_ok(void);

// GERÇEK LOG FRAME TESTLERİ — Oturum 3 zemin gerçeği
// Tanımlar test_bms_live_parse.cpp içindedir.
extern void test_e000_real_log_frame_session3_sample1(void);
extern void test_e000_real_log_frame_session3_sample2(void);
extern void test_e000_real_log_frame_session3_sample3(void);
extern void test_e001_real_log_frame_session3_sample1(void);
extern void test_e001_max_min_reversed(void);
extern void test_e001_negative_temps(void);
extern void test_e001_preserves_other_fields(void);

// BİLİNMİYOR ID stub'ları (E002-E033)
extern void test_e002_stub_accepts_valid_dlc(void);
extern void test_e032_stub_accepts_valid_dlc(void);
extern void test_e033_stub_accepts_valid_dlc(void);
extern void test_stubs_do_not_write_telemetry(void);

// Cell voltage parsing (E015-E020) and E001 min/max/avg
extern void test_e015_dlc_too_short(void);
extern void test_e015_parses_four_cells_correctly(void);
extern void test_e015_cells_in_lifepo4_range(void);
extern void test_e020_parses_four_cells_correctly(void);
extern void test_e001_parses_min_max_avg_cell_voltages(void);
extern void test_e001_preserves_temperature_fields(void);

// Motor timeout
extern void test_timeout_not_seen_yet(void);
extern void test_timeout_already_invalidated(void);
extern void test_timeout_within_window(void);
extern void test_timeout_at_threshold(void);
extern void test_timeout_past_threshold(void);
extern void test_timeout_just_received(void);
extern void test_timeout_tick_wraparound_within_window(void);
extern void test_timeout_tick_wraparound_at_threshold(void);

// BMS timeout
extern void test_bms_timeout_not_seen_yet(void);
extern void test_bms_timeout_already_invalidated(void);
extern void test_bms_timeout_within_window(void);
extern void test_bms_timeout_at_threshold(void);
extern void test_bms_timeout_past_threshold(void);

// CAN Autobaud Retry Policy — kalıcı sağırlık düzeltmesi (bkz.
// AutobaudPolicy.h / Documents/BRING_UP_CHECKLIST.md bölüm 4)
extern void test_pre_reception_interval_elapsed_retries(void);
extern void test_pre_reception_interval_not_elapsed_no_retry(void);
extern void test_interval_boundary_triggers(void);
extern void test_verified_never_retries_even_if_interval_elapsed(void);
extern void test_frame_received_never_retries_even_if_interval_elapsed(void);
extern void test_both_flags_true_no_retry(void);
extern void test_tick_wraparound_within_window_no_retry(void);
extern void test_tick_wraparound_at_threshold_retries(void);
extern void test_post_reception_timeout_is_out_of_scope(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Motor status parse (MSTest/mock_motor doğrulanmış format)
    RUN_TEST(test_motor_status_dlc_too_short);
    RUN_TEST(test_motor_status_dlc_7_too_short);
    RUN_TEST(test_motor_status_dlc_8_valid);
    RUN_TEST(test_motor_status_rpm_big_endian);
    RUN_TEST(test_motor_status_rpm_zero);
    RUN_TEST(test_motor_status_rpm_max);
    RUN_TEST(test_motor_status_voltage_parsing);
    RUN_TEST(test_motor_status_error_flags_byte7);
    RUN_TEST(test_motor_status_motor_running_flag);
    RUN_TEST(test_motor_status_motor_stopped_flag);
    RUN_TEST(test_motor_status_invalid_does_not_modify_out);

    // Lithium Balance c-BMS — CAN ID 0xE000 (DOĞRULANDI: akım/packV/SoC)
    RUN_TEST(test_e000_dlc_too_short);
    RUN_TEST(test_e000_parsing_nominal);
    RUN_TEST(test_e000_parsing_negative_current);
    RUN_TEST(test_e000_current_scale_round_trip);
    RUN_TEST(test_e000_current_13A_trips_critical_threshold);
    RUN_TEST(test_e000_preserves_other_fields);

    // Lithium Balance c-BMS — CAN ID 0xE001 sıcaklık (DOĞRULANDI)
    RUN_TEST(test_e001_dlc_too_short);
    RUN_TEST(test_e001_parsing_temps);

    // checkPackVoltageFault — saf pack voltajı eşik kontrolü
    RUN_TEST(test_packv_fault_599_is_undervoltage);
    RUN_TEST(test_packv_fault_600_boundary_is_undervoltage);
    RUN_TEST(test_packv_fault_790_is_ok);
    RUN_TEST(test_packv_fault_601_875_band_is_ok);
    RUN_TEST(test_packv_fault_876_boundary_is_overvoltage);
    RUN_TEST(test_packv_fault_877_is_overvoltage);

    // Charger komut frame'i — CAN ID 0x1806E5F4 (DOĞRULANDI)
    RUN_TEST(test_charger_dlc_too_short);
    RUN_TEST(test_charger_session2_frame);
    RUN_TEST(test_charger_big_endian_order);
    RUN_TEST(test_charger_dlc_4_minimum_ok);

    // GERÇEK LOG FRAME TESTLERİ — Oturum 3 (zemin gerçeği)
    RUN_TEST(test_e000_real_log_frame_session3_sample1);
    RUN_TEST(test_e000_real_log_frame_session3_sample2);
    RUN_TEST(test_e000_real_log_frame_session3_sample3);
    RUN_TEST(test_e001_real_log_frame_session3_sample1);
    RUN_TEST(test_e001_max_min_reversed);
    RUN_TEST(test_e001_negative_temps);
    RUN_TEST(test_e001_preserves_other_fields);

    // BİLİNMİYOR ID stub'ları (E002-E033)
    RUN_TEST(test_e002_stub_accepts_valid_dlc);
    RUN_TEST(test_e032_stub_accepts_valid_dlc);
    RUN_TEST(test_e033_stub_accepts_valid_dlc);
    RUN_TEST(test_stubs_do_not_write_telemetry);

    // Cell voltage parsing (E015-E020) and E001 min/max/avg
    RUN_TEST(test_e015_dlc_too_short);
    RUN_TEST(test_e015_parses_four_cells_correctly);
    RUN_TEST(test_e015_cells_in_lifepo4_range);
    RUN_TEST(test_e020_parses_four_cells_correctly);
    RUN_TEST(test_e001_parses_min_max_avg_cell_voltages);
    RUN_TEST(test_e001_preserves_temperature_fields);

    // Motor timeout
    RUN_TEST(test_timeout_not_seen_yet);
    RUN_TEST(test_timeout_already_invalidated);
    RUN_TEST(test_timeout_within_window);
    RUN_TEST(test_timeout_at_threshold);
    RUN_TEST(test_timeout_past_threshold);
    RUN_TEST(test_timeout_just_received);
    RUN_TEST(test_timeout_tick_wraparound_within_window);
    RUN_TEST(test_timeout_tick_wraparound_at_threshold);

    // BMS timeout
    RUN_TEST(test_bms_timeout_not_seen_yet);
    RUN_TEST(test_bms_timeout_already_invalidated);
    RUN_TEST(test_bms_timeout_within_window);
    RUN_TEST(test_bms_timeout_at_threshold);
    RUN_TEST(test_bms_timeout_past_threshold);

    // CAN Autobaud Retry Policy
    RUN_TEST(test_pre_reception_interval_elapsed_retries);
    RUN_TEST(test_pre_reception_interval_not_elapsed_no_retry);
    RUN_TEST(test_interval_boundary_triggers);
    RUN_TEST(test_verified_never_retries_even_if_interval_elapsed);
    RUN_TEST(test_frame_received_never_retries_even_if_interval_elapsed);
    RUN_TEST(test_both_flags_true_no_retry);
    RUN_TEST(test_tick_wraparound_within_window_no_retry);
    RUN_TEST(test_tick_wraparound_at_threshold_retries);
    RUN_TEST(test_post_reception_timeout_is_out_of_scope);

    return UNITY_END();
}
