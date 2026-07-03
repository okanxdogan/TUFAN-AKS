#include <unity.h>

extern void test_push_pop_fifo(void);
extern void test_capacity_drop_oldest(void);
extern void test_empty_pop(void);
extern void test_reset(void);
extern void test_link_down_up_scenario(void);
extern void test_peek_does_not_remove(void);
extern void test_drop_front_removes_front(void);
extern void test_peek_without_drop_keeps_packet_for_retry(void);
extern void test_empty_peek(void);
extern void test_reset_clears_peek(void);
extern void test_capacity_is_75(void);
extern void test_offline_should_sample_first_sample_is_immediate(void);
extern void test_offline_should_sample_within_period_is_false(void);
extern void test_offline_should_sample_at_period_is_true(void);
extern void test_offline_should_sample_past_period_is_true(void);
extern void test_1hz_sampling_over_10s_outage_yields_about_10_packets(void);
extern void test_60s_outage_simulation_stays_within_capacity(void);
extern void test_replay_throttle_drains_60_packets_within_expected_ticks(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_push_pop_fifo);
    RUN_TEST(test_capacity_drop_oldest);
    RUN_TEST(test_empty_pop);
    RUN_TEST(test_reset);
    RUN_TEST(test_link_down_up_scenario);
    RUN_TEST(test_peek_does_not_remove);
    RUN_TEST(test_drop_front_removes_front);
    RUN_TEST(test_peek_without_drop_keeps_packet_for_retry);
    RUN_TEST(test_empty_peek);
    RUN_TEST(test_reset_clears_peek);
    RUN_TEST(test_capacity_is_75);
    RUN_TEST(test_offline_should_sample_first_sample_is_immediate);
    RUN_TEST(test_offline_should_sample_within_period_is_false);
    RUN_TEST(test_offline_should_sample_at_period_is_true);
    RUN_TEST(test_offline_should_sample_past_period_is_true);
    RUN_TEST(test_1hz_sampling_over_10s_outage_yields_about_10_packets);
    RUN_TEST(test_60s_outage_simulation_stays_within_capacity);
    RUN_TEST(test_replay_throttle_drains_60_packets_within_expected_ticks);

    return UNITY_END();
}
