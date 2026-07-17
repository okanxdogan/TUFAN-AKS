#include <unity.h>
#include "E22Config.h"

// ---------------------------------------------------------------------------
// Sözleşme değerleri — ORTAK BLOK ile birebir (regresyon kilidi)
// ---------------------------------------------------------------------------
void test_contract_regs_match_ortak_blok(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, E22_CONTRACT_REGS.addh);
    TEST_ASSERT_EQUAL_HEX8(0x00, E22_CONTRACT_REGS.addl);
    TEST_ASSERT_EQUAL_HEX8(0x00, E22_CONTRACT_REGS.netid);
    TEST_ASSERT_EQUAL_HEX8(0x63, E22_CONTRACT_REGS.reg0);
    TEST_ASSERT_EQUAL_HEX8(0x00, E22_CONTRACT_REGS.reg1);
    TEST_ASSERT_EQUAL_HEX8(0x17, E22_CONTRACT_REGS.reg2);
    TEST_ASSERT_EQUAL_HEX8(0x00, E22_CONTRACT_REGS.reg3);
}

// ---------------------------------------------------------------------------
// Komut inşası
// ---------------------------------------------------------------------------
void test_build_read_all_command(void) {
    uint8_t buf[3] = {};
    const size_t len = e22_buildReadAllCommand(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(3u, len);
    TEST_ASSERT_EQUAL_HEX8(0xC1, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x09, buf[2]);
}

void test_build_read_all_command_buffer_too_small(void) {
    uint8_t buf[2] = {};
    TEST_ASSERT_EQUAL_UINT(0u, e22_buildReadAllCommand(buf, sizeof(buf)));
}

void test_build_write_command(void) {
    uint8_t buf[12] = {};
    const size_t len = e22_buildWriteCommand(E22_CONTRACT_REGS, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(12u, len);
    TEST_ASSERT_EQUAL_HEX8(0xC0, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);  // start addr
    TEST_ASSERT_EQUAL_HEX8(0x09, buf[2]);  // len
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);  // ADDH
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[4]);  // ADDL
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[5]);  // NETID
    TEST_ASSERT_EQUAL_HEX8(0x63, buf[6]);  // REG0
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[7]);  // REG1
    TEST_ASSERT_EQUAL_HEX8(0x17, buf[8]);  // REG2
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[9]);  // REG3
    TEST_ASSERT_EQUAL_HEX8(0x5A, buf[10]); // CRYPT_H (UKS ile aynı: E22_CRYPT_KEY=0x5A3C, G7)
    TEST_ASSERT_EQUAL_HEX8(0x3C, buf[11]); // CRYPT_L
}

void test_build_write_command_buffer_too_small(void) {
    uint8_t buf[11] = {};
    TEST_ASSERT_EQUAL_UINT(0u, e22_buildWriteCommand(E22_CONTRACT_REGS, buf, sizeof(buf)));
}

// ---------------------------------------------------------------------------
// G7-FIX-2: CRYPT'in gecici (0xC2/RAM) yazma komutu — read-before-write skip
// yolunda ADDH..REG3 zaten hedefle ayniyken CRYPT'i her boot'ta tazelemek
// icin kullanilir (bkz. LoraLink::configureE22, needsWrite=false dali).
// ---------------------------------------------------------------------------
void test_build_write_crypt_temp_command(void) {
    uint8_t buf[5] = {};
    const size_t len = e22_buildWriteCryptTempCommand(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(5u, len);
    TEST_ASSERT_EQUAL_HEX8(0xC2, buf[0]);  // E22_CMD_WRITE_TEMP (kalici DEGIL)
    TEST_ASSERT_EQUAL_HEX8(0x07, buf[1]);  // E22_REG_ADDR_CRYPT_H
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[2]);  // len (CRYPT_H + CRYPT_L)
    TEST_ASSERT_EQUAL_HEX8(0x5A, buf[3]);  // CRYPT_H (E22_CRYPT_KEY=0x5A3C, G7)
    TEST_ASSERT_EQUAL_HEX8(0x3C, buf[4]);  // CRYPT_L
}

void test_build_write_crypt_temp_command_buffer_too_small(void) {
    uint8_t buf[4] = {};
    TEST_ASSERT_EQUAL_UINT(0u, e22_buildWriteCryptTempCommand(buf, sizeof(buf)));
}

