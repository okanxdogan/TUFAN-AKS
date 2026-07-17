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

// Faz 1 — sıcaklık eşikleri (≥55 WARN / ≥70 FAULT, hasWarning/hasCritical'a bağlı)
extern void test_temp_below_warn_is_clean(void);
extern void test_temp_at_warn_threshold_is_warning(void);
extern void test_temp_below_crit_is_warning_only(void);
extern void test_temp_at_crit_threshold_is_critical(void);

// Faz 1 — akım eşikleri uçtan uca (DOĞRULANDI + bağlandı; şarj 11/13 A,
// deşarj 9/15 A) + 9.9 A nominal şarj regresyonu
extern void test_nominal_charge_current_no_fault(void);
extern void test_charge_current_at_warn_is_warning_only(void);
extern void test_charge_current_at_crit_is_critical(void);
extern void test_discharge_current_at_crit_is_critical(void);

// Faz 1 — voltaj eşikleri
extern void test_warning_voltage_above_warn_low(void);
extern void test_warning_voltage_at_warn_low(void);
extern void test_critical_voltage_at_crit_low(void);
extern void test_warning_voltage_below_warn_high(void);
extern void test_warning_voltage_at_warn_high(void);
extern void test_critical_voltage_at_crit_high(void);

// GÜVENLİK-EŞİĞİ DÜZELTMESİ — hücre voltajı deci-mV/mV birim uyumsuzluğu
extern void test_cell_voltage_realistic_nominal_no_condition(void);
extern void test_cell_undervoltage_warn_threshold_deci_mv(void);
extern void test_cell_undervoltage_critical_realistic(void);
extern void test_cell_overvoltage_warn_threshold_deci_mv(void);
extern void test_cell_overvoltage_critical_realistic(void);

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
extern void test_reset_interlock_critical_temp_blocks(void);
extern void test_reset_interlock_warning_temp_does_not_block(void);
extern void test_reset_interlock_critical_voltage_low_blocks(void);
extern void test_reset_interlock_critical_voltage_high_blocks(void);
extern void test_reset_interlock_critical_current_blocks(void);
extern void test_reset_interlock_nominal_charge_current_does_not_block(void);
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
extern void test_idle_to_ready_with_charger_active_when_roles_unassigned(void);
extern void test_ready_to_drive_on_drive_enable(void);
extern void test_idle_to_emergency_stop(void);
extern void test_drive_to_emergency_stop(void);
extern void test_idle_to_fault_on_fault_event(void);
extern void test_ready_to_fault_on_fault_event(void);
extern void test_fault_pending_processed_when_queue_full(void);
extern void test_ready_to_fault_on_critical_telemetry(void);
extern void test_ready_to_fault_on_critical_temp(void);
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
extern void test_estop_zero_torque_reaches_can_queue_before_contactor_open(void);
extern void test_flag0_torque_frame_disabled(void);

// G2 — TorqueRequestQueue (VCU task -> CAN task tork isteği kuyruğu)
extern void test_torque_queue_drain_empty_returns_false(void);
extern void test_torque_queue_push_then_drain_returns_value_once(void);
extern void test_torque_queue_zero_value_is_a_valid_pending_request(void);
extern void test_torque_queue_overwrites_undrained_value_with_latest(void);
extern void test_torque_queue_push_after_drain_is_pending_again(void);

