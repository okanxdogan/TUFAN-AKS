#!/usr/bin/env bash
# can_replay host aracini derler. Gercek lib/CanParse/CanParse.cpp'yi,
# native testlerin kullandigi ayni IDF stub'lariyla (test/support/idf_stubs)
# linkler — donanim gerekmez.
set -euo pipefail
cd "$(dirname "$0")/../.."  # ESP_AKS koku

g++ -std=gnu++17 -Wall -Wextra -O2 \
    -I include \
    -I lib/CanParse \
    -I lib/Telemetry \
    -I test/support/idf_stubs \
    lib/CanParse/CanParse.cpp \
    tools/can_replay/can_replay.cpp \
    -o tools/can_replay/can_replay

echo "OK: tools/can_replay/can_replay"
