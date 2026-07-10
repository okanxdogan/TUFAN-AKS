// can_replay — kaydedilmiş CAN sniffer oturumunu GERÇEK saf parser'lardan
// (lib/CanParse) geçirip invariant'ları doğrulayan offline regresyon aracı.
//
// Donanım yok: twai_message_t, test/support/idf_stubs'taki stub struct'tır;
// CanParse.cpp gerçek üretim kodu olarak linklenir. Böylece 8 dakikalık
// gerçek oturumun tamamı parse edildiğinde parser'ların çökmediği ve decode
// kurallarının tutarlı kaldığı garanti edilir.
//
// Satır formatı (boş satırlar ve '#' yorumları atlanır):
//   EXT | 0xE000     | 8 | FF FF 03 16 0F 5E 09 71
//   STD | 0x000      | 8 | 00 00 00 00 00 00 00 00
//
// Kullanım: bkz. README.md (build.sh ile derle, log yolunu argüman ver).
// Çıkış kodu: 0 = tüm invariant'lar geçti, 1 = ihlal/parse hatası var.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "CanParse.h"

namespace {

struct Frame {
    bool isExt = false;
    uint32_t id = 0;
    twai_message_t msg = {};
    int lineNo = 0;
};

struct Stats {
    long total = 0;
    std::map<std::string, long> countsById;  // "EXT 0x0000E000" -> adet
    std::set<std::string> unknownIds;
    std::vector<std::string> violations;
    long violationCount = 0;

    void addViolation(const std::string& v) {
        violationCount++;
        if (violations.size() < 20)  // rapor ilk 20 ihlali gösterir
            violations.push_back(v);
    }
};

// Bu idle oturum için beklenen sabitler (Ek A / CAN_Message_Table.md).
// --generic bayrağı ile kapatılır; başka oturumlar için yalnız
// "çökmezlik + bilinen ID" kontrolleri kalır.
constexpr uint16_t kExpectedPackVDeciV = 790;   // 79.0 V
constexpr uint16_t kExpectedChargerVsetDeciV = 880;   // 88.0 V
constexpr uint16_t kExpectedChargerIsetDeciA = 1000;  // 100.0 A

// Bilinen 10 ID (Documents/CAN_Message_Table.md ile senkron).
const std::set<uint32_t> kKnownExtIds = {
    0x0000E000, 0x0000E001, 0x0000E002, 0x0000E003, 0x0000E004,
    0x0000E005, 0x0000E032, 0x0000E033, 0x1806E5F4,
};
const std::set<uint32_t> kKnownStdIds = {0x000};

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string idKey(bool isExt, uint32_t id) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s 0x%08X", isExt ? "EXT" : "STD",
             (unsigned)id);
    return buf;
}

// "EXT | 0xE000 | 8 | FF FF .." satırını Frame'e çevirir; bozuk satır ->
// false + err doldurulur.
bool parseLine(const std::string& line, int lineNo, Frame& out,
               std::string& err) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, '|'))
        parts.push_back(trim(item));
    if (parts.size() != 4) {
        err = "beklenen 4 alan (FMT|ID|DLC|HEX), bulunan " +
              std::to_string(parts.size());
        return false;
    }

    if (parts[0] == "EXT")
        out.isExt = true;
    else if (parts[0] == "STD")
        out.isExt = false;
    else {
        err = "bilinmeyen frame formati: '" + parts[0] + "'";
        return false;
    }

    out.id = (uint32_t)strtoul(parts[1].c_str(), nullptr, 16);
    out.msg.identifier = out.id;

    const long dlc = strtol(parts[2].c_str(), nullptr, 10);
    if (dlc < 0 || dlc > TWAI_FRAME_MAX_DLC) {
        err = "gecersiz DLC: " + parts[2];
        return false;
    }
    out.msg.data_length_code = (uint8_t)dlc;

    std::stringstream hs(parts[3]);
    std::string byteStr;
    int n = 0;
    while (hs >> byteStr) {
        if (n >= TWAI_FRAME_MAX_DLC) {
            err = "8'den fazla veri byte'i";
            return false;
        }
        out.msg.data[n++] = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
    }
    if (n != dlc) {
        err = "DLC=" + std::to_string(dlc) + " ama " + std::to_string(n) +
              " hex byte var";
        return false;
    }
    out.lineNo = lineNo;
    return true;
}

