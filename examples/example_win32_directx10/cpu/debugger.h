#pragma once
#include <string>
#include "opcode.h"

namespace debugger {
struct Instruction {
    mips::Opcode opcode;
    std::string mnemonic;
    std::string parameters;
    bool valid = true;

    bool isBranch() const;
};

extern bool mapRegisterNames;
extern bool followPC;
std::string reg(unsigned int n);
Instruction decodeInstruction(mips::Opcode& i);
};  // namespace debugger

typedef uint8_t (*ReadCallBackFn)(const uint8_t* data, size_t off);

struct DebugCtx {
    uint32_t viewMemory;
    uint32_t PC;
    uint32_t EPC;

    uint32_t hi; // What ?
    uint32_t lo;
    uint32_t cause;
    uint32_t status;

    uint32_t reg[32];
    const char* regName[32];
    ReadCallBackFn cbRead;
};

void debuggerWindow(DebugCtx* cpu);
