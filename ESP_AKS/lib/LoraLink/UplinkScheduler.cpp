#include "UplinkScheduler.h"

UplinkScheduler::UplinkScheduler(uint32_t linkTimeoutMs, uint32_t bootGraceMs,
                                 uint32_t offlineSamplePeriodMs,
                                 uint8_t replayBurstPerTick,
                                 uint32_t unknownByteWarnIntervalMs)
    : m_linkTimeoutMs(linkTimeoutMs),
      m_bootGraceMs(bootGraceMs),
      m_offlineSamplePeriodMs(offlineSamplePeriodMs),
      m_replayBurstPerTick(replayBurstPerTick),
      m_unknownByteWarnIntervalMs(unknownByteWarnIntervalMs) {}

UplinkScheduler::RxResult UplinkScheduler::onRxByte(uint8_t rxByte,
                                                    uint64_t nowMs) {
    if (lora_classify_rx_byte(rxByte) == LoraRxByteKind::HEARTBEAT) {
        m_lastHeartbeatMs.store(nowMs, std::memory_order_relaxed);
        return RxResult::HEARTBEAT;
    }
    // Bilinmeyen byte — sayaç her zaman artar, throttled WARN kararı döner.
    uint32_t count = m_unknownRxByteCount;
    uint64_t lastWarn = m_lastUnknownRxByteWarnMs;
    const bool warn = lora_note_unknown_byte(nowMs, &count, &lastWarn,
                                             m_unknownByteWarnIntervalMs);
    m_unknownRxByteCount = count;
    m_lastUnknownRxByteWarnMs = lastWarn;
    return warn ? RxResult::UNKNOWN_WARN : RxResult::UNKNOWN_QUIET;
}

UplinkScheduler::LinkTransition UplinkScheduler::updateLink(uint64_t nowMs,
                                                            uint64_t bootMs) {
    const uint64_t lastHeartbeatMs =
        m_lastHeartbeatMs.load(std::memory_order_relaxed);
    const bool timeout = link_check_timeout_with_boot_grace(
        nowMs, lastHeartbeatMs, m_linkTimeoutMs, bootMs, m_bootGraceMs);

    const bool linkDown = m_linkDown.load(std::memory_order_relaxed);
    LinkTransition tr;
    tr.linkDown = linkDown;

    if (timeout && !linkDown) {
        m_linkDown.store(true, std::memory_order_relaxed);
        // Yeni kesinti başlıyor — örnekleme saatini ve ts aralığını sıfırla.
        m_lastOfflineSampleMs = 0u;
        m_offlineHasSamples = false;
        tr.changed = true;
        tr.becameDown = true;
        tr.linkDown = true;
    } else if (!timeout && linkDown && lastHeartbeatMs != 0u) {
        m_linkDown.store(false, std::memory_order_relaxed);
        tr.changed = true;
        tr.becameUp = true;
        tr.linkDown = false;
        tr.hadSamples = m_offlineHasSamples;
        tr.bufferedCount = ob_count();
        tr.firstTs = m_offlineFirstTs;
        tr.lastTs = m_offlineLastTs;
    }
    return tr;
}

bool UplinkScheduler::offlineSample(uint64_t nowMs, const TelemetryData& live) {
    if (!offline_should_sample(nowMs, m_lastOfflineSampleMs, m_offlineHasSamples,
                               m_offlineSamplePeriodMs)) {
        return false;
    }
    m_lastOfflineSampleMs = nowMs;
    ob_push(live);
    // TEL_timestampMs paket üretildiğinde (CAN task) set edildi — DEĞİŞTİRİLMEZ;
    // kesinti aralığının ts ile ispatı (9.2.e / 9.4.b.vi) buna dayanır.
    if (!m_offlineHasSamples) {
        m_offlineFirstTs = live.TEL_timestampMs;
        m_offlineHasSamples = true;
    }
    m_offlineLastTs = live.TEL_timestampMs;
    return true;
}

int UplinkScheduler::onTxTickLinkUp(bool haveLive, const TelemetryData& live,
                                    SendFn send, void* ctx) {
    int sent = 0;

    // Throttled replay: seq yalnızca gerçek TX anında ilerler (Telemetry
    // içinde) — peek/drop_front sırayı bozmaz. AUX meşgulse paket tamponda
    // KALIR (send FALSE döner → break, düşürülmez).
    for (uint8_t b = 0; b < m_replayBurstPerTick; b++) {
        TelemetryData replay = {};
        if (!ob_peek(replay))
            break;  // tampon boş
        if (!send(replay, /*isReplay=*/true, ctx))
            break;  // AUX meşgul → paket kalır, sıradaki tik'te tekrar denenir
        ob_drop_front();
        sent++;
    }

    if (haveLive) {
        if (send(live, /*isReplay=*/false, ctx))
            sent++;
        // AUX meşgulse canlı paket atlanır (offline değil, link UP — sıradaki
        // tik'te taze canlı okuma denenir).
    }
    return sent;
}
