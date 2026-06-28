#include <unity.h>

// Rol 3 — BMS gösterge EDGE CASE test seti.
// SADECE BmsModel.h (BmsPackData) + test-local doğrulayıcılara (bms_edge_helpers.h)
// bağımlıdır. BmsSim/BmsAlgo modüllerine DERLEME bağımlılığı YOKTUR; testler
// paylaşılan sözleşmeyi (BmsPackData) doğrular. Tanımlar:
//   test_sensor_fault.cpp, test_cell_extremes.cpp,
//   test_temp_current.cpp, test_pack_voltage_overflow.cpp

// Sensör arızası / geçersiz veri
extern void test_all_cells_zero_detected(void);
extern void test_all_cells_zero_but_marked_valid_is_implausible(void);
extern void test_nominal_pack_not_all_zero(void);
extern void test_invalid_pack_not_consumable(void);
extern void test_valid_pack_is_consumable(void);
extern void test_invalid_flag_independent_of_content(void);

// Hücre uç değerleri / indeks sınırları
extern void test_single_cell_overvolt_delta(void);
extern void test_cell_at_absolute_max_4200(void);
extern void test_overvolt_cell_above_plausible(void);
extern void test_single_cell_undervolt_2500(void);
extern void test_cell_below_undervolt_threshold(void);
extern void test_max_cell_at_index_0(void);
extern void test_max_cell_at_index_23(void);
extern void test_min_cell_at_index_0(void);
extern void test_min_cell_at_index_23(void);
extern void test_uniform_pack_min_max_first_index_tie(void);

// Sıcaklık / akım uç değerleri
extern void test_overtemp_detected(void);
extern void test_temp_at_threshold_not_overtemp(void);
extern void test_overtemp_at_index_23(void);
extern void test_undertemp_negative_detected(void);
extern void test_temp_at_lower_threshold_not_undertemp(void);
extern void test_int16_temp_min_extreme(void);
extern void test_int16_temp_max_extreme(void);
extern void test_current_max_int32_stored(void);
extern void test_current_min_int32_stored(void);
extern void test_current_sign_discharge_negative(void);
extern void test_current_zero_is_idle(void);

// KRİTİK — paket gerilimi taşması
extern void test_pack_sum_exceeds_uint16_range(void);
extern void test_pack_sum_uint16_wraps_around(void);
extern void test_pack_sum_overflow_threshold_boundary(void);
extern void test_pack_sum_nominal_fits_but_near_limit(void);
extern void test_pack_sum_deciv_representation_fits_uint16(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Sensör arızası / geçersiz veri
    RUN_TEST(test_all_cells_zero_detected);
    RUN_TEST(test_all_cells_zero_but_marked_valid_is_implausible);
    RUN_TEST(test_nominal_pack_not_all_zero);
    RUN_TEST(test_invalid_pack_not_consumable);
    RUN_TEST(test_valid_pack_is_consumable);
    RUN_TEST(test_invalid_flag_independent_of_content);

    // Hücre uç değerleri / indeks sınırları
    RUN_TEST(test_single_cell_overvolt_delta);
    RUN_TEST(test_cell_at_absolute_max_4200);
    RUN_TEST(test_overvolt_cell_above_plausible);
    RUN_TEST(test_single_cell_undervolt_2500);
    RUN_TEST(test_cell_below_undervolt_threshold);
    RUN_TEST(test_max_cell_at_index_0);
    RUN_TEST(test_max_cell_at_index_23);
    RUN_TEST(test_min_cell_at_index_0);
    RUN_TEST(test_min_cell_at_index_23);
    RUN_TEST(test_uniform_pack_min_max_first_index_tie);

    // Sıcaklık / akım uç değerleri
    RUN_TEST(test_overtemp_detected);
    RUN_TEST(test_temp_at_threshold_not_overtemp);
    RUN_TEST(test_overtemp_at_index_23);
    RUN_TEST(test_undertemp_negative_detected);
    RUN_TEST(test_temp_at_lower_threshold_not_undertemp);
    RUN_TEST(test_int16_temp_min_extreme);
    RUN_TEST(test_int16_temp_max_extreme);
    RUN_TEST(test_current_max_int32_stored);
    RUN_TEST(test_current_min_int32_stored);
    RUN_TEST(test_current_sign_discharge_negative);
    RUN_TEST(test_current_zero_is_idle);

    // KRİTİK — paket gerilimi taşması
    RUN_TEST(test_pack_sum_exceeds_uint16_range);
    RUN_TEST(test_pack_sum_uint16_wraps_around);
    RUN_TEST(test_pack_sum_overflow_threshold_boundary);
    RUN_TEST(test_pack_sum_nominal_fits_but_near_limit);
    RUN_TEST(test_pack_sum_deciv_representation_fits_uint16);

    return UNITY_END();
}
