#include <unity.h>

extern void test_never_received_heartbeat(void);
extern void test_within_timeout(void);
extern void test_past_timeout(void);
extern void test_exactly_at_timeout_is_not_down(void);
extern void test_link_up_then_down_then_up(void);
extern void test_boot_grace_not_down_within_grace_period(void);
extern void test_boot_grace_down_after_grace_expires_with_no_heartbeat(void);
extern void test_boot_grace_exactly_at_grace_is_not_down(void);
extern void test_boot_grace_uses_normal_timeout_once_heartbeat_seen(void);
extern void test_boot_grace_still_times_out_after_stale_heartbeat(void);
extern void test_boot_grace_relative_to_nonzero_boot_time(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_never_received_heartbeat);
    RUN_TEST(test_within_timeout);
    RUN_TEST(test_past_timeout);
    RUN_TEST(test_exactly_at_timeout_is_not_down);
    RUN_TEST(test_link_up_then_down_then_up);
    RUN_TEST(test_boot_grace_not_down_within_grace_period);
    RUN_TEST(test_boot_grace_down_after_grace_expires_with_no_heartbeat);
    RUN_TEST(test_boot_grace_exactly_at_grace_is_not_down);
    RUN_TEST(test_boot_grace_uses_normal_timeout_once_heartbeat_seen);
    RUN_TEST(test_boot_grace_still_times_out_after_stale_heartbeat);
    RUN_TEST(test_boot_grace_relative_to_nonzero_boot_time);

    return UNITY_END();
}
