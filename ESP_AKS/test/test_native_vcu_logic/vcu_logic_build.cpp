// Native test build wrapper'ı: lib/VcuLogic/VcuLogic.cpp'yi YALNIZCA bu test
// suite'i kapsamında derler. VcuLogic library'si native'de lib_ignore ile
// dışlandığından (diğer suite'lere freertos/esp_log sembolü sızdırmaz), .cpp'yi
// bu vendor-include ile derliyoruz. Yol -I lib/VcuLogic (platformio.ini
// [env:native]) üzerinden çözülür. Production (esp32dev) build'de LDF onu
// normal library olarak derler; bu dosya test/ ağacı dışında kaldığından
// derlenmez.
#include "VcuLogic.cpp"
