#!/bin/bash
set -e

echo "Setting up mock include paths..."
mkdir -p mocks/esphome/components
cp -r ../components/directolor_radio mocks/esphome/components/

compile_tests() {
    g++ -std=c++14 -Werror=deprecated -I./mocks \
        "$@" \
        test_directolor_radio.cpp \
        ../components/directolor_radio/directolor_radio.cpp \
        ../components/directolor_radio/directolor_packet.cpp \
        ../components/directolor_radio/payload_queue.cpp \
        ../components/directolor_cover/directolor_cover.cpp \
        -o test_runner
}

# Mock version.h defaults to 2026.11.0. get_name() is deprecated there, so
# -Werror=deprecated fails the build if the legacy logging path is compiled.
echo "Compiling tests (ESPHome >= 2026.11 entity log names)..."
compile_tests

echo "Compilation successful. Running tests:"
./test_runner

# VERSION_CODE(2026, 10, 0). The mock omits get_log_name() below 2026.11, so
# this fails if the new logging path is still compiled.
legacy_version_code=$(( (2026 << 16) | (10 << 8) | 0 ))
echo "Compiling tests (ESPHome 2026.10 legacy entity names, ESPHOME_VERSION_CODE=${legacy_version_code})..."
compile_tests -DESPHOME_VERSION_CODE="${legacy_version_code}"

echo "Legacy compilation successful. Running tests:"
./test_runner
