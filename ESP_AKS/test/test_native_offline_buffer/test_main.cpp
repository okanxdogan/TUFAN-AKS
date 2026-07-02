#include <unity.h>

extern void test_push_pop_fifo(void);
extern void test_capacity_drop_oldest(void);
extern void test_empty_pop(void);
extern void test_reset(void);
extern void test_link_down_up_scenario(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_push_pop_fifo);
    RUN_TEST(test_capacity_drop_oldest);
    RUN_TEST(test_empty_pop);
    RUN_TEST(test_reset);
    RUN_TEST(test_link_down_up_scenario);

    return UNITY_END();
}
