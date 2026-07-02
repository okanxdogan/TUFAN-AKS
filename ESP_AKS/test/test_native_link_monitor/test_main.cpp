#include <unity.h>

extern void test_never_received_heartbeat(void);
extern void test_within_timeout(void);
extern void test_past_timeout(void);
extern void test_exactly_at_timeout_is_not_down(void);
extern void test_link_up_then_down_then_up(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_never_received_heartbeat);
    RUN_TEST(test_within_timeout);
    RUN_TEST(test_past_timeout);
    RUN_TEST(test_exactly_at_timeout_is_not_down);
    RUN_TEST(test_link_up_then_down_then_up);

    return UNITY_END();
}
