#include "cpu.h"
#include <fmt/core.h>
#include "debugger.h"

struct Segment {
    const char* name;
    uint32_t base;
    uint32_t size;

    bool inRange(uint32_t addr) { return addr >= base && addr < base + size; }

    static Segment fromAddress(uint32_t);
};

namespace segments {
Segment RAM = {"RAM", System::RAM_BASE, System::RAM_SIZE};
Segment EXPANSION = {"EXPANSION", System::EXPANSION_BASE, System::EXPANSION_SIZE};
Segment SCRATCHPAD = {"SCRATCHPAD", System::SCRATCHPAD_BASE, System::SCRATCHPAD_SIZE};
Segment IO = {"IO", System::IO_BASE, System::IO_SIZE};
Segment BIOS = {"BIOS", System::BIOS_BASE, System::BIOS_SIZE};
Segment IOCONTROL = {"IOCONTROL", 0xfffe0130, 4};
Segment UNKNOWN = {"UNKNOWN", 0, 0};
};  // namespace segments

Segment Segment::fromAddress(uint32_t address) {
    uint32_t addr = align_mips<uint32_t>(address);

    if (addr >= segments::RAM.base && addr < segments::RAM.base + segments::RAM.size * 4) {
        return segments::RAM;
    }
    if (segments::EXPANSION.inRange(addr)) {
        return segments::EXPANSION;
    }
    if (segments::SCRATCHPAD.inRange(addr)) {
        return segments::SCRATCHPAD;
    }
    if (segments::BIOS.inRange(addr)) {
        return segments::BIOS;
    }
    if (segments::IO.inRange(addr)) {
        return segments::IO;
    }
    if (segments::IOCONTROL.inRange(address)) {
        return segments::IOCONTROL;
    }

    return segments::UNKNOWN;
}

std::string formatOpcode(mips::Opcode& opcode) {
    auto disasm = debugger::decodeInstruction(opcode);
    return fmt::format("{} {:{}c} {}", disasm.mnemonic, ' ', std::max(0, 6 - (int)disasm.mnemonic.length()), disasm.parameters);
}
