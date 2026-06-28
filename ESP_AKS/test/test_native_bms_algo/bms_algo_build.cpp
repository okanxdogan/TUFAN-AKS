// Native test build wrapper'ı: lib/BmsAlgo kaynaklarını yalnızca bu test
// suite kapsamında derler. BmsAlgo saf C++ (IDF bağımsız) olduğundan ekstra
// fake/stub gerekmez; yine de mevcut telemetry/relay kalıbına uyumlu kalmak
// ve sembolleri bu suite'e izole etmek için kaynaklar buradan dahil edilir.
#include "BmsAlgo.cpp"
#include "BmsNextionPacket.cpp"
