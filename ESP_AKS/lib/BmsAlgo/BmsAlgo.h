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
static constexpr uint16_t BMS_SOC_EMPTY_MV = 3000;  // %0 referans hücre gerilimi
static constexpr uint16_t BMS_SOC_FULL_MV = 4200;   // %100 referans hücre gerilimi

// ===========================================================================
// UYARI (WARNING / CRITICAL) EŞİKLERİ
// ===========================================================================
// CRITICAL koşulları (herhangi biri yeterli): koruma müdahalesi gerekebilir.
static constexpr uint16_t BMS_CELL_UNDERVOLT_CRIT_MV = 3000;  // < => CRITICAL
static constexpr uint16_t BMS_CELL_OVERVOLT_CRIT_MV = 4250;   // > => CRITICAL
static constexpr int16_t BMS_TEMP_OVERTEMP_CRIT_C = 60;       // > => CRITICAL

// WARNING koşulları: eşiğe yaklaşıldı; henüz kritik değil ama izlenmeli.
static constexpr uint16_t BMS_CELL_UNDERVOLT_WARN_MV = 3200;  // < => WARNING
static constexpr uint16_t BMS_CELL_OVERVOLT_WARN_MV = 4150;   // > => WARNING
static constexpr int16_t BMS_TEMP_OVERTEMP_WARN_C = 50;       // > => WARNING

// ---------------------------------------------------------------------------
// Tek anlık görüntüyü yorumla. in.isValid == false ise güvenli taraf seçilir:
// warningLevel = CRITICAL ve makul (zararsız) varsayılanlar döner.
// ---------------------------------------------------------------------------
BmsComputed computePack(const BmsPackData& in);
