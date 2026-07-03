#include "OfflineBuffer.h"

static TelemetryData s_buf[OB_CAPACITY];
static int s_head  = 0;
static int s_tail  = 0;
static int s_count = 0;

void ob_reset() {
    s_head  = 0;
    s_tail  = 0;
    s_count = 0;
}

bool ob_push(const TelemetryData& data) {
    if (s_count == OB_CAPACITY) {
        // En eskiyi düşür: head'i ilerlet, count geçici olarak azalt
        s_head = (s_head + 1) % OB_CAPACITY;
        s_count--;
    }
    s_buf[s_tail] = data;
    s_tail = (s_tail + 1) % OB_CAPACITY;
    s_count++;
    return true;
}

bool ob_pop(TelemetryData& out) {
    if (s_count == 0) return false;
    out    = s_buf[s_head];
    s_head = (s_head + 1) % OB_CAPACITY;
    s_count--;
    return true;
}

bool ob_peek(TelemetryData& out) {
    if (s_count == 0) return false;
    out = s_buf[s_head];
    return true;
}

bool ob_drop_front() {
    if (s_count == 0) return false;
    s_head = (s_head + 1) % OB_CAPACITY;
    s_count--;
    return true;
}

int ob_count() {
    return s_count;
}

bool ob_is_empty() {
    return s_count == 0;
}
