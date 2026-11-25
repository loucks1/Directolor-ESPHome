#pragma once

// Host stand-in for the version.h ESPHome generates into the build tree.
// The generated header includes esphome/core/macros.h, which defines VERSION_CODE.
#ifndef VERSION_CODE
#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))
#endif

// Default is 2026.11.0 so the host tests compile the get_log_name() path.
// Override with -DESPHOME_VERSION_CODE=$(((2026 << 16) | (10 << 8))) for 2026.10.0.
#ifndef ESPHOME_VERSION_CODE
#define ESPHOME_VERSION "2026.11.0"
#define ESPHOME_VERSION_CODE VERSION_CODE(2026, 11, 0)
#endif