// ---------------------------------------------------------------------------
// Yanıt doğrulama — kabul senaryosu
// ---------------------------------------------------------------------------
void test_parse_valid_response_accepted(void) {
    const uint8_t resp[12] = {
        0xC1, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,  // ADDH..REG3
        0xAB, 0xCD,  // CRYPT_H/L — herhangi bir değer olabilir
    };
    E22RegValues out = {};
    TEST_ASSERT_TRUE(e22_parseRegResponse(resp, sizeof(resp), out));
    TEST_ASSERT_TRUE(e22_regsEqual(out, E22_CONTRACT_REGS));
}

// ---------------------------------------------------------------------------
// FF FF FF hata çerçevesi → red
// ---------------------------------------------------------------------------
void test_is_error_response_detects_ff_ff_ff(void) {
    const uint8_t resp[3] = {0xFF, 0xFF, 0xFF};
    TEST_ASSERT_TRUE(e22_isErrorResponse(resp, sizeof(resp)));
}

void test_is_error_response_false_for_valid(void) {
    const uint8_t resp[12] = {
        0xC1, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x00, 0x00,
    };
    TEST_ASSERT_FALSE(e22_isErrorResponse(resp, sizeof(resp)));
}

void test_parse_rejects_ff_ff_ff_error_frame(void) {
    const uint8_t resp[12] = {
        0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(resp, sizeof(resp), out));
}

// ---------------------------------------------------------------------------
// Eksik / yanlış başlık → red
// ---------------------------------------------------------------------------
void test_parse_rejects_wrong_header_byte(void) {
    const uint8_t resp[12] = {
        0xC0, 0x00, 0x09,  // yazma başlığı, okuma yanıtında beklenmez
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x00, 0x00,
    };
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(resp, sizeof(resp), out));
}

void test_parse_rejects_wrong_start_addr(void) {
    const uint8_t resp[12] = {
        0xC1, 0x01, 0x09,  // yanlış başlangıç adresi
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x00, 0x00,
    };
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(resp, sizeof(resp), out));
}

void test_parse_rejects_wrong_len_field(void) {
    const uint8_t resp[12] = {
        0xC1, 0x00, 0x05,  // yanlış uzunluk alanı
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x00, 0x00,
    };
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(resp, sizeof(resp), out));
}

void test_parse_rejects_truncated_response(void) {
    const uint8_t resp[6] = {0xC1, 0x00, 0x09, 0x00, 0x00, 0x00};  // 12 byte bekleniyor
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(resp, sizeof(resp), out));
}

void test_parse_rejects_null_buffer(void) {
    E22RegValues out = {};
    TEST_ASSERT_FALSE(e22_parseRegResponse(nullptr, 12, out));
}

// ---------------------------------------------------------------------------
// CRYPT alanı karşılaştırma dışı — iki yanıt yalnızca CRYPT byte'larında
// farklı olsa bile parse edilen register kümeleri EŞİT sayılmalı.
// ---------------------------------------------------------------------------
void test_crypt_bytes_excluded_from_comparison(void) {
    const uint8_t respA[12] = {
        0xC1, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x11, 0x22,  // CRYPT set A
    };
    const uint8_t respB[12] = {
        0xC1, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x63, 0x00, 0x17, 0x00,
        0x99, 0xEE,  // CRYPT set B — farklı
    };
    E22RegValues outA = {}, outB = {};
    TEST_ASSERT_TRUE(e22_parseRegResponse(respA, sizeof(respA), outA));
    TEST_ASSERT_TRUE(e22_parseRegResponse(respB, sizeof(respB), outB));
    TEST_ASSERT_TRUE(e22_regsEqual(outA, outB));
}

// ---------------------------------------------------------------------------
// Gerçek bir fark (CRYPT dışı bir alanda) tespit edilmeli
// ---------------------------------------------------------------------------
void test_regs_equal_detects_real_difference(void) {
    E22RegValues a = E22_CONTRACT_REGS;
    E22RegValues b = E22_CONTRACT_REGS;
    b.reg2 = 0x01;  // farklı kanal
    TEST_ASSERT_FALSE(e22_regsEqual(a, b));
}