void processExtFrame(const Frame& f, bool sessionAsserts, Stats& st) {
    char where[32];
    snprintf(where, sizeof(where), "satir %d", f.lineNo);

    switch (f.id) {
        case 0x0000E000: {
            TelemetryData out{};
            if (!CanParse::parseLbBmsE000(f.msg, out)) {
                st.addViolation(std::string(where) +
                                ": parseLbBmsE000 false dondu (DLC<8)");
                return;
            }
            if (!sessionAsserts) return;
            if (out.TEL_bmsPackVoltageDeciV != kExpectedPackVDeciV)
                st.addViolation(std::string(where) + ": E000 packV=" +
                                std::to_string(out.TEL_bmsPackVoltageDeciV) +
                                " deciV, beklenen 790 (79.0 V)");
            if (f.msg.data[2] != 0x03 || f.msg.data[3] != 0x16)
                st.addViolation(std::string(where) +
                                ": E000 byte[2:3] != 0x0316");
            const int32_t cur = out.TEL_bmsCurrentCentiA;
            if (cur != -10 && cur != -20)
                st.addViolation(std::string(where) + ": E000 akim " +
                                std::to_string(cur) +
                                ", beklenen {-10,-20} centi-A (idle)");
            break;
        }
        case 0x1806E5F4: {
            ChargerCommand cmd{};
            if (!CanParse::parseCharger1806E5F4(f.msg, cmd)) {
                st.addViolation(std::string(where) +
                                ": parseCharger1806E5F4 false dondu (DLC<4)");
                return;
            }
            if (!sessionAsserts) return;
            if (cmd.chargeVoltageSetpointDeciV != kExpectedChargerVsetDeciV ||
                cmd.chargeCurrentSetpointDeciA != kExpectedChargerIsetDeciA)
                st.addViolation(
                    std::string(where) + ": charger setpoint Vset=" +
                    std::to_string(cmd.chargeVoltageSetpointDeciV) +
                    " Iset=" + std::to_string(cmd.chargeCurrentSetpointDeciA) +
                    ", beklenen 880/1000 (88.0 V / 100.0 A)");
            break;
        }
        // 0xE001 DOĞRULANDI (sıcaklık byte[6:7] parse edilir); burada yalnız
        // "cokmeden kabul ediyor" regresyonu kosulur. Sonraki case'ler
        // (E002-E005, E032, E033) hala DOĞRULANMAMIŞ stub parser'lardır —
        // TelemetryData'ya anlam yuklenmez.
        case 0x0000E001: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE001(f.msg, out);
            break;
        }
        case 0x0000E002: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE002(f.msg, out);
            break;
        }
        case 0x0000E003: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE003(f.msg, out);
            break;
        }
        case 0x0000E004: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE004(f.msg, out);
            break;
        }
        case 0x0000E005: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE005(f.msg, out);
            break;
        }
        case 0x0000E032: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE032(f.msg, out);
            break;
        }
        case 0x0000E033: {
            TelemetryData out{};
            (void)CanParse::parseLbBmsE033(f.msg, out);
            break;
        }
        default:
            st.unknownIds.insert(idKey(true, f.id));
            break;
    }
}

}  // namespace

int main(int argc, char** argv) {
    bool sessionAsserts = true;
    std::string path;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--generic") == 0)
            sessionAsserts = false;
        else
            path = argv[i];
    }
    if (path.empty()) {
        fprintf(stderr,
                "kullanim: can_replay [--generic] <sniffer-log.txt>\n"
                "  --generic  oturuma ozel sabit-deger assert'lerini kapat\n");
        return 2;
    }

    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "HATA: log acilamadi: %s\n", path.c_str());
        return 2;
    }

    Stats st;
    std::string line;
    int lineNo = 0;
    long badLines = 0;
    while (std::getline(in, line)) {
        lineNo++;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;

        Frame f;
        std::string err;
        if (!parseLine(t, lineNo, f, err)) {
            badLines++;
            st.addViolation("satir " + std::to_string(lineNo) +
                            ": log satiri bozuk — " + err);
            continue;
        }

        st.total++;
        st.countsById[idKey(f.isExt, f.id)]++;

        if (f.isExt) {
            processExtFrame(f, sessionAsserts, st);
        } else if (kKnownStdIds.count(f.id) == 0) {
            st.unknownIds.insert(idKey(false, f.id));
        }
        // STD 0x000: firmware'de islenmiyor (reserved/heartbeat adayi);
        // burada yalnizca sayilir.
    }

    printf("=== can_replay ozet raporu ===\n");
    printf("Log: %s%s\n", path.c_str(),
           sessionAsserts ? " (oturum assert'leri: ACIK)" : " (--generic)");
    printf("Toplam frame: %ld  (bozuk satir: %ld)\n\n", st.total, badLines);
    printf("ID dagilimi:\n");
    for (const auto& kv : st.countsById)
        printf("  %-16s %8ld\n", kv.first.c_str(), kv.second);

    if (!st.unknownIds.empty()) {
        printf("\nBILINEN 10 ID DISINDA ID'LER (%zu):\n", st.unknownIds.size());
        for (const auto& id : st.unknownIds) printf("  %s\n", id.c_str());
    } else {
        printf("\nBilinen 10 ID disinda ID yok. ✓\n");
    }

    if (st.violationCount > 0) {
        printf("\nIHLALLER (%ld toplam, ilk %zu gosteriliyor):\n",
               st.violationCount, st.violations.size());
        for (const auto& v : st.violations) printf("  %s\n", v.c_str());
        printf("\nSONUC: FAIL\n");
        return 1;
    }

    printf("\nTum invariant'lar gecti. SONUC: PASS\n");
    return 0;
}
