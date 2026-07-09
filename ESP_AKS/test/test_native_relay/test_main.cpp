#include <unity.h>

// Faz 6 — RelayManager bit logic + active-low + safe init order.

// set/get bit logic
extern void test_setRelay_channel0_on_drives_olata_low_bit(void);
extern void test_setRelay_channel0_off_restores_olata_high(void);
extern void test_setRelay_channel8_drives_olatb_low_bit(void);
extern void test_setRelay_channel9_drives_olatb_bit1_low(void);
extern void test_setRelay_out_of_range_does_not_write_spi(void);
extern void test_setRelay_before_begin_is_noop(void);
extern void test_getRelayState_reflects_setRelay(void);
extern void test_getRelayState_out_of_range_returns_false(void);
extern void test_setRelay_multiple_channels_accumulates_state(void);

// allOn / allOff
extern void test_allOn_drives_all_active_low(void);
extern void test_allOn_sets_all_state_bits(void);
extern void test_allOff_drives_all_high(void);
extern void test_allOff_clears_state(void);
extern void test_allOff_before_begin_clears_state_but_no_spi(void);
extern void test_allOn_before_begin_is_noop(void);

// init sequence (safety-critical)
extern void test_begin_writes_in_safe_order(void);
extern void test_begin_returns_true_on_success(void);
extern void test_begin_initializes_state_to_zero(void);

// G3 — geri-okuma / çıkış doğrulama (verifyOutputs)
extern void test_verify_readback_match_no_fault(void);
extern void test_verify_detects_olat_corruption_reasserts_and_faults(void);
extern void test_verify_detects_iodir_reset_to_input(void);
extern void test_clear_actuator_fault(void);
extern void test_verifyIfDue_throttles_to_period(void);
extern void test_readRegister_returns_written_values(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_setRelay_channel0_on_drives_olata_low_bit);
    RUN_TEST(test_setRelay_channel0_off_restores_olata_high);
    RUN_TEST(test_setRelay_channel8_drives_olatb_low_bit);
    RUN_TEST(test_setRelay_channel9_drives_olatb_bit1_low);
    RUN_TEST(test_setRelay_out_of_range_does_not_write_spi);
    RUN_TEST(test_setRelay_before_begin_is_noop);
    RUN_TEST(test_getRelayState_reflects_setRelay);
    RUN_TEST(test_getRelayState_out_of_range_returns_false);
    RUN_TEST(test_setRelay_multiple_channels_accumulates_state);

    RUN_TEST(test_allOn_drives_all_active_low);
    RUN_TEST(test_allOn_sets_all_state_bits);
    RUN_TEST(test_allOff_drives_all_high);
    RUN_TEST(test_allOff_clears_state);
    RUN_TEST(test_allOff_before_begin_clears_state_but_no_spi);
    RUN_TEST(test_allOn_before_begin_is_noop);

    RUN_TEST(test_begin_writes_in_safe_order);
    RUN_TEST(test_begin_returns_true_on_success);
    RUN_TEST(test_begin_initializes_state_to_zero);

    RUN_TEST(test_verify_readback_match_no_fault);
    RUN_TEST(test_verify_detects_olat_corruption_reasserts_and_faults);
    RUN_TEST(test_verify_detects_iodir_reset_to_input);
    RUN_TEST(test_clear_actuator_fault);
    RUN_TEST(test_verifyIfDue_throttles_to_period);
    RUN_TEST(test_readRegister_returns_written_values);

    return UNITY_END();
}
