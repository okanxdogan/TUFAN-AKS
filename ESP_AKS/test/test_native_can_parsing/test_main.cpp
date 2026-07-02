#include <unity.h>

// Faz 3 — CAN parsing testleri.
// Tanımlar test_motor_status_parse.cpp, test_bms_config_parse.cpp,
// test_bms_live_parse.cpp ve test_motor_timeout.cpp içindedir.

// Motor status
extern void test_motor_status_dlc_too_short(void);
extern void test_motor_status_dlc_4_no_error_flag(void);
extern void test_motor_status_dlc_5_with_error_flag(void);
extern void test_motor_status_dlc_8_ok(void);
extern void test_motor_status_rpm_big_endian(void);
extern void test_motor_status_rpm_zero(void);
extern void test_motor_status_rpm_max(void);
extern void test_motor_status_torque_negative(void);
extern void test_motor_status_torque_positive(void);
extern void test_motor_status_torque_min_int16(void);
extern void test_motor_status_invalid_does_not_modify_out(void);

// BMS config
extern void test_bms_config_dlc_too_short(void);
extern void test_bms_config_pack_voltage_big_endian(void);
extern void test_bms_config_cell_voltage_big_endian(void);
extern void test_bms_config_temp_highest_signed(void);
extern void test_bms_config_temp_lowest_signed(void);
extern void test_bms_config_temp_negative(void);
extern void test_bms_config_system_state(void);
extern void test_bms_config_system_state_fault(void);
extern void test_bms_config_sets_valid_flag(void);
extern void test_bms_config_preserves_other_fields(void);

// BMS live
extern void test_bms_live_dlc_too_short(void);
extern void test_bms_live_pack_voltage_big_endian(void);
extern void test_bms_live_error_flags(void);
extern void test_bms_live_current_signed_minus_one(void);
extern void test_bms_live_current_signed_min(void);
extern void test_bms_live_current_signed_max(void);
extern void test_bms_live_current_negative(void);
extern void test_bms_live_current_positive(void);
extern void test_bms_live_current_zero(void);
extern void test_bms_live_soc_big_endian(void);
extern void test_bms_live_soc_full(void);
extern void test_bms_live_sets_valid_flag(void);
extern void test_bms_live_preserves_other_fields(void);

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

    // Motor status parse
    RUN_TEST(test_motor_status_dlc_too_short);
    RUN_TEST(test_motor_status_dlc_4_no_error_flag);
    RUN_TEST(test_motor_status_dlc_5_with_error_flag);
    RUN_TEST(test_motor_status_dlc_8_ok);
    RUN_TEST(test_motor_status_rpm_big_endian);
    RUN_TEST(test_motor_status_rpm_zero);
    RUN_TEST(test_motor_status_rpm_max);
    RUN_TEST(test_motor_status_torque_negative);
    RUN_TEST(test_motor_status_torque_positive);
    RUN_TEST(test_motor_status_torque_min_int16);
    RUN_TEST(test_motor_status_invalid_does_not_modify_out);

    // BMS config parse
    RUN_TEST(test_bms_config_dlc_too_short);
    RUN_TEST(test_bms_config_pack_voltage_big_endian);
    RUN_TEST(test_bms_config_cell_voltage_big_endian);
    RUN_TEST(test_bms_config_temp_highest_signed);
    RUN_TEST(test_bms_config_temp_lowest_signed);
    RUN_TEST(test_bms_config_temp_negative);
    RUN_TEST(test_bms_config_system_state);
    RUN_TEST(test_bms_config_system_state_fault);
    RUN_TEST(test_bms_config_sets_valid_flag);
    RUN_TEST(test_bms_config_preserves_other_fields);

    // BMS live parse
    RUN_TEST(test_bms_live_dlc_too_short);
    RUN_TEST(test_bms_live_pack_voltage_big_endian);
    RUN_TEST(test_bms_live_error_flags);
    RUN_TEST(test_bms_live_current_signed_minus_one);
    RUN_TEST(test_bms_live_current_signed_min);
    RUN_TEST(test_bms_live_current_signed_max);
    RUN_TEST(test_bms_live_current_negative);
    RUN_TEST(test_bms_live_current_positive);
    RUN_TEST(test_bms_live_current_zero);
    RUN_TEST(test_bms_live_soc_big_endian);
    RUN_TEST(test_bms_live_soc_full);
    RUN_TEST(test_bms_live_sets_valid_flag);
    RUN_TEST(test_bms_live_preserves_other_fields);

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
