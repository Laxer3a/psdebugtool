#pragma once
#include <cstdint>
#include <unordered_map>
#include "opcode.h"

struct System;

namespace mips {

struct CPU {
    inline static const int REGISTER_COUNT = 32;
    uint32_t reg[REGISTER_COUNT + 1];
    uint32_t hi, lo;
};
};  // namespace mips
