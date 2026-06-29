#include <unity.h>

#include <cstring>
#include <string>

#include "BmsAlgo.h"
#include "BmsNextionPacket.h"
#include "bms_test_fixtures.h"

using bms_fixtures::makeUniformPack;

namespace {

// emit callback'inin komutları topladığı basit toplayıcı. Her komut ';' ile
// ayrılarak tek bir string'e biriktirilir (strstr ile aranabilir).
struct CmdCollector {
    std::string all;
    int count = 0;
};

void collect(const char* cmd, size_t len, void* ctx) {
    auto* c = static_cast<CmdCollector*>(ctx);
    c->all.append(cmd, len);
    c->all.push_back(';');
    c->count++;
}

}  // namespace

// nullptr emit => çökme yok, hiçbir şey üretilmez (no-op).
void test_packet_null_emit_is_noop(void) {
    BmsComputed c = computePack(makeUniformPack(3700));
    buildBmsNextionCommands(c, makeUniformPack(3700), nullptr, nullptr);
    TEST_PASS();  // çökmediği için geçer
}

// Komut sayısı: 24 cell + 24 j (bar) + 24 bal + 7 özet (delta,soc,bmspackv,
// cellmax,cellmin,tmax,tmin) + 1 warn + 1 warntxt = 81.
void test_packet_command_count(void) {
    BmsPackData p = makeUniformPack(3700);
    BmsComputed c = computePack(p);
    CmdCollector col;
    buildBmsNextionCommands(c, p, collect, &col);
    TEST_ASSERT_EQUAL_INT(81, col.count);
}

// Bilinen hücre gerilimi doğru cellN.val komutuna dönüşmeli.
void test_packet_cell_voltage_command(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[5] = 3812;
    BmsComputed c = computePack(p);
    CmdCollector col;
    buildBmsNextionCommands(c, p, collect, &col);

    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "cell5.val=3812;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "cell0.val=3700;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "cell23.val=3700;"));

    // Bar doluluğu: 3812 mV -> (812*100)/1200 = 67; 3700 mV -> 58.
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "j5.val=67;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "j0.val=58;"));
}

// SoC ve delta özet komutları beklenen değerlerle üretilmeli.
// Uniform 3600 mV => soc 50, delta 0.
void test_packet_soc_and_delta_commands(void) {
    BmsPackData p = makeUniformPack(3600);
    BmsComputed c = computePack(p);
    CmdCollector col;
    buildBmsNextionCommands(c, p, collect, &col);

    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "soc.val=50;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "delta.val=0;"));
}

// Dengeleme bayrağı balN.val=1/0 olarak çıkmalı.
void test_packet_balance_flag_commands(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[2] = 3800;  // delta 100 > 50 => cell2 dengelenir
    BmsComputed c = computePack(p);
    CmdCollector col;
    buildBmsNextionCommands(c, p, collect, &col);

    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "bal2.val=1;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "bal0.val=0;"));
}

// Uyarı metni: warntxt.txt="CRIT" ve warn.val=2 (undervoltage senaryosu).
void test_packet_warning_text_command(void) {
    BmsPackData p = makeUniformPack(3700);
    p.cellVoltageMv[0] = 2900;  // CRITICAL
    BmsComputed c = computePack(p);
    CmdCollector col;
    buildBmsNextionCommands(c, p, collect, &col);

    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "warn.val=2;"));
    TEST_ASSERT_NOT_NULL(strstr(col.all.c_str(), "warntxt.txt=\"CRIT\";"));
}

// bmsWarningText haritalaması.
void test_packet_warning_text_mapping(void) {
    TEST_ASSERT_EQUAL_STRING("OK", bmsWarningText(BMS_WARN_OK));
    TEST_ASSERT_EQUAL_STRING("WARN", bmsWarningText(BMS_WARN_WARNING));
    TEST_ASSERT_EQUAL_STRING("CRIT", bmsWarningText(BMS_WARN_CRITICAL));
    TEST_ASSERT_EQUAL_STRING("UNK", bmsWarningText(99));
}
