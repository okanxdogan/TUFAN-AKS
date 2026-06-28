#include <unity.h>

// BmsSim — yalancı veri üreteci (SimCellDataSource) ve gerçek BMS stub'u
// (RealCellDataSource) için native testler.
// Tanımlar: test_scenario_normal.cpp, test_scenario_unbalanced.cpp,
// test_scenario_danger.cpp, test_determinism.cpp.

// Senaryo A (Normal)
extern void test_normal_all_cells_in_band(void);
extern void test_normal_band_holds_over_many_reads(void);
extern void test_normal_temp_and_current_reasonable(void);

// Senaryo B (Dengesiz)
extern void test_unbalanced_cell6_is_high(void);
extern void test_unbalanced_other_cells_nominal(void);
extern void test_unbalanced_spread_exceeds_50mv(void);

// Senaryo C (Tehlike)
extern void test_danger_undervoltage_has_low_cell(void);
extern void test_danger_undervoltage_others_not_low(void);
extern void test_danger_overtemp_has_hot_cell(void);
extern void test_danger_overtemp_voltages_remain_normal(void);

// Determinizm + HAL ayniligi
extern void test_same_seed_same_output(void);
extern void test_reset_replays_same_sequence(void);
extern void test_different_seed_changes_output(void);
extern void test_real_source_ingest_roundtrip(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Senaryo A
    RUN_TEST(test_normal_all_cells_in_band);
    RUN_TEST(test_normal_band_holds_over_many_reads);
    RUN_TEST(test_normal_temp_and_current_reasonable);

    // Senaryo B
    RUN_TEST(test_unbalanced_cell6_is_high);
    RUN_TEST(test_unbalanced_other_cells_nominal);
    RUN_TEST(test_unbalanced_spread_exceeds_50mv);

    // Senaryo C
    RUN_TEST(test_danger_undervoltage_has_low_cell);
    RUN_TEST(test_danger_undervoltage_others_not_low);
    RUN_TEST(test_danger_overtemp_has_hot_cell);
    RUN_TEST(test_danger_overtemp_voltages_remain_normal);

    // Determinizm + HAL
    RUN_TEST(test_same_seed_same_output);
    RUN_TEST(test_reset_replays_same_sequence);
    RUN_TEST(test_different_seed_changes_output);
    RUN_TEST(test_real_source_ingest_roundtrip);

    return UNITY_END();
}
