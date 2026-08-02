#pragma once
#include <cstdint>
#include <string>

unsigned long millis();

namespace esphome {
    class Component {
    public:
        virtual void setup() {}
        virtual void loop() {}
        virtual void dump_config() {}
    };
}
