#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Nextion Reset (brown-out) Algılama (SAF, donanımsız, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: Nextion brown-out/reset attığında tüm component'ler Editor
// varsayılanlarına döner, ama ESP'nin change-cache'leri (HMI_lastScreenData /
// BmsNextionCache) eski değerleri tutmaya devam eder. updateScreen'deki
// `HMI_forceRefresh = !HMI_hasCachedScreen` yalnızca ESP boot'undan sonraki
// İLK çağrıda true olduğundan, reset sonrası sadece sürekli değişen alanlar
// (packa, centiA çözünürlüğü) yeniden gönderilir; packv/temp/bat/state/valid/
// contactor/hücre barları BİR DAHA ASLA gönderilmez (saha gözlemi). Ayrıca
// begin()'de bir kez gönderilen bkcmd=0 da reset ile kaybolur.
//
// ÇÖZÜM: Nextion açılışta RX hattına Startup event'ini basar:
//     0x00 0x00 0x00 0xFF 0xFF 0xFF
// Bu dedektör, readTouchCommand'ın byte okuma yoluna PARALEL bağlanır ve bu
// diziyi byte-byte bir durum makinesiyle arar. Dizi tamamlandığında true
// döner; çağıran (DisplayHMI) bkcmd=0'ı yeniden gönderir, cache'leri geçersiz
// kılar ve consumeResetFlag() üzerinden BMS cache resetini tetikler.
//
// Durum makinesi KMP tarzı geri dönüşlü: eşleşme kırılan byte 0x00/0xFF ise
// desenin mümkün olan en uzun ön-ekine geri döner (baştan aramaz) — böylece
// "00 00 00 00 FF FF FF" (fazladan sıfır) veya parçalı/karışık akışlarda dizi
// kaçırılmaz. 0x5A/CMD/~CMD dokunmatik çerçeveleri 0x00-0xFF dışı byte'larla
// dedektörü sıfırlar, çerçeve mantığına HİÇ dokunulmaz (paralel gözlemci).
//
// Referans desen: lib/CanManager/AutobaudPolicy.h (saf karar mantığı +
// native test edilebilirlik). Testler: test/test_native_hmi_helpers/
// test_reset_detect.cpp.
class HMI_NextionResetDetect {
   public:
    // Bir RX byte'ı besler; Startup event dizisinin SON byte'ıyla true döner
    // (dedektör bir sonraki arama için kendini sıfırlar), aksi halde false.
    bool HMI_feedByte(uint8_t HMI_byte) {
        if (HMI_matched < 3) {
            // 0x00 ön-ek fazı: her 0x00 ilerletir, başka her byte sıfırlar.
            HMI_matched = (HMI_byte == 0x00) ? (uint8_t)(HMI_matched + 1) : 0;
            return false;
        }
        // 0xFF sonek fazı (HMI_matched 3..5).
        if (HMI_byte == 0xFF) {
            ++HMI_matched;
            if (HMI_matched == 6) {
                HMI_matched = 0;
                return true;
            }
            return false;
        }
        if (HMI_byte == 0x00) {
            // Desen kırıldı ama 0x00 yeni bir ön-ek başlatabilir:
            //   "00 00 00" + 0x00  -> hâlâ "00 00 00" (3'te kal),
            //   "00 00 00 FF(..)" + 0x00 -> yalnız son 0x00 ön-ek (1'e dön).
            HMI_matched = (HMI_matched == 3) ? 3 : 1;
            return false;
        }
        HMI_matched = 0;
        return false;
    }

    void HMI_reset() { HMI_matched = 0; }

   private:
    // Desenin eşleşmiş byte sayısı (0..5); 6'ya ulaşınca event tamamlanır.
    uint8_t HMI_matched = 0;
};
