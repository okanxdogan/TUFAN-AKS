#include <unity.h>
#include "CanParse.h"
#include <cstring>

using namespace CanParse;

void test_e015_dlc_too_short(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E015;
    msg.data_length_code = 7;
    TelemetryData out = {};

    bool success = parseLbBmsE015(msg, out);

    TEST_ASSERT_FALSE(success);
}

void test_e015_parses_four_cells_correctly(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E015;
    msg.data_length_code = 8;
    // data=83 1B 83 1B 83 0E 83 2B → hücre[0]=3355, [1]=3355, [2]=3342, [3]=3371 mV
    msg.data[0] = 0x83; msg.data[1] = 0x1B;
    msg.data[2] = 0x83; msg.data[3] = 0x1B;
    msg.data[4] = 0x83; msg.data[5] = 0x0E;
    msg.data[6] = 0x83; msg.data[7] = 0x2B;

    TelemetryData out = {};
    bool success = parseLbBmsE015(msg, out);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT16(3356, out.TEL_bmsCellVoltages[0]); // 33563 -> yuvarla 3356
    TEST_ASSERT_EQUAL_UINT16(3356, out.TEL_bmsCellVoltages[1]); // 33563 -> yuvarla 3356
    TEST_ASSERT_EQUAL_UINT16(3355, out.TEL_bmsCellVoltages[2]); // 33550 -> yuvarla 3355
    TEST_ASSERT_EQUAL_UINT16(3358, out.TEL_bmsCellVoltages[3]); // 33579 -> yuvarla 3358 (kesme 3357 idi)
}

void test_e015_cells_in_lifepo4_range(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E015;
    msg.data_length_code = 8;
    // Let's test min and max typical range: 2500mV to 3650mV
    // 25000 = 0x61A8, 36500 = 0x8E94
    msg.data[0] = 0x61; msg.data[1] = 0xA8;
    msg.data[2] = 0x8E; msg.data[3] = 0x94;
    msg.data[4] = 0x75; msg.data[5] = 0x30; // 30000 = 0x7530 (3000mV)
    msg.data[6] = 0x7D; msg.data[7] = 0x00; // 32000 = 0x7D00 (3200mV)

    TelemetryData out = {};
    bool success = parseLbBmsE015(msg, out);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT16(2500, out.TEL_bmsCellVoltages[0]);
    TEST_ASSERT_EQUAL_UINT16(3650, out.TEL_bmsCellVoltages[1]);
    TEST_ASSERT_EQUAL_UINT16(3000, out.TEL_bmsCellVoltages[2]);
    TEST_ASSERT_EQUAL_UINT16(3200, out.TEL_bmsCellVoltages[3]);
}

void test_e020_parses_four_cells_correctly(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E020;
    msg.data_length_code = 8;
    // 20-23
    msg.data[0] = 0x61; msg.data[1] = 0xA8;
    msg.data[2] = 0x8E; msg.data[3] = 0x94;
    msg.data[4] = 0x75; msg.data[5] = 0x30; 
    msg.data[6] = 0x7D; msg.data[7] = 0x00;

    TelemetryData out = {};
    bool success = parseLbBmsE020(msg, out);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT16(2500, out.TEL_bmsCellVoltages[20]);
    TEST_ASSERT_EQUAL_UINT16(3650, out.TEL_bmsCellVoltages[21]);
    TEST_ASSERT_EQUAL_UINT16(3000, out.TEL_bmsCellVoltages[22]);
    TEST_ASSERT_EQUAL_UINT16(3200, out.TEL_bmsCellVoltages[23]);
}

void test_e001_parses_min_max_avg_cell_voltages(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E001;
    msg.data_length_code = 8;
    // data=82 E1 86 07 83 A0 1C 1C
    msg.data[0] = 0x82; msg.data[1] = 0xE1; // min: 33505 -> 3350
    msg.data[2] = 0x86; msg.data[3] = 0x07; // max: 34311 -> 3431
    msg.data[4] = 0x83; msg.data[5] = 0xA0; // avg: 33696 -> 3369
    msg.data[6] = 0x1C; msg.data[7] = 0x1C; // temp: 28, 28

    TelemetryData out = {};
    bool success = parseLbBmsE001(msg, out);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT16(33505, out.TEL_bmsCellVoltageMinDeciMv);
    TEST_ASSERT_EQUAL_UINT16(34311, out.TEL_bmsCellVoltageMaxDeciMv);
    TEST_ASSERT_EQUAL_UINT16(33696, out.TEL_bmsCellVoltageAvgDeciMv);
    TEST_ASSERT_EQUAL_INT8(28, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(28, out.TEL_bmsTempLowestC);
}

void test_e001_preserves_temperature_fields(void) {
    twai_message_t msg = {};
    msg.identifier = 0x0000E001;
    msg.data_length_code = 8;
    // 00 00 00 00 00 00 FF 10 (Temp1: -1, Temp2: 16)
    msg.data[0] = 0; msg.data[1] = 0;
    msg.data[2] = 0; msg.data[3] = 0;
    msg.data[4] = 0; msg.data[5] = 0;
    msg.data[6] = 0xFF; msg.data[7] = 0x10;

    TelemetryData out = {};
    bool success = parseLbBmsE001(msg, out);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT8(16, out.TEL_bmsTempHighestC);
    TEST_ASSERT_EQUAL_INT8(-1, out.TEL_bmsTempLowestC);
}
