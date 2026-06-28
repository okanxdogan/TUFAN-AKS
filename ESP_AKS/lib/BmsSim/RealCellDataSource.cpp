//
// RealCellDataSource.cpp — gerçek BMS yolu için saf in-memory buffer STUB'u.
// Donanım/IDF bağımlılığı YOKTUR; native testlerde de derlenir.
//
// Gerçek CAN entegrasyonu yapıldığında bu dosyada DEĞİL, CAN görev döngüsünde
// frame parse edilir ve ingest() çağrılır. Bu sınıf yalnızca son geçerli
// görüntüyü thread'ler arası basit/transport-agnostik şekilde taşır.
//
#include "RealCellDataSource.h"

RealCellDataSource::RealCellDataSource() : REAL_latest_{}, REAL_hasData_(false) {
    REAL_latest_.isValid = false;
}

bool RealCellDataSource::begin() {
    // Gerçek donanımda: TWAI/CAN sürücüsü init, filtre kurulumu vb. buraya.
    // Saf buffer sürümünde yapacak bir şey yok.
    REAL_hasData_ = false;
    REAL_latest_.isValid = false;
    return true;
}

bool RealCellDataSource::read(BmsPackData& out) {
    if (!REAL_hasData_) {
        // Henüz hiç frame ingest edilmedi — taze veri yok.
        out = BmsPackData{};
        out.isValid = false;
        return false;
    }
    out = REAL_latest_;
    return out.isValid;
}

void RealCellDataSource::ingest(const BmsPackData& snapshot) {
    REAL_latest_ = snapshot;
    REAL_hasData_ = true;
}

void RealCellDataSource::invalidate() {
    REAL_latest_.isValid = false;
}
