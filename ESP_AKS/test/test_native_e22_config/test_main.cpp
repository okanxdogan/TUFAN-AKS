#include <unity.h>

extern void test_contract_regs_match_ortak_blok(void);
extern void test_build_read_all_command(void);
extern void test_build_read_all_command_buffer_too_small(void);
extern void test_build_write_command(void);
extern void test_build_write_command_buffer_too_small(void);
extern void test_parse_valid_response_accepted(void);
extern void test_is_error_response_detects_ff_ff_ff(void);
extern void test_is_error_response_false_for_valid(void);
extern void test_parse_rejects_ff_ff_ff_error_frame(void);
extern void test_parse_rejects_wrong_header_byte(void);
extern void test_parse_rejects_wrong_start_addr(void);
extern void test_parse_rejects_wrong_len_field(void);
extern void test_parse_rejects_truncated_response(void);
extern void test_parse_rejects_null_buffer(void);
extern void test_crypt_bytes_excluded_from_comparison(void);
extern void test_regs_equal_detects_real_difference(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_contract_regs_match_ortak_blok);
    RUN_TEST(test_build_read_all_command);
    RUN_TEST(test_build_read_all_command_buffer_too_small);
    RUN_TEST(test_build_write_command);
    RUN_TEST(test_build_write_command_buffer_too_small);
    RUN_TEST(test_parse_valid_response_accepted);
    RUN_TEST(test_is_error_response_detects_ff_ff_ff);
    RUN_TEST(test_is_error_response_false_for_valid);
    RUN_TEST(test_parse_rejects_ff_ff_ff_error_frame);
    RUN_TEST(test_parse_rejects_wrong_header_byte);
    RUN_TEST(test_parse_rejects_wrong_start_addr);
    RUN_TEST(test_parse_rejects_wrong_len_field);
    RUN_TEST(test_parse_rejects_truncated_response);
    RUN_TEST(test_parse_rejects_null_buffer);
    RUN_TEST(test_crypt_bytes_excluded_from_comparison);
    RUN_TEST(test_regs_equal_detects_real_difference);

    return UNITY_END();
}
