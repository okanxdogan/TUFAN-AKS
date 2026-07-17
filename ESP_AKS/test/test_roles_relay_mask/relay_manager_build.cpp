// Native test build wrapper'ı (RELAY_ROLES_ASSIGNED=1 varyantı):
// lib/RelayManager/RelayManager.cpp yalnızca bu suite kapsamında derlenir
// (test_native_relay/relay_manager_build.cpp deseni) — [env:native_roles]
// -D RELAY_ROLES_ASSIGNED=1 bayrağıyla, bank maskesi 0x1FF olur.
#include "RelayManager.cpp"
