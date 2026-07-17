// Native test build wrapper'ı (RELAY_ROLES_ASSIGNED=1 varyantı):
// lib/VcuLogic/VcuLogic.cpp'yi yalnızca bu test suite'i kapsamında derler —
// test_native_vcu_logic/vcu_logic_build.cpp ile aynı desen; fark, bu suite'in
// [env:native_roles] altında -D RELAY_ROLES_ASSIGNED=1 ile derlenmesidir.
#include "VcuLogic.cpp"
