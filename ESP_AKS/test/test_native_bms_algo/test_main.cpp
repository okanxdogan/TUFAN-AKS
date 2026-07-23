#include <unity.h>

#include "fake_nextion_emit.h"

// SoC (State of Charge) haritalaması — LiFePO4 sabitleri (2500/3650 mV)
extern void test_soc_at_empty_is_zero_percent(void);
extern void test_soc_at_full_is_hundred_percent(void);
extern void test_soc_at_midpoint_is_fifty_percent(void);
extern void test_soc_below_empty_clamps_to_zero(void);
extern void test_soc_above_full_clamps_to_hundred(void);

// Uyarı seviyesi sınırları — hücre voltajı: LiFePO4 eşikleri, strictly </>
// semantiği; sıcaklık: 55/70 °C, >= semantiği (VCU politikasıyla eş anlı)
extern void test_overvolt_at_crit_threshold_is_warning_not_critical(void);
extern void test_overvolt_above_crit_threshold_is_critical(void);
extern void test_overvolt_at_warn_threshold_is_ok(void);
extern void test_overvolt_above_warn_threshold_is_warning(void);
extern void test_undervolt_at_crit_threshold_is_warning_not_critical(void);
extern void test_undervolt_below_crit_threshold_is_critical(void);
extern void test_undervolt_at_warn_threshold_is_ok(void);
extern void test_undervolt_below_warn_threshold_is_warning(void);
extern void test_overtemp_below_warn_threshold_is_ok(void);
extern void test_overtemp_at_warn_threshold_is_warning(void);
extern void test_overtemp_below_crit_threshold_is_warning(void);
extern void test_overtemp_at_crit_threshold_is_critical(void);

// Dengeleme (balance) regresyon kilidi — kimyadan bağımsız, DEĞİŞMEDİ
extern void test_balance_at_threshold_no_balancing(void);
extern void test_balance_above_threshold_triggers_balancing(void);

// cellBarFill — buildBmsNextionCommands üzerinden dolaylı test
extern void test_cell_bar_fill_at_empty_is_zero(void);
extern void test_cell_bar_fill_at_full_is_hundred(void);
extern void test_cell_bar_fill_at_midpoint_is_fifty(void);

extern void test_cache_init_emits_on_first_nonforced_call(void);
extern void test_byte_budget_is_respected(void);
extern void test_valid_to_invalid_data_transition(void);

// BMS panel round-robin resync — slot invalidasyonu (BmsNextionPacket.h)
extern void test_resync_invalidate_cell_slot_reemits_triple(void);
extern void test_resync_invalidate_summary_slots_reemit(void);
extern void test_resync_invalidation_survives_budget_exhaustion(void);
extern void test_resync_full_rotation_covers_entire_panel(void);

// G8/M4 — hücre kaynağı doğrulama kapısı (cellDataValid)
extern void test_unverified_cell_source_returns_no_data(void);
extern void test_unverified_imbalance_is_not_masked_as_healthy_nor_critical(void);
extern void test_verified_cell_source_detects_imbalance(void);
extern void test_cell_data_valid_defaults_true(void);
extern void test_unverified_pipeline_emits_sentinels_and_no_data_warn(void);

// Ekran ÖZET min/max kaynağı = BYS raporu (0xE001), 24'lük tarama fallback
extern void test_reported_minmax_overrides_scan_summary(void);
extern void test_reported_minmax_keeps_scan_indices(void);
extern void test_reported_minmax_does_not_alter_balancing(void);
extern void test_zero_reported_falls_back_to_scan(void);
extern void test_partial_zero_reported_falls_back_to_scan(void);
extern void test_no_cell_data_but_reported_fills_summary_only(void);
extern void test_no_cell_data_and_zero_reported_stays_zero(void);
extern void test_invalid_pack_ignores_reported_minmax(void);

void setUp(void) {
    fake_nextion_reset();
}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_soc_at_empty_is_zero_percent);
    RUN_TEST(test_soc_at_full_is_hundred_percent);
    RUN_TEST(test_soc_at_midpoint_is_fifty_percent);
    RUN_TEST(test_soc_below_empty_clamps_to_zero);
    RUN_TEST(test_soc_above_full_clamps_to_hundred);

    RUN_TEST(test_overvolt_at_crit_threshold_is_warning_not_critical);
    RUN_TEST(test_overvolt_above_crit_threshold_is_critical);
    RUN_TEST(test_overvolt_at_warn_threshold_is_ok);
    RUN_TEST(test_overvolt_above_warn_threshold_is_warning);
    RUN_TEST(test_undervolt_at_crit_threshold_is_warning_not_critical);
    RUN_TEST(test_undervolt_below_crit_threshold_is_critical);
    RUN_TEST(test_undervolt_at_warn_threshold_is_ok);
    RUN_TEST(test_undervolt_below_warn_threshold_is_warning);
    RUN_TEST(test_overtemp_below_warn_threshold_is_ok);
    RUN_TEST(test_overtemp_at_warn_threshold_is_warning);
    RUN_TEST(test_overtemp_below_crit_threshold_is_warning);
    RUN_TEST(test_overtemp_at_crit_threshold_is_critical);

    RUN_TEST(test_cell_bar_fill_at_empty_is_zero);
    RUN_TEST(test_cell_bar_fill_at_full_is_hundred);
    RUN_TEST(test_cell_bar_fill_at_midpoint_is_fifty);

    RUN_TEST(test_cache_init_emits_on_first_nonforced_call);
    RUN_TEST(test_byte_budget_is_respected);
    RUN_TEST(test_valid_to_invalid_data_transition);

    RUN_TEST(test_resync_invalidate_cell_slot_reemits_triple);
    RUN_TEST(test_resync_invalidate_summary_slots_reemit);
    RUN_TEST(test_resync_invalidation_survives_budget_exhaustion);
    RUN_TEST(test_resync_full_rotation_covers_entire_panel);

    RUN_TEST(test_unverified_cell_source_returns_no_data);
    RUN_TEST(test_unverified_imbalance_is_not_masked_as_healthy_nor_critical);
    RUN_TEST(test_verified_cell_source_detects_imbalance);
    RUN_TEST(test_cell_data_valid_defaults_true);
    RUN_TEST(test_unverified_pipeline_emits_sentinels_and_no_data_warn);

    RUN_TEST(test_reported_minmax_overrides_scan_summary);
    RUN_TEST(test_reported_minmax_keeps_scan_indices);
    RUN_TEST(test_reported_minmax_does_not_alter_balancing);
    RUN_TEST(test_zero_reported_falls_back_to_scan);
    RUN_TEST(test_partial_zero_reported_falls_back_to_scan);
    RUN_TEST(test_no_cell_data_but_reported_fills_summary_only);
    RUN_TEST(test_no_cell_data_and_zero_reported_stays_zero);
    RUN_TEST(test_invalid_pack_ignores_reported_minmax);

    return UNITY_END();
}
