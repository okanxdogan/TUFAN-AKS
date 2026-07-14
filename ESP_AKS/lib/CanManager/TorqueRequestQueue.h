#pragma once
#include <atomic>
#include <cstdint>

// TorqueRequestQueue — SAF (donanım/FreeRTOS bağımsız), tek-değerli "en son
// istenen tork" saklama alanı.
//
// G2 thread-safety hazırlığı: sıfır-tork isteği VcuLogic (VCU task) →
// TorqueSink → CanManager::sendTorqueCommand yolundan gelir; motor sürücüsü
// entegre olup (MOTOR_DRIVER_PRESENT=1) gerçek twai_transmit çağrısı devreye
// girdiğinde bu çağrı ARTIK VCU task'inden DOĞRUDAN yapılmaz. sendTorqueCommand
// isteği yalnızca buraya YAZAR (push); gerçek gönderim CAN task döngüsünde
// (CanManager::processRxMessages ile aynı yerde) drainPending() ile ÇEKİLİP
// yapılır. Böylece twai_transmit HER ZAMAN CAN task'inden çağrılır (bkz.
// Documents/MOTOR_ENTEGRASYON_NOTU.md madde 4).
//
// "EN SON istenen değer kazanır" semantiği kasıtlıdır (bir FreeRTOS kuyruğu
// DEĞİL, tek-yuvalı bir "pending" bayrak + değer): art arda gelen sıfır-tork
// istekleri (ör. E-STOP handler'ının tekrarlanan tick'leri) ya da CAN
// task'inin bir tik'i kaçırması durumunda bir öncekinin üzerine yazılır —
// AKS→UKS telemetri hattındaki "yalnızca en son anlık görüntü gönderilir,
// kaçan paket yeniden gönderilmez" politikasıyla aynı ruhtadır (bkz.
// Documents/LoRa_Link_Analysis.md "Loss handling policy"). Tork gibi sürekli
// güncellenen bir setpoint için bu doğru semantiktir; ESKİ (henüz drain
// edilmemiş) bir ara değerin kaybolması kabul edilebilir, GÜVENLİ tarafta
// kalır (ör. 0 → 50 → 0 sırasında yalnızca son 0 iletilir — ara 50 asla motor
// sürücüye gitmez).
//
// Thread-safety: tek üretici (VCU task, push) / tek tüketici (CAN task,
// drainPending) varsayımıyla tasarlandı — bu, bu depodaki GERÇEK kullanım
// şeklidir (yalnızca CanManager::sendTorqueCommand push() çağırır, yalnızca
// CanManager::processRxMessages drainPending() çağırır). push() önce değeri
// (relaxed) sonra pending bayrağını (release) yazar; drainPending() önce
// pending'i (acquire ile exchange) sonra değeri (relaxed) okur — release/
// acquire çifti, push()'taki değer yazımının drain()'e GÖRÜNÜR olmasını
// garanti eder (bkz. UplinkScheduler.h'deki benzer atomic gerekçelendirme).
class TorqueRequestQueue {
   public:
    // VCU (veya herhangi bir çağıran) task'ten: yeni bir tork isteği kaydeder.
    void push(uint16_t torqueValue) {
        m_value.store(torqueValue, std::memory_order_relaxed);
        m_pending.store(true, std::memory_order_release);
    }

    // CAN task'ten: bekleyen bir istek varsa `out`'a yazar ve true döner
    // (pending'i tüketir, false yapar); bekleyen istek yoksa false döner ve
    // `out`'a DOKUNULMAZ. Yalnızca CAN task (processRxMessages ile aynı
    // döngü) çağırmalıdır.
    bool drainPending(uint16_t& out) {
        if (!m_pending.exchange(false, std::memory_order_acq_rel))
            return false;
        out = m_value.load(std::memory_order_relaxed);
        return true;
    }

    // Gözlem/test amaçlı: bekleyen bir istek var mı (tüketmeden bakar).
    bool hasPending() const {
        return m_pending.load(std::memory_order_acquire);
    }

    // Yalnız testler için: kuyruğu temiz duruma döndürür. (std::atomic
    // üyeler yüzünden bu sınıf kopyalanamaz/atanamaz — testler arası
    // izolasyon için atama yerine bu sıfırlama kullanılır.)
    void resetForTest() {
        m_pending.store(false, std::memory_order_relaxed);
        m_value.store(0, std::memory_order_relaxed);
    }

   private:
    std::atomic<bool> m_pending{false};
    std::atomic<uint16_t> m_value{0};
};
