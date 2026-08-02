#!/bin/bash
set -e

echo "Setting up mock include paths..."
mkdir -p mocks/esphome/components
cp -r ../components/directolor_radio mocks/esphome/components/

echo "Compiling tests..."

g++ -std=c++14 -I./mocks \
    test_directolor_radio.cpp \
    ../components/directolor_radio/directolor_radio.cpp \
    ../components/directolor_radio/payload_queue.cpp \
    -o test_runner

echo "Compilation successful. Running tests:"
./test_runner
