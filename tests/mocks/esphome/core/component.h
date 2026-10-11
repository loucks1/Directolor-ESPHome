#pragma once
#include <cstdint>

namespace esphome {
class Component {
public:
    virtual ~Component() = default;
    virtual void setup() {}
    virtual void loop() {}
    virtual void dump_config() {}

    template <typename Func>
    void set_timeout(const char *name, uint32_t timeout, Func &&func)
    {
        (void)name;
        (void)timeout;
        (void)func;
    }
    void cancel_timeout(const char *name) { (void)name; }
};
} // namespace esphome
