#include <unity.h>

// ---------------------------------------------------------------------------
// Test fonksiyonlarının ileri bildirimleri.
// Tanımlar test_safety_thresholds.cpp ve test_reset_interlock.cpp içindedir.
// ---------------------------------------------------------------------------
// Faz 1 — isCurrentCritical
extern void test_isCurrentCritical_charge_below_threshold(void);
extern void test_isCurrentCritical_charge_at_threshold(void);
extern void test_isCurrentCritical_charge_above_threshold(void);
extern void test_isCurrentCritical_zero_is_safe(void);
extern void test_isCurrentCritical_discharge_below_threshold(void);
extern void test_isCurrentCritical_discharge_at_threshold(void);
extern void test_isCurrentCritical_discharge_above_threshold(void);

// Faz 1 — isCurrentWarning
extern void test_isCurrentWarning_charge_below_threshold(void);
extern void test_isCurrentWarning_charge_at_threshold(void);
extern void test_isCurrentWarning_discharge_below_threshold(void);
extern void test_isCurrentWarning_discharge_at_threshold(void);

// Faz 1 — doğrulanmamış sinyaller karar dışı (Ek B)
extern void test_unverified_temp_not_wired(void);
extern void test_unverified_current_not_wired(void);

// Faz 1 — voltaj eşikleri
extern void test_warning_voltage_above_warn_low(void);
extern void test_warning_voltage_at_warn_low(void);
extern void test_critical_voltage_at_crit_low(void);
extern void test_warning_voltage_below_warn_high(void);
extern void test_warning_voltage_at_warn_high(void);
extern void test_critical_voltage_at_crit_high(void);

// Faz 1 — error flag'ler
extern void test_critical_motor_error_flag_set(void);
extern void test_critical_bms_error_flag_set(void);

// Faz 1 — motor timeout
extern void test_motor_timeout_in_idle_is_safe(void);
extern void test_motor_timeout_in_ready_is_critical(void);
extern void test_motor_timeout_in_drive_is_critical(void);

// Faz 1 — bms timeout (post-reception E000 freshness kaybı)
extern void test_bms_timeout_in_idle_is_safe(void);
extern void test_bms_timeout_in_ready_is_critical(void);
extern void test_bms_timeout_in_drive_is_critical(void);

// Faz 1 — bms data invalid
extern void test_warning_bms_invalid_skips_thresholds(void);
extern void test_critical_bms_invalid_with_motor_error_still_critical(void);

// Faz 1 — baseline & akım uçtan uca
extern void test_baseline_clean_data_no_conditions(void);

// Faz 1 — reset interlock
extern void test_reset_interlock_clean_baseline_passes(void);
extern void test_reset_interlock_motor_error_blocks(void);
extern void test_reset_interlock_bms_error_blocks(void);
extern void test_reset_interlock_unverified_temp_does_not_block(void);
extern void test_reset_interlock_critical_voltage_low_blocks(void);
extern void test_reset_interlock_critical_voltage_high_blocks(void);
extern void test_reset_interlock_unverified_current_does_not_block(void);
extern void test_reset_interlock_motor_timeout_in_fault_blocks(void);
extern void test_reset_interlock_bms_timeout_in_fault_blocks(void);
extern void test_reset_interlock_warning_level_passes(void);
extern void test_reset_interlock_unverified_bms_system_state_does_not_block(void);

// Faz 2 — state machine geçişleri
extern void test_init_transitions_to_idle_and_calls_allOff(void);
extern void test_idle_to_ready_on_start_request(void);
extern void test_idle_start_rejected_when_bms_never_valid(void);
extern void test_idle_start_permitted_when_bms_valid_and_clean(void);
extern void test_idle_start_rejected_when_warning_active(void);
extern void test_ready_to_drive_on_drive_enable(void);
extern void test_idle_to_emergency_stop(void);
extern void test_drive_to_emergency_stop(void);
extern void test_idle_to_fault_on_fault_event(void);
extern void test_ready_to_fault_on_fault_event(void);
extern void test_fault_pending_processed_when_queue_full(void);
extern void test_ready_to_fault_on_critical_telemetry(void);
extern void test_drive_to_fault_on_bms_timeout(void);
extern void test_fault_to_idle_on_reset_when_clean(void);
extern void test_emergency_stop_to_idle_on_reset_when_clean(void);
extern void test_fault_stays_on_reset_when_motor_error(void);
extern void test_idle_reset_is_noop(void);
extern void test_idle_with_motor_timeout_stays_idle(void);
extern void test_emergency_stop_opens_contactors_after_delay(void);
extern void test_idle_with_unverified_bms_system_state_stays_idle(void);

// Faz 2 — G3 actuator fault entegrasyonu
extern void test_run_calls_verifyIfDue_each_tick(void);
extern void test_idle_start_rejected_when_actuator_fault(void);
extern void test_ready_to_fault_on_actuator_fault(void);
extern void test_drive_to_fault_on_actuator_fault(void);
extern void test_reset_from_fault_clears_actuator_fault(void);

// G2 — E-STOP/FAULT sıfır-tork → delay → kontaktör açma sırası
extern void test_estop_requests_zero_torque_before_opening_contactors(void);
extern void test_fault_requests_zero_torque_before_opening_contactors(void);
extern void test_estop_without_torque_sink_still_opens_contactors(void);
extern void test_flag0_torque_frame_disabled(void);

