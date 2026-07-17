#include <atomic>

#include "VcuLogic.h"
#include "DeratingPolicy.h"
#include "IRelayActuator.h"
#include "SystemConfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static constexpr const char* TAG = "VCU_LOGIC";
static constexpr uint32_t TASK_PERIOD_MS = 20;

namespace VcuLogic {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static std::atomic<VcuState> s_state{VcuState::INIT};
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;
// Monotonic uptime (transition'da SIFIRLANMAZ) — aktüatör periyodik verify'ının
// zamanlaması için (s_stateTimer state geçişinde sıfırlandığından periyot
// ölçümüne uygun değil).
static uint32_t s_uptimeMs = 0;
static TelemetryData s_TEL_latestData = {};
static bool s_VCU_warningLogged = false;
static SemaphoreHandle_t s_TEL_dataMutex = nullptr;
// E-STOP bypass: set atomically in postEvent so queue saturation
// cannot swallow an emergency stop command.
static std::atomic<bool> s_eStopPending{false};

// R1: FAULT bypass — E-STOP ile AYNI desen. FAULT_DETECTED kuyruğu bypass edip
// yalnız bu atomic bayrağı set eder; kuyruk dolu olsa bile fault KAYBOLMAZ.
// TEMİZLENME KURALI: run() her tick exchange(false) ile okur→tüketir; yani
// bayrak "işlenmemiş fault isteği var mı"yı temsil eder ve okunduğu an temizlenir.
// İstek, FAULT'a geçilerek (veya zaten FAULT olunduğu için ek geçiş gerekmeden)
// handle edilmiş sayılır.
static std::atomic<bool> s_faultPending{false};

// M2: enjekte edilen aktüatör (röle sürücüsü) arayüzü. init() içinde bağlanır;
// somut RelayManager singleton'ına doğrudan bağımlılık YOK.
static IRelayActuator* s_relays = nullptr;

static bool s_relaysOpenedInEstop = false;
static bool s_relaysOpenedInFault = false;
static uint32_t s_lastEstopLogMs = 0;
static uint32_t s_lastFaultLogMs = 0;

// READY girişi reddedildiğinde log spam'ini önlemek için: aynı ret nedeni her
// tick değil, neden değiştiğinde veya en fazla 1 sn'de bir loglanır. Neden
// string'leri statik literal olduğundan pointer karşılaştırması geçerlidir.
static const char* s_lastReadyRejectReason = nullptr;
static uint32_t s_lastReadyRejectLogMs = 0;

// Torque komut sink'i (main.cpp bağlar). Motor sürücüsü entegrasyon iskeleti;
// bağlı değilse istek sessizce yok sayılır.
static TorqueSink s_torqueSink = nullptr;

#if RELAY_ROLES_ASSIGNED
// Flaşör gölge durumu (şartname 6.e.ii) — kenar-tetikli setRelay için son
// komut edilen durum. Karar mantığı saf flasherDesiredState'te (VcuLogic.h);
// FAULT/E-STOP dahil HER tick çalışır (allOff bank maskesini açtığından
// flaşör kanalına dokunmaz — sıcaklık eşikte kaldıkça yanık kalır).
static bool s_flasherOn = false;

// IDLE'daki S1 (şarj hattı kontaktörü) son komutu: -1 = bilinmiyor (IDLE'a
// her girişte sıfırlanır → ilk IDLE tick'i S1'i deterministik olarak charger
// durumuna göre yazar), 0/1 = son setRelay komutu. Kenar-tetikli: her tick
// SPI yazmamak için yalnız istenen durum değişince setRelay çağrılır.
static int8_t s_s1LastCmdInIdle = -1;
#endif

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next);
static void requestZeroTorque();
static bool pollEvent(VcuEvent& out);
static bool isResetInterlockSatisfied();
static bool hasWarningCondition();
static bool hasCriticalCondition();
static const char* readyRejectReason(const TelemetryData& VCU_data);
static TelemetryData getTelemetrySnapshot();

