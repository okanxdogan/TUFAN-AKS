#pragma once
//
// RealCellDataSource — gerçek BMS gelince doldurulacak STUB.
// ICellDataSource sözleşmesini implemente eder; SimCellDataSource ile birebir
// aynı arayüzü sunar ki ana kart ikisini ayırt edemesin (HAL kuralı).
//
// TASARIM TERCİHİ: bu sınıf saf bir in-memory buffer'dır. Donanım/IDF/CAN
// bağımlılığı YOKTUR — bu yüzden native testlerde de derlenir. Gerçek CAN
// entegrasyonu DIŞARIDA yapılır: CAN görev döngüsü (örn. CanManager) gelen
// frame'leri parse edip ingest() ile bu buffer'a yazar; ana kart ise read()
// ile okur. Böylece "gerçek BMS" yolu da transport-agnostik kalır.
//
//   [CAN RX task] --parse--> ingest(BmsPackData)  --read()--> [MCU algoritması]
//
#include <cstdint>

#include "BmsModel.h"

class RealCellDataSource : public ICellDataSource {
   public:
    RealCellDataSource();

    // ICellDataSource sözleşmesi -----------------------------------------
    // Gerçek donanımda CAN sürücüsü init burada yapılır. Saf buffer sürümünde
    // no-op; true döner.
    bool begin() override;

    // En son ingest() ile yazılan anlık görüntüyü kopyalar. Henüz taze veri
    // yoksa out.isValid=false yazar ve false döner.
    //
    // GERÇEK ENTEGRASYON NOTU: CAN frame'lerini doğrudan burada okumak yerine
    // (mutex/timeout karmaşası MCU'ya sızmasın diye) ingest() ile beslenen
    // son geçerli görüntüyü döndürürüz. Timeout/geçersizlik kararı ingest
    // eden tarafta verilir (bkz. invalidate()).
    bool read(BmsPackData& out) override;

    // CAN entegrasyon yüzeyi ---------------------------------------------
    // CAN görev döngüsü, 0x111/0x112 vb. frame'leri parse ettikten sonra
    // tamamlanmış paket görüntüsünü buraya yazar. snapshot.isValid alanı
    // çağıran tarafça doğru set edilmelidir.
    void ingest(const BmsPackData& snapshot);

    // Veri tazeliği kaybolduğunda (CAN timeout) çağrılır: bir sonraki read()
    // isValid=false döndürür.
    void invalidate();

   private:
    BmsPackData REAL_latest_;  // son ingest edilen görüntü
    bool        REAL_hasData_; // hiç ingest edildi mi
};
