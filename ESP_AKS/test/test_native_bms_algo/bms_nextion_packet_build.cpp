// Native test build wrapper'ı: lib/BmsAlgo/BmsNextionPacket.cpp'yi yalnızca
// bu test suite'i kapsamında derler. cellBarFill() internal linkage
// (anonymous namespace) olduğundan doğrudan çağrılamaz; test_cell_bar_fill.cpp
// onu buildBmsNextionCommands() üzerinden dolaylı olarak kilitler.
#include "BmsNextionPacket.cpp"
