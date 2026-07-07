#pragma once
#include <atomic>
#include <cstdint>

#include "LinkMonitor.h"       // link_check_timeout_with_boot_grace (saf)
#include "LoraRxHandler.h"     // lora_classify_rx_byte / lora_note_unknown_byte (saf)
#include "OfflineBuffer.h"     // ob_push/ob_peek/ob_drop_front/ob_count + offline_should_sample
#include "VehicleData.h"       // TelemetryData (M3)

// UplinkScheduler — SAF, tick-driven, donanımsız uplink beyni (M1 refactor).
//
// vTask_LoRa_UKS'in link durum makinesi + offline örnekleme + replay
// politikasını tek yerde toplar. Donanıma (UART/GPIO/ESP-IDF) DOKUNMAZ:
// girdi = zaman + rx byte'ları + telemetri; çıktı = "şimdi şunu gönder"
// kararları (enjekte edilen bir gönderim callback'i üzerinden). Böylece
// aks_loop_sim.py Python modeliyle kod arasındaki ayrışma riski, GERÇEK C++
// sınıfı native test edilerek kapatılır.
//
// DAVRANIŞ KORUYAN: link timeout/boot-grace, 1 Hz offline örnekleme, tik
// başına <=REPLAY_BURST replay + 1 canlı — hepsi eski task gövdesindekiyle
// birebir aynı. Sabitler (timeout, period, burst) ctor'dan enjekte edilir.
//
// OfflineBuffer'ı (global ob_*) besler; tampon zaten native-test edilebilir
// tekil dairesel FIFO'dur (bkz. test_native_offline_buffer). Scheduler onun
// üstünde FSM + örnekleme + replay orkestrasyonunu yürütür.
class UplinkScheduler {
   public:
    // Gönderim callback'i: bir paketi (replay veya canlı) donanıma iletir.
    // AUX hazır değilse / iletemezse FALSE döner → replay paketi tampondan
    // DÜŞÜRÜLMEZ (sıradaki tik'te tekrar denenir). Native testte kayıt/no-op.
    using SendFn = bool (*)(const TelemetryData& pkt, bool isReplay, void* ctx);

    // Bir RX byte'ının sınıflandırma sonucu (orchestration loglar).
    enum class RxResult { HEARTBEAT, UNKNOWN_QUIET, UNKNOWN_WARN };

    // Link FSM geçişi (orchestration DOWN/UP log satırını basar).
    struct LinkTransition {
        bool changed = false;
        bool linkDown = false;    // geçiş sonrası durum
        bool becameDown = false;  // bu tick UP->DOWN
        bool becameUp = false;    // bu tick DOWN->UP
        bool hadSamples = false;  // UP anında kesintide örnek var mıydı
        int bufferedCount = 0;    // UP anında ob_count()
        uint32_t firstTs = 0;     // kesinti ilk örnek ts_ms
        uint32_t lastTs = 0;      // kesinti son örnek ts_ms
    };

    UplinkScheduler(uint32_t linkTimeoutMs, uint32_t bootGraceMs,
                    uint32_t offlineSamplePeriodMs, uint8_t replayBurstPerTick,
                    uint32_t unknownByteWarnIntervalMs);

    // --- RX olayları ---
    RxResult onRxByte(uint8_t rxByte, uint64_t nowMs);
    uint32_t unknownByteCount() const { return m_unknownRxByteCount; }

    // --- Link FSM (her ~10 ms tick) ---
    LinkTransition updateLink(uint64_t nowMs, uint64_t bootMs);
    // R2: LoRa task yazar; LoRa_IsLinkDown() ile BAŞKA task'lar okur — atomic
    // (torn-read yok). Bayrak tek word; relaxed yeterli (senkronizasyon değil,
    // yalnız yırtılmasız görünürlük).
    bool isLinkDown() const { return m_linkDown.load(std::memory_order_relaxed); }

    // --- OFFLINE mod: canlı paketi 1 Hz'e seyreltilmiş örnekle (buffer'a yaz) ---
    // Döner: bu tick tampona bir örnek yazıldı mı.
    bool offlineSample(uint64_t nowMs, const TelemetryData& live);

    // --- NORMAL mod TX tick: <=burst replay (başarılı gönderimde düşür) + 1 canlı ---
    // Döner: bu tick fiilen gönderilen paket sayısı.
    int onTxTickLinkUp(bool haveLive, const TelemetryData& live, SendFn send,
                       void* ctx);

   private:
    // Config (ctor'dan)
    uint32_t m_linkTimeoutMs;
    uint32_t m_bootGraceMs;
    uint32_t m_offlineSamplePeriodMs;
    uint8_t m_replayBurstPerTick;
    uint32_t m_unknownByteWarnIntervalMs;

    // Taşınan statikler (M1'de yer değişti). R2: task-arası paylaşılan ikisi
    // std::atomic yapıldı — 32-bit Xtensa'da 64-bit volatile okuma ATOMİK
    // DEĞİL, LoRa_IsLinkDown()/heartbeat okuyan diğer task'larda torn-read
    // olurdu. relaxed order yeterli: bunlar bağımsız durum bayrakları, başka
    // veriyi publish etmiyor; yalnız yırtılmasız (tek-word) görünürlük gerek.
    //   s_linkDown        -> m_linkDown        (std::atomic<bool>)
    //   s_lastHeartbeatMs -> m_lastHeartbeatMs (std::atomic<uint64_t>)
    //   s_unknownRxByteCount / s_lastUnknownRxByteWarnMs: yalnız LoRa task'inde
    //   okunur/yazılır (paylaşılmaz) → düz tip kalır.
    std::atomic<bool> m_linkDown{false};
    std::atomic<uint64_t> m_lastHeartbeatMs{0u};  // 0 = henüz heartbeat gelmedi
    uint32_t m_unknownRxByteCount = 0u;
    uint64_t m_lastUnknownRxByteWarnMs = 0u;

    // Kesinti örnekleme durumu (eski task-local'ler)
    uint64_t m_lastOfflineSampleMs = 0u;  // 0 = bu kesintide henüz örnek yok
    uint32_t m_offlineFirstTs = 0u;
    uint32_t m_offlineLastTs = 0u;
    bool m_offlineHasSamples = false;
};
