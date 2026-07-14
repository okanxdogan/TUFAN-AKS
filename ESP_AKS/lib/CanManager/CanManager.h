#pragma once

#include <cstdint>
#include "AutobaudPolicy.h"  // Saf autobaud retry karar mantığı (autobaud_should_retry)
#include "CanParse.h"
#include "TorqueRequestQueue.h"  // G2: VCU task -> CAN task tork isteği kuyruğu
#include "VehicleData.h"  // TelemetryData (M3: LoRa Telemetry class'ına ihtiyaç yok)
#include "TelemetrySanitize.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// MotorStatus struct'ı CanParse.h içinde tanımlı (saf parser'larla paylaşılır).

enum class CAN_Event : uint8_t {
    NONE = 0,
    FAULT_DETECTED = 1,
};

using CAN_EventCallback = void (*)(CAN_Event CAN_event, void* CAN_context);

class CanManager {
   public:
    CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin);

    bool begin();
    void setEventCallback(CAN_EventCallback CAN_callback, void* CAN_context);

    // Motor sürücüsü torque komutu. Çağıran taraf (VcuLogic, VCU task) hangi
    // task'ten çağırırsa çağırsın GÜVENLİDİR — gerçek twai_transmit BURADAN
    // asla DOĞRUDAN yapılmaz (G2 thread-safety). MOTOR_DRIVER_PRESENT=0 iken
    // GERÇEK FRAME GÖNDERMEZ (bir kez uyarı loglar, false döner, kuyruğa da
    // yazmaz); =1 iken isteği TorqueRequestQueue'ya yazar (true döner) —
    // gerçek gönderim processRxMessages() içinden (CAN task döngüsü)
    // drainTorqueQueue() ile yapılır. E-STOP/FAULT güvenli kapanış sırasında
    // torque(0) ile çağrılır. Bkz. Documents/MOTOR_ENTEGRASYON_NOTU.md madde 4.
    bool sendTorqueCommand(uint16_t torqueValue);

    // Dispatch one received message — call this in the CAN task loop
    void processRxMessages();

    // Thread-safe read of latest motor status
    MotorStatus getMotorStatus() const;

    // Returns raw internal telemetry. Sanitization happens only at the uplink gate in vTask_LoRa_UKS.
    TelemetryData getTelemetryData() const;

    // Thread-safe read of last charger command (0x1806E5F4, DOĞRULANDI).
    // `out` her zaman son görülen setpoint'lerle doldurulur; dönüş değeri
    // verinin taze olup olmadığını söyler (CAN_CHARGER_TIMEOUT_MS içinde
    // frame görüldüyse true). Charger akışı OPSİYONEL — false FAULT değildir.
    // NOT: Gözlem/test amaçlı API — firmware'de şu an BİLİNÇLİ olarak hiçbir
    // çağıranı yok (setpoint'ler karar mantığına bağlanmaz); bench/diagnostik
    // kullanım için tutulur, "ölü kod" diye SİLİNMEMELİ.
    bool getChargerCommand(ChargerCommand& out) const;

   private:
    void handleMotorStatus(const twai_message_t& msg);

    // Lithium Balance c-BMS handler'ları
    void handleLbBmsE000(const twai_message_t& msg);        // packV, akım, SoC — DOĞRULANDI
    void handleLbBmsE001(const twai_message_t& msg);        // Sıcaklıklar — DOĞRULANDI
    void handleLbBmsE015(const twai_message_t& msg);
    void handleLbBmsE016(const twai_message_t& msg);
    void handleLbBmsE017(const twai_message_t& msg);
    void handleLbBmsE018(const twai_message_t& msg);
    void handleLbBmsE019(const twai_message_t& msg);
    void handleLbBmsE020(const twai_message_t& msg);
    void handleCharger1806E5F4(const twai_message_t& msg);  // setpoint'ler — DOĞRULANDI (AKS yalnızca dinler)
    void handleLbBmsStub(const twai_message_t& msg, uint32_t canId);  // diğer ID'ler — DOĞRULANMADI

    void updateMotorStatusValidity();
    void updateBmsValidity();
    void updateCellVoltageValidity();
    void updateChargerValidity();
    void notifyFaultIfNeeded(uint8_t CAN_previousFlags, uint8_t CAN_currentFlags,
                             const char* CAN_faultSource);

    // Bitrate henüz doğrulanmamışken (CAN_bitrateVerified=false) VE hiçbir
    // geçerli frame alınmamışken (CAN_hasReceivedAnyFrame=false), CAN task
    // döngüsünden her tik çağrılır — karar (retry zamanı geldi mi?) saf
    // autobaud_should_retry (AutobaudPolicy.h) ile verilir. Retry-eligible
    // olduğunda TEK bir hız (rotasyonla) yeniden denenir; processRxMessages()
    // ve sendTorqueCommand() bu pencerede isInitialized=false ile no-op
    // kalır (yarım kurulu driver'a twai_* çağrısı gitmez).
    void retryAutobaudIfNeeded();

    // G2: processRxMessages() (CAN task döngüsü) tarafından her tik çağrılır;
    // s_torqueQueue'da bekleyen bir istek varsa gerçek twai_transmit'i BURADAN
    // (CAN task'inden) yapar. MOTOR_DRIVER_PRESENT=0 iken hiçbir şey yapmaz
    // (kuyruğa zaten yazılmaz — sendTorqueCommand bkz.).
    void drainTorqueQueue();

    twai_general_config_t g_config;
    twai_timing_config_t t_config;
    twai_filter_config_t f_config;
    bool isInitialized = false;

    MotorStatus s_motorStatus = {};
    TelemetryData s_telemetryData = {};
    mutable SemaphoreHandle_t s_mutex = nullptr;

    TickType_t CAN_lastMotorStatusTick = 0;
    bool CAN_hasSeenMotorStatus = false;
    bool CAN_motorTimeoutLogged = false;

    // G9: motor errorFlags debounce sayacı — ardışık hatalı frame sayısı. Temiz
    // frame gelince sıfırlanır (bkz. MotorFaultDebounce.h). Yalnız handleMotorStatus
    // yazar/okur (CAN task'ine yerel; ek mutex gerektirmez).
    uint16_t CAN_motorErrorConsecutive = 0;

    bool CAN_busOffLogged = false;
    bool CAN_busRecoveredLogged = false;

    // Autobaud yeniden-deneme durumu (bkz. AutobaudPolicy.h / BRING_UP_
    // CHECKLIST.md bölüm 4). CAN_bitrateVerified: begin()'de auto-detect
    // başarılıysa VEYA bir retry döngüsü bitrate bulduysa true. CAN_
    // hasReceivedAnyFrame: fallback hızında bile PASİF olarak ilk geçerli
    // frame alındığında true (processRxMessages ana drain döngüsünde set
    // edilir) — ikisinden biri true olduğu an retry KALICI OLARAK durur.
    bool CAN_bitrateVerified = false;
    bool CAN_hasReceivedAnyFrame = false;
    uint8_t CAN_autobaudRetryBaudIndex = 0;  // 0=500k,1=125k,2=250k rotasyonu
    TickType_t CAN_lastAutobaudAttemptTick = 0;
    TickType_t CAN_lastAutobaudWarnLogTick = 0;

    // G6: RX yolu sertleştirme sayaçları (sibling counter'larla aynı CAN_
    // önek konvansiyonu). RX_QUEUE_FULL alarmı ve atılan remote (RTR) frame'ler.
    uint32_t CAN_rxQueueFullCount = 0;
    uint32_t CAN_rxRemoteFrameCount = 0;
    TickType_t CAN_lastRxQueueFullLogTick = 0;

    // sendTorqueCommand flag-0 yolunda tek-sefer uyarı (E-STOP spam önleme).
    bool CAN_torqueSkipLogged = false;

    // G2: VCU task'ten gelen tork isteğinin CAN task'e ulaştığı kuyruk —
    // yalnızca sendTorqueCommand() yazar (push), yalnızca drainTorqueQueue()
    // okur (drainPending). Bkz. TorqueRequestQueue.h.
    TorqueRequestQueue s_torqueQueue;

    // BMS freshness tracking — G12: packV (E000) ve sıcaklık (E001) AYRI
    // mesaj-ID'leri; freshness ID bazına izlenir. TEL_bmsDataValid /
    // TEL_bmsTimeoutActive kararı updateBmsValidity'de İKİSİ birleştirilerek
    // verilir (biri akıp diğeri kesilirse bayat alan maskelenmesin).
    TickType_t CAN_lastBmsE000Tick = 0;
    bool CAN_hasSeen_BmsE000 = false;
    TickType_t CAN_lastBmsE001Tick = 0;
    bool CAN_hasSeen_BmsE001 = false;
    bool CAN_bmsTimeoutLogged = false;

    // Cell voltage freshness tracking (E015-E020)
    TickType_t CAN_lastCellVoltageTick = 0;
    bool CAN_hasSeen_CellVoltage = false;
    bool CAN_cellVoltageTimeoutLogged = false;
    uint8_t CAN_cellVoltageSeenMask = 0;  // bit0=E015, bit1=E016... bit5=E020
    bool CAN_cellVoltageComplete = false; // true if all 6 received

    // Pack voltajı eşik ihlali bayrakları (bit0 = undervoltage,
    // bit1 = overvoltage). Motor errorFlags ile aynı edge-trigger deseni:
    // notifyFaultIfNeeded yalnızca değişimde CAN_Event yayınlar.
    uint8_t CAN_bmsPackFaultFlags = 0;

    // Charger freshness tracking — 0x1806E5F4 (OPSİYONEL akış).
    // Bayatlama yalnızca CAN_chargerValid'i düşürür; FAULT üretmez.
    ChargerCommand s_chargerCommand = {};
    TickType_t CAN_lastChargerTick = 0;
    bool CAN_hasSeenCharger = false;
    bool CAN_chargerValid = false;
    bool CAN_chargerStaleLogged = false;


    CAN_EventCallback CAN_eventCallback = nullptr;
    void* CAN_eventContext = nullptr;
};
