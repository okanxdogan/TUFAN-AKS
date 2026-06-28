// Native test build wrapper'ı: lib/BmsSim kaynaklarını yalnızca bu test
// suite'i kapsamında derler.  BmsSim saf C++ ve IDF bağımlısı değil; yine de
// diğer native suite'lere sembol sızdırmamak için kaynakları burada izole
// biçimde getiriyoruz (mevcut telemetry_build.cpp / relay_manager_build.cpp
// idiomu).
#include "SimCellDataSource.cpp"
#include "RealCellDataSource.cpp"
