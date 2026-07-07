#pragma once
//
// BmsAlgo.h — Ham BmsPackData'yı yorumlayan SAF algoritma katmanı.
//
// computePack(): tek bir anlık görüntüyü (BmsPackData) alır, dengeleme/SoC/
// uyarı kararlarını içeren BmsComputed üretir. Yan etkisiz, deterministik,
// donanım bağımsız — native test edilebilir.
//
// Donanım/IDF bağımlılığı YOKTUR — saf C++17.
//
#include <cstdint>

#include "BmsComputed.h"
#include "BmsModel.h"  // BMS_CELL_COUNT, BmsPackData

// ===========================================================================
// DENGELEME (BALANCING) SABİTLERİ
// ===========================================================================
// Kural: Hücreler arasındaki fark (max - min) bu eşiği AŞARSA, en yüksek
// gerilimli hücre(ler) deşarj edilir. Eşik dahil edilmez (strictly greater):
// delta == 50 mV  -> dengeleme YOK, delta == 51 mV -> dengeleme VAR.
//
// 50 mV; Li-ion için tipik bir pasif denge tetik bandıdır: ölçüm gürültüsünün
// (~birkaç mV) üzerinde, ama hücreleri gereksiz yere boşaltmayacak kadar dar.
static constexpr uint16_t BMS_BALANCE_THRESHOLD_MV = 50;

// Marj: delta eşiği aştığında, en yüksek hücreye bu marj (mV) içinde olan TÜM
// hücreler dengelenir. Böylece neredeyse eşit iki "en yüksek" hücre birlikte
// boşaltılır ve tek bir hücreye yük binmesi önlenir. 0 => yalnız tek en yüksek.
static constexpr uint16_t BMS_BALANCE_TOP_MARGIN_MV = 5;

// ===========================================================================
// SoC (STATE OF CHARGE) SABİTLERİ — basit lineer OCV haritalaması
// ===========================================================================
// SoC, ortalama hücre geriliminin [EMPTY..FULL] aralığında lineer konumudur.
// Açıklanabilir ve native test edilebilir olması için kasıtlı olarak basittir
// (coulomb counting / OCV eğrisi yok). Aralık dışı değerler 0..100'e clamp'lenir.
//
// Kaynak: paket spec (24S LiFePO4, 2.50–3.65 V/hücre) — bkz.
// Documents/Threshold_Ownership.md. EMPTY/FULL, SystemConfig.h'deki pack
// CRITICAL eşikleriyle aynı fiziksel uçlara karşılık gelir: hücre_eşiği × 24
// = pack_eşiği (2500 mV × 24 = 60000 mV = 600 deciV = BMS_CRITICAL_MIN_PACK_
// VOLTAGE_DECI_V; 3650 mV × 24 = 87600 mV = 876 deciV =
// BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V).
//
// NOT: LiFePO4'ün OCV eğrisi 3.2–3.3 V aralığında ÇOK düzdür; bu yüzden
// lineer haritalama LiFePO4'te KABA bir SoC tahmini verir (düz bölgede küçük
// gerilim farkları için SoC ya çok az değişir ya da ölçüm gürültüsüne aşırı
// duyarlı olabilir). Bu, bilinçli bir basitleştirme tercihidir — DEĞİŞTİRİLMEDİ;
// ileride OCV tablosu / coulomb counting ile iyileştirilebilir.
static constexpr uint16_t BMS_SOC_EMPTY_MV = 2500;  // %0 referans hücre gerilimi — LiFePO4 spec min (2.50 V)
static constexpr uint16_t BMS_SOC_FULL_MV = 3650;   // %100 referans hücre gerilimi — LiFePO4 spec maks (3.65 V)

// ===========================================================================
// UYARI (WARNING / CRITICAL) EŞİKLERİ
// ===========================================================================
// Kaynak: paket spec (24S LiFePO4, 2.50–3.65 V/hücre) — bkz.
// Documents/Threshold_Ownership.md. Semantik computePack()'ten (bkz.
// BmsAlgo.cpp): strictly < / > — eşik değerinin KENDİSİ henüz tetiklemez
// (ör. 2500 mV CRITICAL DEĞİL, 2499 mV CRITICAL'dır; 3650 mV CRITICAL DEĞİL,
// 3651 mV CRITICAL'dır).
//
// CRITICAL koşulları (herhangi biri yeterli): koruma müdahalesi gerekebilir.
// UNDERVOLT/OVERVOLT CRIT, pack spec uçlarıyla birebir örtüşür: hücre_eşiği
// × 24 = pack_eşiği (bkz. yukarıdaki SoC notu — aynı 2500/3650 mV değerleri).
static constexpr uint16_t BMS_CELL_UNDERVOLT_CRIT_MV = 2500;  // < => CRITICAL — LiFePO4 spec min (2.50 V)
static constexpr uint16_t BMS_CELL_OVERVOLT_CRIT_MV = 3650;   // > => CRITICAL — LiFePO4 spec maks (3.65 V)
static constexpr int16_t BMS_TEMP_OVERTEMP_CRIT_C = 60;       // > => CRITICAL

// WARNING koşulları: eşiğe yaklaşıldı; henüz kritik değil ama izlenmeli.
// Aynı semantik (strictly < / >). WARN marjı bu katmana özgüdür — SystemConfig.h
// pack-bazlı WARN eşikleriyle (720/852 deciV) birebir hücre×24 eşleşmesi
// GEREKMEZ (yalnızca CRITICAL uçları pack spec'iyle hizalıdır); bu iki katman
// WARN bandını bağımsız seçebilir.
static constexpr uint16_t BMS_CELL_UNDERVOLT_WARN_MV = 2800;  // < => WARNING — CRIT'e (2500) 300 mV marj
static constexpr uint16_t BMS_CELL_OVERVOLT_WARN_MV = 3550;   // > => WARNING — CRIT'e (3650) 100 mV marj
static constexpr int16_t BMS_TEMP_OVERTEMP_WARN_C = 50;       // > => WARNING

// ---------------------------------------------------------------------------
// Tek anlık görüntüyü yorumla.
//   * in.isValid == false        => güvenli taraf: warningLevel = CRITICAL,
//                                    zararsız varsayılanlar (bayat/yok veri).
//   * in.cellDataValid == false  => (paket geçerli ama hücre kaynağı DOĞRULANMADI)
//                                    dengeleme/uyarı HESAPLANMAZ; warningLevel =
//                                    NO_DATA, hiçbir hücre dengelenmez. Fabrike
//                                    per-hücre veriye güvenilmez (G8/M4).
//   * aksi halde                 => gerçek min/max/denge/SoC/uyarı hesaplanır.
// ---------------------------------------------------------------------------
BmsComputed computePack(const BmsPackData& in);