// B12 — DeratingPolicy (WARN bandı tork-izin yüzdesi iskeleti)
extern void test_derating_neutral_when_bms_data_invalid(void);
extern void test_derating_nominal_when_all_signals_clean(void);
extern void test_derating_temp_just_below_warn_is_nominal(void);
extern void test_derating_temp_at_warn_threshold_is_warning_tier(void);
extern void test_derating_temp_just_below_approach_boundary_is_warning_tier(void);
extern void test_derating_temp_at_approach_boundary_is_approaching_critical_tier(void);
extern void test_derating_temp_at_critical_threshold_is_still_approaching_tier(void);
extern void test_derating_charge_current_below_warn_is_nominal(void);
extern void test_derating_charge_current_at_warn_is_warning_tier(void);
extern void test_derating_charge_current_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_discharge_current_below_warn_is_nominal(void);
extern void test_derating_discharge_current_at_warn_is_warning_tier(void);
extern void test_derating_discharge_current_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_pack_undervoltage_above_warn_is_nominal(void);
extern void test_derating_pack_undervoltage_at_warn_is_warning_tier(void);
extern void test_derating_pack_undervoltage_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_pack_overvoltage_at_warn_is_warning_tier(void);
extern void test_derating_pack_overvoltage_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_ignores_cell_voltage_when_not_fresh(void);
extern void test_derating_cell_undervoltage_at_warn_is_warning_tier(void);
extern void test_derating_cell_undervoltage_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_cell_overvoltage_at_warn_is_warning_tier(void);
extern void test_derating_cell_overvoltage_at_approach_boundary_is_approaching_tier(void);
extern void test_derating_cell_voltage_realistic_nominal_is_nominal(void);
extern void test_derating_multiple_warnings_worst_case_wins(void);
extern void test_derating_two_warning_tier_signals_stay_at_warning_tier(void);

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

    // Faz 1 — sıcaklık eşikleri (sınır değerleri: 54/55/69/70)
    RUN_TEST(test_temp_below_warn_is_clean);
    RUN_TEST(test_temp_at_warn_threshold_is_warning);
    RUN_TEST(test_temp_below_crit_is_warning_only);
    RUN_TEST(test_temp_at_crit_threshold_is_critical);

    // Faz 1 — akım eşikleri uçtan uca + 9.9 A regresyon + pack voltajı
    RUN_TEST(test_nominal_charge_current_no_fault);
    RUN_TEST(test_charge_current_at_warn_is_warning_only);
    RUN_TEST(test_charge_current_at_crit_is_critical);
    RUN_TEST(test_discharge_current_at_crit_is_critical);

    RUN_TEST(test_warning_voltage_above_warn_low);
    RUN_TEST(test_warning_voltage_at_warn_low);
    RUN_TEST(test_critical_voltage_at_crit_low);
    RUN_TEST(test_warning_voltage_below_warn_high);
    RUN_TEST(test_warning_voltage_at_warn_high);
    RUN_TEST(test_critical_voltage_at_crit_high);

    RUN_TEST(test_cell_voltage_realistic_nominal_no_condition);
    RUN_TEST(test_cell_undervoltage_warn_threshold_deci_mv);
    RUN_TEST(test_cell_undervoltage_critical_realistic);
    RUN_TEST(test_cell_overvoltage_warn_threshold_deci_mv);
    RUN_TEST(test_cell_overvoltage_critical_realistic);

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
    RUN_TEST(test_reset_interlock_critical_temp_blocks);
    RUN_TEST(test_reset_interlock_warning_temp_does_not_block);
    RUN_TEST(test_reset_interlock_critical_voltage_low_blocks);
    RUN_TEST(test_reset_interlock_critical_voltage_high_blocks);
    RUN_TEST(test_reset_interlock_critical_current_blocks);
    RUN_TEST(test_reset_interlock_nominal_charge_current_does_not_block);
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
    RUN_TEST(test_idle_to_ready_with_charger_active_when_roles_unassigned);
    RUN_TEST(test_ready_to_drive_on_drive_enable);
    RUN_TEST(test_idle_to_emergency_stop);
    RUN_TEST(test_drive_to_emergency_stop);
    RUN_TEST(test_idle_to_fault_on_fault_event);
    RUN_TEST(test_ready_to_fault_on_fault_event);
    RUN_TEST(test_fault_pending_processed_when_queue_full);
    RUN_TEST(test_ready_to_fault_on_critical_telemetry);
    RUN_TEST(test_ready_to_fault_on_critical_temp);
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
    RUN_TEST(test_estop_zero_torque_reaches_can_queue_before_contactor_open);
    RUN_TEST(test_flag0_torque_frame_disabled);

    RUN_TEST(test_torque_queue_drain_empty_returns_false);
    RUN_TEST(test_torque_queue_push_then_drain_returns_value_once);
    RUN_TEST(test_torque_queue_zero_value_is_a_valid_pending_request);
    RUN_TEST(test_torque_queue_overwrites_undrained_value_with_latest);
    RUN_TEST(test_torque_queue_push_after_drain_is_pending_again);

    RUN_TEST(test_derating_neutral_when_bms_data_invalid);
    RUN_TEST(test_derating_nominal_when_all_signals_clean);
    RUN_TEST(test_derating_temp_just_below_warn_is_nominal);
    RUN_TEST(test_derating_temp_at_warn_threshold_is_warning_tier);
    RUN_TEST(test_derating_temp_just_below_approach_boundary_is_warning_tier);
    RUN_TEST(test_derating_temp_at_approach_boundary_is_approaching_critical_tier);
    RUN_TEST(test_derating_temp_at_critical_threshold_is_still_approaching_tier);
    RUN_TEST(test_derating_charge_current_below_warn_is_nominal);
    RUN_TEST(test_derating_charge_current_at_warn_is_warning_tier);
    RUN_TEST(test_derating_charge_current_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_discharge_current_below_warn_is_nominal);
    RUN_TEST(test_derating_discharge_current_at_warn_is_warning_tier);
    RUN_TEST(test_derating_discharge_current_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_pack_undervoltage_above_warn_is_nominal);
    RUN_TEST(test_derating_pack_undervoltage_at_warn_is_warning_tier);
    RUN_TEST(test_derating_pack_undervoltage_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_pack_overvoltage_at_warn_is_warning_tier);
    RUN_TEST(test_derating_pack_overvoltage_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_ignores_cell_voltage_when_not_fresh);
    RUN_TEST(test_derating_cell_undervoltage_at_warn_is_warning_tier);
    RUN_TEST(test_derating_cell_undervoltage_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_cell_overvoltage_at_warn_is_warning_tier);
    RUN_TEST(test_derating_cell_overvoltage_at_approach_boundary_is_approaching_tier);
    RUN_TEST(test_derating_cell_voltage_realistic_nominal_is_nominal);
    RUN_TEST(test_derating_multiple_warnings_worst_case_wins);
    RUN_TEST(test_derating_two_warning_tier_signals_stay_at_warning_tier);

    return UNITY_END();
}