// Faz 0 sanity
static void test_smoke_arithmetic(void) {
    TEST_ASSERT_EQUAL_INT(2, 1 + 1);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Faz 0
    RUN_TEST(test_smoke_arithmetic);

    // Faz 1 — akım eşikleri (saf fonksiyon)
    RUN_TEST(test_isCurrentCritical_charge_below_threshold);
    RUN_TEST(test_isCurrentCritical_charge_at_threshold);
    RUN_TEST(test_isCurrentCritical_charge_above_threshold);
    RUN_TEST(test_isCurrentCritical_zero_is_safe);
    RUN_TEST(test_isCurrentCritical_discharge_below_threshold);
    RUN_TEST(test_isCurrentCritical_discharge_at_threshold);
    RUN_TEST(test_isCurrentCritical_discharge_above_threshold);
    RUN_TEST(test_isCurrentWarning_charge_below_threshold);
    RUN_TEST(test_isCurrentWarning_charge_at_threshold);
    RUN_TEST(test_isCurrentWarning_discharge_below_threshold);
    RUN_TEST(test_isCurrentWarning_discharge_at_threshold);

    // Faz 1 — sıcaklık

    // Faz 1 — pack voltajı
    RUN_TEST(test_unverified_temp_not_wired);
    RUN_TEST(test_unverified_current_not_wired);

    RUN_TEST(test_warning_voltage_above_warn_low);
    RUN_TEST(test_warning_voltage_at_warn_low);
    RUN_TEST(test_critical_voltage_at_crit_low);
    RUN_TEST(test_warning_voltage_below_warn_high);
    RUN_TEST(test_warning_voltage_at_warn_high);
    RUN_TEST(test_critical_voltage_at_crit_high);

    // Faz 1 — error flag'ler
    RUN_TEST(test_critical_motor_error_flag_set);
    RUN_TEST(test_critical_bms_error_flag_set);

    // Faz 1 — motor timeout
    RUN_TEST(test_motor_timeout_in_idle_is_safe);
    RUN_TEST(test_motor_timeout_in_ready_is_critical);
    RUN_TEST(test_motor_timeout_in_drive_is_critical);

    RUN_TEST(test_bms_timeout_in_idle_is_safe);
    RUN_TEST(test_bms_timeout_in_ready_is_critical);
    RUN_TEST(test_bms_timeout_in_drive_is_critical);

    // Faz 1 — bms invalid
    RUN_TEST(test_warning_bms_invalid_skips_thresholds);
    RUN_TEST(test_critical_bms_invalid_with_motor_error_still_critical);

    // Faz 1 — baseline & akım entegre
    RUN_TEST(test_baseline_clean_data_no_conditions);

    // Faz 1 — reset interlock
    RUN_TEST(test_reset_interlock_clean_baseline_passes);
    RUN_TEST(test_reset_interlock_motor_error_blocks);
    RUN_TEST(test_reset_interlock_bms_error_blocks);
    RUN_TEST(test_reset_interlock_unverified_temp_does_not_block);
    RUN_TEST(test_reset_interlock_critical_voltage_low_blocks);
    RUN_TEST(test_reset_interlock_critical_voltage_high_blocks);
    RUN_TEST(test_reset_interlock_unverified_current_does_not_block);
    RUN_TEST(test_reset_interlock_motor_timeout_in_fault_blocks);
    RUN_TEST(test_reset_interlock_bms_timeout_in_fault_blocks);
    RUN_TEST(test_reset_interlock_warning_level_passes);
    RUN_TEST(test_reset_interlock_unverified_bms_system_state_does_not_block);

    // Faz 2 — state machine
    RUN_TEST(test_init_transitions_to_idle_and_calls_allOff);
    RUN_TEST(test_idle_to_ready_on_start_request);
    RUN_TEST(test_idle_start_rejected_when_bms_never_valid);
    RUN_TEST(test_idle_start_permitted_when_bms_valid_and_clean);
    RUN_TEST(test_idle_start_rejected_when_warning_active);
    RUN_TEST(test_ready_to_drive_on_drive_enable);
    RUN_TEST(test_idle_to_emergency_stop);
    RUN_TEST(test_drive_to_emergency_stop);
    RUN_TEST(test_idle_to_fault_on_fault_event);
    RUN_TEST(test_ready_to_fault_on_fault_event);
    RUN_TEST(test_fault_pending_processed_when_queue_full);
    RUN_TEST(test_ready_to_fault_on_critical_telemetry);
    RUN_TEST(test_drive_to_fault_on_bms_timeout);
    RUN_TEST(test_fault_to_idle_on_reset_when_clean);
    RUN_TEST(test_emergency_stop_to_idle_on_reset_when_clean);
    RUN_TEST(test_fault_stays_on_reset_when_motor_error);
    RUN_TEST(test_idle_reset_is_noop);
    RUN_TEST(test_idle_with_motor_timeout_stays_idle);
    RUN_TEST(test_emergency_stop_opens_contactors_after_delay);
    RUN_TEST(test_idle_with_unverified_bms_system_state_stays_idle);

    RUN_TEST(test_run_calls_verifyIfDue_each_tick);
    RUN_TEST(test_idle_start_rejected_when_actuator_fault);
    RUN_TEST(test_ready_to_fault_on_actuator_fault);
    RUN_TEST(test_drive_to_fault_on_actuator_fault);
    RUN_TEST(test_reset_from_fault_clears_actuator_fault);

    RUN_TEST(test_estop_requests_zero_torque_before_opening_contactors);
    RUN_TEST(test_fault_requests_zero_torque_before_opening_contactors);
    RUN_TEST(test_estop_without_torque_sink_still_opens_contactors);
    RUN_TEST(test_flag0_torque_frame_disabled);

    return UNITY_END();
}