static void handleIdle();
static void handleReady();
static void handleDrive();
static void handleEmergencyStop();
static void handleFault();

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void init(IRelayActuator& relays) {
    s_relays = &relays;

    s_eventQueue = xQueueCreate(8, sizeof(VcuEvent));
    if (s_eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    s_TEL_dataMutex = xSemaphoreCreateMutex();
    if (s_TEL_dataMutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create telemetry mutex");
        return;
    }

    // Safety first — ensure all relays are off at startup
    s_relays->allOff(false);

    transitionTo(VcuState::IDLE);
}

void run() {
    s_stateTimer += TASK_PERIOD_MS;
    s_uptimeMs += TASK_PERIOD_MS;

#if RELAY_ROLES_ASSIGNED
    // Flaşör (şartname 6.e.ii): sıcaklık uyarısına bağlı sesli+ışıklı ikaz.
    // run()'ın EN BAŞINDA, tüm erken-return'lardan ÖNCE — FAULT/E-STOP dahil
    // her durumda ve her tick'te çalışır. Karar saf flasherDesiredState'te:
    // 55 °C'de ON, (55−FLASHER_HYSTERESIS_C)=53 °C altına inince OFF; bayat
    // veri/timeout'ta son durum korunur. handleFault/handleEmergencyStop'un
    // periyodik allOff re-assert'i bank maskesi dışındaki flaşör kanalına
    // dokunmadığından bu mantıkla çakışmaz.
    {
        const TelemetryData VCU_snap = getTelemetrySnapshot();
        const bool desired = flasherDesiredState(
            s_flasherOn, VCU_snap.TEL_bmsDataValid,
            VCU_snap.TEL_bmsTimeoutActive, VCU_snap.TEL_bmsTempHighestC);
        if (desired != s_flasherOn) {
            s_relays->setRelay(RELAY_CH_FLASHER, desired);
            ESP_LOGW(TAG, "Sicaklik uyari flasoru %s (temp=%d C)",
                     desired ? "YANDI" : "SONDU",
                     (int)VCU_snap.TEL_bmsTempHighestC);
            s_flasherOn = desired;
        }
    }
#endif

    if (s_eStopPending.exchange(false, std::memory_order_acquire)) {
        if (s_state.load(std::memory_order_relaxed) != VcuState::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        // Already in EMERGENCY_STOP — clear flag but let run() continue
        // so handleEmergencyStop() keeps executing (timer must not reset).
    }

    // R1: FAULT bypass — kuyruk dolu olsa bile kaybolmayan fault isteği. E-STOP'tan
    // SONRA kontrol edilir (E-STOP daha yüksek öncelikli güvenli durum). Zaten
    // FAULT ise re-entry yok (timer sıfırlanmaz); değilse FAULT'a geç. Bayrak her
    // durumda exchange ile tüketilir (temizlenme kuralı — bkz. tanım).
    if (s_faultPending.exchange(false, std::memory_order_acquire)) {
        if (s_state.load(std::memory_order_relaxed) != VcuState::FAULT) {
            ESP_LOGE(TAG, "FAULT pending (atomic bypass) — entering FAULT");
            transitionTo(VcuState::FAULT);
            return;
        }
    }

    // G3: hafif periyodik actuator (röle çıkışı) doğrulaması. Aktüatör katmanı
    // RELAY_VERIFY_PERIOD_MS'den seyrek olmayacak şekilde OLAT/IODIR'i geri
    // okur; uyuşmazlıkta re-init + re-assert eder ve kalıcı atomic fault
    // bayrağını set eder.
    s_relays->verifyIfDue(s_uptimeMs);

    // Actuator fault kalıcı bayrağını HER tick oku (R1: düşen event tuzağı
    // yok). HV bus canlıyken (READY/DRIVE) gelen fault, mevcut fault yoluna
    // (E-STOP dizisi / P3 sıralı kontaktör açma) girer. IDLE'da ise READY
    // giriş guard'ı ile bloklanır (aşağıda), zorla FAULT'a geçilmez.
    {
        VcuState st = s_state.load(std::memory_order_relaxed);
        if ((st == VcuState::READY || st == VcuState::DRIVE) &&
            s_relays->hasActuatorFault()) {
            ESP_LOGE(TAG, "Actuator fault while HV live — entering FAULT");
            transitionTo(VcuState::FAULT);
            return;
        }
    }

    VcuState currentState = s_state.load(std::memory_order_relaxed);
    if ((currentState == VcuState::IDLE || currentState == VcuState::READY ||
         currentState == VcuState::DRIVE) &&
        hasCriticalCondition()) {
        ESP_LOGE(TAG, "Critical safety threshold exceeded, entering FAULT");
        transitionTo(VcuState::FAULT);
        return;
    }

    // AÇIK İŞ (B12): Warning bandında derating (tork/güç sınırlama) politikası
    // — İSKELET KURULDU (bkz. lib/VcuLogic/DeratingPolicy.h). WARN aktifken
    // 0..100 bir tork-izin yüzdesi hesaplanıp kenar-tetikli loglanır, ama
    // ARAÇ DAVRANIŞI HALA DEĞİŞMEZ: bu yüzde hiçbir tork komutuna/CanManager
    // çağrısına bağlanmıyor (motor sürücüsü elimizde yok — bkz. KAPSAM KİLİDİ,
    // DeratingPolicy.h başlık yorumu). Warning READY girişini
    // isReadyEntryPermitted üzerinden zaten bloklar.
    //
    // ENTEGRASYON NOKTASI (motor sürücüsü geldiğinde): aşağıdaki
    // `deratingPercent` değeri, tork komut yolu (setTorqueSink/G2) gerçek
    // frame üretmeye başladığında torku sınırlamak için BURADA kullanılacak
    // (ör. VcuLogic'in DRIVE'da hesapladığı hedef torku bu yüzdeyle çarpıp
    // sink'e onu göndermek) — bugün böyle bir tork komut ÜRETİMİ yok, bu
    // yüzden bağlanacak bir şey de yok. Bkz. SystemConfig.h "B12: Derating
    // Policy" notu (kademe değerleri CONFIG, ekip kalibrasyonu bekliyor).
    if (hasWarningCondition()) {
        if (!s_VCU_warningLogged) {
            const uint8_t deratingPercent =
                DeratingPolicy::computeTorqueAllowPercent(getTelemetrySnapshot());
            ESP_LOGW(TAG,
                     "Warning threshold active, derating onerisi %%%u "
                     "(tork komutuna henuz baglanmadi — motor surucusu yok)",
                     (unsigned)deratingPercent);
            s_VCU_warningLogged = true;
        }
    } else {
        s_VCU_warningLogged = false;
    }

    VcuEvent event = VcuEvent::NONE;
    if (pollEvent(event)) {
        // High priority events — handled regardless of current state
        if (event == VcuEvent::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        if (event == VcuEvent::FAULT_DETECTED) {
            // R1: FAULT normalde atomic bayrak yoluyla (kuyruk bypass) işlenir;
            // bu dal savunma amaçlı fallback'tir. Zaten FAULT ise re-entry yok.
            if (s_state.load(std::memory_order_relaxed) != VcuState::FAULT)
                transitionTo(VcuState::FAULT);
            return;
        }

        currentState = s_state.load(std::memory_order_relaxed);
        if (event == VcuEvent::RESET &&
            (currentState == VcuState::FAULT ||
             currentState == VcuState::EMERGENCY_STOP)) {
            if (!isResetInterlockSatisfied()) {
                ESP_LOGW(TAG, "RESET rejected: safety interlock still active");
                return;
            }
            // Actuator fault'u temizle; donanım hâlâ bozuksa bir sonraki
            // periyodik verify yeniden latch'ler ve READY tekrar bloklanır.
            s_relays->clearActuatorFault();
            transitionTo(VcuState::IDLE);
            return;
        }

        // State-specific event handling
        switch (currentState) {
            case VcuState::IDLE:
                if (event == VcuEvent::START_REQUEST) {
                    // READY 10 kontaktörü kapatıp HV bus'ı enerjilendirir;
                    // batarya hakkında doğrulanmış/taze veri yoksa geçme.
                    // Actuator fault guard'ı isReadyEntryPermitted'ın DIŞINDA
                    // tutuldu: o predicate saf/donanımsız (yalnız TelemetryData);
                    // röle donanım sağlığı ayrı bir enjekte edilmiş sorgu
                    // olduğundan burada ayrı guard olarak kontrol edilir.
                    TelemetryData VCU_snap = getTelemetrySnapshot();
                    bool actuatorFault = s_relays->hasActuatorFault();
                    if (isReadyEntryPermitted(VCU_snap) && !actuatorFault) {
                        transitionTo(VcuState::READY);
                    } else {
                        const char* reason =
                            actuatorFault ? "actuator fault"
                                          : readyRejectReason(VCU_snap);
                        if (reason != s_lastReadyRejectReason ||
                            s_stateTimer - s_lastReadyRejectLogMs >= 1000) {
                            ESP_LOGW(TAG, "READY reddedildi: %s", reason);
                            s_lastReadyRejectReason = reason;
                            s_lastReadyRejectLogMs = s_stateTimer;
                        }
                    }
                }
                break;

            case VcuState::READY:
                if (event == VcuEvent::DRIVE_ENABLE)
                    transitionTo(VcuState::DRIVE);
                break;

            default:
                break;
        }
    }

    // Periodic state logic
    switch (s_state.load(std::memory_order_relaxed)) {
        case VcuState::IDLE:
            handleIdle();
            break;
        case VcuState::READY:
            handleReady();
            break;
        case VcuState::DRIVE:
            handleDrive();
            break;
        case VcuState::EMERGENCY_STOP:
            handleEmergencyStop();
            break;
        case VcuState::FAULT:
            handleFault();
            break;
        default:
            break;
    }
}

void postEvent(VcuEvent event) {
    if (s_eventQueue == nullptr)
        return;
    if (event == VcuEvent::EMERGENCY_STOP) {
        // Bypass the queue so a full queue cannot swallow an E-STOP.
        // The flag is checked at the top of run() before any queue drain.
        s_eStopPending.store(true, std::memory_order_release);
        return;
    }
    if (event == VcuEvent::FAULT_DETECTED) {
        // R1: E-STOP ile AYNI desen — kuyruğu BYPASS et, yalnız atomic bayrağı
        // set et. run() en tepede (kuyruk drenajından önce) bunu okur, böylece
        // kuyruk dolu olsa bile FAULT kaybolmaz.
        //
        // Neden kuyruğa da yazMIYORuz: run() tik başına kuyruktan yalnız BİR
        // olay çeker; FAULT'un bir kopyası kuyrukta kalırsa, bayrak yolu zaten
        // FAULT'a aldıktan sonra o bayat kopya bir sonraki tik'i tüketip ardından
        // gelen olayları (ör. RESET) bir tik geciktirir. E-STOP tam da bu yüzden
        // kuyruğu bypass eder; FAULT da aynı kanıtlanmış deseni izler.
        s_faultPending.store(true, std::memory_order_release);
        return;
    }
    if (xQueueSend(s_eventQueue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d",
                 static_cast<int>(event));
    }
}

VcuState getState() {
    return s_state.load(std::memory_order_relaxed);
}

void setTelemetryData(const TelemetryData& TEL_data) {
    if (s_TEL_dataMutex == nullptr)
        return;

    xSemaphoreTake(s_TEL_dataMutex, portMAX_DELAY);
    s_TEL_latestData = TEL_data;
    xSemaphoreGive(s_TEL_dataMutex);
}

void setTorqueSink(TorqueSink sink) {
    s_torqueSink = sink;
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------
static void handleIdle() {
    // All contactors off — safe resting state
    // Waiting for START_REQUEST from LoRa/UKS
#if RELAY_ROLES_ASSIGNED
    // Şartname 8.2.a.iii: şarjda S1 KAPALI + S2 AÇIK. IDLE'da sürüş bankı
    // (S2 dahil) zaten açık; burada yalnız S1, charger freshness'ına
    // (TEL_chargerActive — CAN_chargerValid'den, bayatlama dahil) göre
    // sürülür. Kenar-tetikli: istenen durum değişmedikçe SPI yazılmaz.
    // Charger aktifken START_REQUEST zaten reddedilir (isReadyEntryPermitted).
    const TelemetryData VCU_snap = getTelemetrySnapshot();
    const int8_t desired = VCU_snap.TEL_chargerActive ? 1 : 0;
    if (desired != s_s1LastCmdInIdle) {
        s_relays->setRelay(RELAY_CH_S1_CHARGE, desired == 1);
        ESP_LOGI(TAG, "IDLE: S1 (sarj hatti) %s (chargerActive=%d)",
                 desired == 1 ? "KAPATILDI" : "ACILDI", (int)desired);
        s_s1LastCmdInIdle = desired;
    }
#endif
}

static void handleReady() {
    // Close the drive-side contactors on entry (runs once via stateTimer guard)
    if (s_stateTimer <= TASK_PERIOD_MS) {
#if RELAY_ROLES_ASSIGNED
        // Şartname 8.2.a.vii: sürüşte S1 AÇIK + S2 KAPALI. READY girişi
        // allOn KULLANMAZ (allOn bank maskesini — S1 dahil — kapatırdı);
        // bunun yerine yalnız SÜRÜŞ bankı (RELAY_DRIVE_BANK_MASK = S2 +
        // kanal 1-7; S1 bilinçli olarak bu maskenin DIŞINDA) kapatılır.
        // S1 savunma amaçlı açık komutlanır: IDLE'da charger aktifken READY
        // zaten reddedildiğinden S1 normalde açıktır; bu yazım sırayı
        // şartname durumuna deterministik kilitler.
        s_relays->setRelay(RELAY_CH_S1_CHARGE, false);
        for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
            if (RELAY_DRIVE_BANK_MASK & (1u << ch))
                s_relays->setRelay(ch, true);
        }
        ESP_LOGI(TAG, "Surus banki kapatildi (S1 acik) — system READY");
#else
        // Roller atanmadı: eski tek-bank davranışı — bank maskesi (10 kanalın
        // tamamı) kapatılır, bayt-bayt bugünkü allOn ile aynı.
        s_relays->allOn();
        ESP_LOGI(TAG, "All contactors closed — system READY");
#endif
    }
    // DRIVE is entered only after an explicit DRIVE_ENABLE command.
    // Future interlocks should be added here before propulsion is allowed.
}

static void handleDrive() {
    // Contactors remain closed during drive.
    // G2: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0) — hiçbir torque
    // komutu ÜRETİLMİYOR. Propulsion, motor sürücüsü gelip torque haritalama
    // modeli tanımlanana kadar fiilen devre dışı.
}

static void handleEmergencyStop() {
    // G2 GERÇEĞİ: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0). Bu
    // fazda torque komutu gönderilmiyor; kontaktörler yük altında açılıyor
    // OLABİLİR. Saha riski: ark/kontak kaynaması. Entegrasyonda
    // sendTorqueCommand(0) dizisi aktive edilecek.
    //
    // Güvenli kapanış sırası (flag'ten bağımsız çağrı sırası): (1) t=0'da
    // sıfır tork iste → (2) VCU_CONTACTOR_OPEN_DELAY_MS bekle → (3) kontaktör aç.
    if (s_stateTimer <= TASK_PERIOD_MS) {
        requestZeroTorque();  // (1) — flag 0 iken gerçek frame üretmez
        ESP_LOGW(TAG, "EMERGENCY STOP: sifir-tork istegi (motor surucusu yoksa atlanir)");
    }

    // (2)+(3): torkun sönmesi için bekle, sonra pozitif kontaktör bankını aç.
    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        if (!s_relaysOpenedInEstop) {
            s_relays->allOff(false); // First time, log it
            s_relaysOpenedInEstop = true;
        } else if (s_stateTimer - s_lastEstopLogMs >= 1000) {
            // G3: sessiz re-assert artık doğrulama DEĞİL sadece log açısından
            // sessiz — allOff() içindeki verifyOutputs() geri-okumayı yapar,
            // uyuşmazsa loglar ve actuator fault'u latch'ler (sürdürür).
            s_relays->allOff(true); // Silent (log) re-assert + verify
        }
    }

    // Log once per second to avoid flooding
    if (s_stateTimer - s_lastEstopLogMs >= 1000) {
        ESP_LOGE(TAG, "EMERGENCY STOP active — all relays off");
        s_lastEstopLogMs = s_stateTimer;
    }
    // Recovery only via physical reset or explicit RESET event
}

static void handleFault() {
    // G2 GERÇEĞİ: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0). Bu
    // fazda torque komutu gönderilmiyor; kontaktörler yük altında açılıyor
    // OLABİLİR. Saha riski: ark/kontak kaynaması. Entegrasyonda
    // sendTorqueCommand(0) dizisi aktive edilecek.
    //
    // Güvenli kapanış sırası: (1) sıfır tork → (2) bekle → (3) kontaktör aç.
    if (s_stateTimer <= TASK_PERIOD_MS) {
        requestZeroTorque();  // (1) — flag 0 iken gerçek frame üretmez
        ESP_LOGW(TAG, "FAULT: sifir-tork istegi (motor surucusu yoksa atlanir)");
    }

    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        if (!s_relaysOpenedInFault) {
            s_relays->allOff(false); // First time, log it
            s_relaysOpenedInFault = true;
        } else if (s_stateTimer - s_lastFaultLogMs >= 1000) {
            // G3: allOff() içindeki verifyOutputs() re-assert'i geri-okur;
            // uyuşmazsa loglar + actuator fault latch'lenir (sürdürülür).
            s_relays->allOff(true); // Silent (log) re-assert + verify
        }
    }

    if (s_stateTimer - s_lastFaultLogMs >= 1000) {
        ESP_LOGE(TAG, "FAULT state — send RESET event to recover");
        s_lastFaultLogMs = s_stateTimer;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// P3 güvenli kapanış sırasının 1. adımı: sıfır tork iste. Sink bağlıysa
// (üretimde CanManager::sendTorqueCommand) çağrılır; MOTOR_DRIVER_PRESENT=0
// iken orada gerçek frame ÜRETİLMEZ. Sink bağlı değilse istek yok sayılır.
static void requestZeroTorque() {
    if (s_torqueSink != nullptr)
        s_torqueSink(0);
}

static void transitionTo(VcuState next) {
    VcuState current = s_state.load(std::memory_order_relaxed);
    ESP_LOGI(TAG, "State: %d → %d", static_cast<int>(current),
             static_cast<int>(next));
    s_state.store(next, std::memory_order_relaxed);
    s_stateTimer = 0;

    if (next == VcuState::EMERGENCY_STOP) {
        s_relaysOpenedInEstop = false;
        s_lastEstopLogMs = (uint32_t)-1000;
    } else if (next == VcuState::FAULT) {
        s_relaysOpenedInFault = false;
        s_lastFaultLogMs = (uint32_t)-1000;
    }

#if RELAY_ROLES_ASSIGNED
    if (next == VcuState::IDLE) {
        // IDLE'a her girişte S1 komut izini "bilinmiyor" yap: FAULT/E-STOP
        // yolunda allOff S1'i açmış olabilir — ilk IDLE tick'i S1'i charger
        // durumuna göre deterministik olarak yeniden yazar (bkz. handleIdle).
        s_s1LastCmdInIdle = -1;
    }
#endif
}

static bool pollEvent(VcuEvent& out) {
    if (s_eventQueue == nullptr)
        return false;
    return xQueueReceive(s_eventQueue, &out, 0) == pdTRUE;
}

static TelemetryData getTelemetrySnapshot() {
    if (s_TEL_dataMutex == nullptr)
        return s_TEL_latestData;

    TelemetryData VCU_dataCopy = {};
    xSemaphoreTake(s_TEL_dataMutex, portMAX_DELAY);
    VCU_dataCopy = s_TEL_latestData; // <-- V HARFİ DÜZELTİLDİ
    xSemaphoreGive(s_TEL_dataMutex);
    return VCU_dataCopy;
}

static bool isResetInterlockSatisfied() {
    return isResetInterlockSatisfied(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

static bool hasWarningCondition() {
    return hasWarningCondition(getTelemetrySnapshot());
}

static bool hasCriticalCondition() {
    return hasCriticalCondition(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

// isReadyEntryPermitted() reddettiğinde hangi koşulun sağlanmadığını döndürür.
// Sıralama predicate ile birebir aynı olmalı; loglama için statik literal
// döndürür (pointer karşılaştırmasıyla "neden değişti mi" tespiti için).
static const char* readyRejectReason(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return "bmsDataValid=0";
#if RELAY_ROLES_ASSIGNED
    // Şartname 8.2.a.iii — sıralama isReadyEntryPermitted ile birebir aynı.
    if (VCU_data.TEL_chargerActive)
        return "charger aktif — sarj modunda READY yasak";
#endif
    if (hasCriticalCondition(VCU_data, VcuState::IDLE))
        return "kritik kosul aktif";
    if (hasWarningCondition(VCU_data))
        return "uyari kosulu aktif";
#if MOTOR_DRIVER_PRESENT
    if (!VCU_data.TEL_motorDataValid)
        return "motorDataValid=0";
#endif
    return "bilinmiyor";
}

// Motor timeout detection already lives in CanParse::isMotorStatusTimedOut +
// CanManager::updateMotorStatusValidity; if the Teknofest spec needs an
// error-flag bit for this, it should hook into that timeout path, not a
// separate one here (separate task).

#ifdef NATIVE_BUILD
void resetForTest() {
    s_state.store(VcuState::INIT, std::memory_order_relaxed);
    s_stateTimer = 0;
    s_uptimeMs = 0;
    s_TEL_latestData = {};
    s_VCU_warningLogged = false;
    s_eStopPending.store(false, std::memory_order_relaxed);
    s_faultPending.store(false, std::memory_order_relaxed);
    s_relays = nullptr;  // init() yeniden bağlar

    s_relaysOpenedInEstop = false;
    s_relaysOpenedInFault = false;
    s_lastEstopLogMs = 0;
    s_lastFaultLogMs = 0;
    s_lastReadyRejectReason = nullptr;
    s_lastReadyRejectLogMs = 0;
    s_torqueSink = nullptr;
#if RELAY_ROLES_ASSIGNED
    s_flasherOn = false;
    s_s1LastCmdInIdle = -1;
#endif

    // Olay kuyruğunu (queue) boşalt
    if (s_eventQueue != nullptr) {
        VcuEvent drained = VcuEvent::NONE;
        while (xQueueReceive(s_eventQueue, &drained, 0) == pdTRUE) {
            // discard
        }
    }
}
#endif

}  // namespace VcuLogic
