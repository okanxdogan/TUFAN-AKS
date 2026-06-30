#include <unity.h>

// Rol 2 — BmsAlgo: dengeleme, SoC, uyarı seviyesi ve Nextion paket üretimi.

// Dengeleme
extern void test_balance_uniform_no_flags(void);
extern void test_balance_single_high_cell_flagged(void);
extern void test_balance_delta_exactly_50_no_flag(void);
extern void test_balance_delta_51_flag_present(void);
extern void test_balance_top_margin_groups_cells(void);

// SoC
extern void test_soc_empty_is_zero(void);
extern void test_soc_full_is_hundred(void);
extern void test_soc_midpoint_3600mv(void);
extern void test_soc_quarter_3300mv(void);
extern void test_soc_below_empty_clamps_zero(void);
extern void test_soc_above_full_clamps_hundred(void);

// Uyarı seviyesi / geçersiz girdi
extern void test_warning_nominal_is_ok(void);
extern void test_warning_undervoltage_critical(void);
extern void test_warning_overtemp_critical(void);
extern void test_warning_undervoltage_warn_band(void);
extern void test_warning_overtemp_warn_band(void);
extern void test_warning_critical_overrides_warning(void);
extern void test_invalid_input_safe_critical(void);

// Nextion paket üretici
extern void test_packet_null_emit_is_noop(void);
extern void test_packet_command_count(void);
extern void test_packet_cell_voltage_command(void);
extern void test_packet_balance_flag_commands(void);
extern void test_packet_warning_text_command(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_balance_uniform_no_flags);
    RUN_TEST(test_balance_single_high_cell_flagged);
    RUN_TEST(test_balance_delta_exactly_50_no_flag);
    RUN_TEST(test_balance_delta_51_flag_present);
    RUN_TEST(test_balance_top_margin_groups_cells);

    RUN_TEST(test_soc_empty_is_zero);
    RUN_TEST(test_soc_full_is_hundred);
    RUN_TEST(test_soc_midpoint_3600mv);
    RUN_TEST(test_soc_quarter_3300mv);
    RUN_TEST(test_soc_below_empty_clamps_zero);
    RUN_TEST(test_soc_above_full_clamps_hundred);

    RUN_TEST(test_warning_nominal_is_ok);
    RUN_TEST(test_warning_undervoltage_critical);
    RUN_TEST(test_warning_overtemp_critical);
    RUN_TEST(test_warning_undervoltage_warn_band);
    RUN_TEST(test_warning_overtemp_warn_band);
    RUN_TEST(test_warning_critical_overrides_warning);
    RUN_TEST(test_invalid_input_safe_critical);

    RUN_TEST(test_packet_null_emit_is_noop);
    RUN_TEST(test_packet_command_count);
    RUN_TEST(test_packet_cell_voltage_command);
    RUN_TEST(test_packet_balance_flag_commands);
    RUN_TEST(test_packet_warning_text_command);

    return UNITY_END();
}
