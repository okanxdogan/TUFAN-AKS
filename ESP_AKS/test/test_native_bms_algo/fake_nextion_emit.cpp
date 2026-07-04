#include "fake_nextion_emit.h"

#include <cstring>

namespace {
// buildBmsNextionCommands tek çağrıda ~75 komut üretir (24 cell + 24 j +
// 24 bal + cellmax/cellmin/warn); pay bırakmak için 128 kapasite yeterli.
constexpr size_t kMaxCommands = 128;
constexpr size_t kMaxCmdLen = 40;
char s_commands[kMaxCommands][kMaxCmdLen];
size_t s_count = 0;
}  // namespace

void fake_nextion_reset() {
    s_count = 0;
}

void fake_nextion_capture(const char* cmd, size_t len, void* /*ctx*/) {
    if (s_count >= kMaxCommands) return;
    size_t copyLen = (len < kMaxCmdLen - 1) ? len : (kMaxCmdLen - 1);
    memcpy(s_commands[s_count], cmd, copyLen);
    s_commands[s_count][copyLen] = '\0';
    ++s_count;
}

size_t fake_nextion_command_count() {
    return s_count;
}

const char* fake_nextion_command_at(size_t index) {
    return s_commands[index];
}

const char* fake_nextion_find(const char* prefix) {
    const size_t prefixLen = strlen(prefix);
    for (size_t i = 0; i < s_count; ++i) {
        if (strncmp(s_commands[i], prefix, prefixLen) == 0) {
            return s_commands[i];
        }
    }
    return nullptr;
}
