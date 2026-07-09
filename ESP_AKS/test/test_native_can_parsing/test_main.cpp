#include <unity.h>

// Faz 3 — CAN parsing testleri.
// Tanımlar test_motor_status_parse.cpp, test_bms_config_parse.cpp,
// test_bms_live_parse.cpp ve test_motor_timeout.cpp içindedir.

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

// Lithium Balance c-BMS — CAN ID 0xE000 (DOĞRULANDI: packV)
extern void test_e000_dlc_too_short(void);
extern void test_e000_packv_big_endian(void);
extern void test_e000_packv_zero(void);
extern void test_e000_packv_max_uint16(void);
extern void test_e000_packv_nominal_78v(void);
extern void test_e000_sets_valid_flag(void);
extern void test_e000_preserves_other_fields(void);
extern void test_e000_dlc_8_raw_fields_parsed(void);
extern void test_e000_session2_idle_frame(void);
extern void test_e000_session2_end_frame(void);
extern void test_e000_dlc_2_too_short(void);
extern void test_e000_dlc_6_counter2_deterministic_zero(void);

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

// Lithium Balance c-BMS — DOĞRULANMAMIŞ ID stub'ları
extern void test_e001_stub_accepts_valid_dlc(void);
extern void test_e001_stub_rejects_zero_dlc(void);
extern void test_e002_stub_accepts_valid_dlc(void);
extern void test_e032_stub_accepts_valid_dlc(void);
extern void test_e033_stub_accepts_valid_dlc(void);
extern void test_stubs_do_not_write_telemetry(void);

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

    // Lithium Balance c-BMS — CAN ID 0xE000 (DOĞRULANDI: packV)
    RUN_TEST(test_e000_dlc_too_short);
    RUN_TEST(test_e000_packv_big_endian);
    RUN_TEST(test_e000_packv_zero);
    RUN_TEST(test_e000_packv_max_uint16);
    RUN_TEST(test_e000_packv_nominal_78v);
    RUN_TEST(test_e000_sets_valid_flag);
    RUN_TEST(test_e000_preserves_other_fields);
    RUN_TEST(test_e000_dlc_8_raw_fields_parsed);
    RUN_TEST(test_e000_session2_idle_frame);
    RUN_TEST(test_e000_session2_end_frame);
    RUN_TEST(test_e000_dlc_2_too_short);
    RUN_TEST(test_e000_dlc_6_counter2_deterministic_zero);

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

    // Lithium Balance c-BMS — DOĞRULANMAMIŞ ID stub'ları
    RUN_TEST(test_e001_stub_accepts_valid_dlc);
    RUN_TEST(test_e001_stub_rejects_zero_dlc);
    RUN_TEST(test_e002_stub_accepts_valid_dlc);
    RUN_TEST(test_e032_stub_accepts_valid_dlc);
    RUN_TEST(test_e033_stub_accepts_valid_dlc);
    RUN_TEST(test_stubs_do_not_write_telemetry);

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

    return UNITY_END();
}
