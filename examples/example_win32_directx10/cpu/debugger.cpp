#include "debugger.h"
#include <fmt/core.h>
#include <array>

namespace debugger {
bool mapRegisterNames = true;
bool followPC = true;

// clang-format off
	std::array<const char*, 32> regNames = {
		"zero",
		"at",
		"v0", "v1",
		"a0", "a1", "a2", "a3",
		"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
		"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
		"t8", "t9",
		"k0", "k1",
		"gp",
		"sp",
		"fp",
		"ra",
	};

    std::array<const char*, 16> cop0RegNames = {
        "cop0r0?",   // r0
        "cop0r1?",   // r1
        "cop0r2?",   // r2
        "bpc",       // r3
        "cop0r4?",   // r4
        "bda",       // r5
        "tar",       // r6
        "dcic",      // r7
        "bada",      // r8
        "bdam",      // r9
        "cop0r10?",  // r10
        "bpcm",      // r11
        "status",    // r12
        "cause",     // r13
        "epc",       // r14
        "prid",      // r15
    };

    std::array<const char*, 64> cop2RegNames = {
        // Data
        "vxy0",      // r0
        "vz0",       // r1
        "vxy1",      // r2
        "vz1",       // r3
        "vxy2",      // r4
        "vz2",       // r5
        "rgbc",      // r6
        "otz",       // r7
        "ir0",       // r8
        "ir1",       // r9
        "ir2",       // r10
        "ir3",       // r11
        "sxy0",      // r12
        "sxy1",      // r13
        "sxy2",      // r14
        "sxyp",      // r15
        "sz0",       // r16
        "sz1",       // r17
        "sz2",       // r18
        "sz3",       // r19
        "rgb0",      // r20
        "rgb1",      // r21
        "rgb2",      // r22
        "res1",      // r23
        "mac0",      // r24
        "mac1",      // r25
        "mac2",      // r26
        "mac3",      // r27
        "irgb",      // r28
        "orgb",      // r29
        "lzcs",      // r30
        "lzcr",      // r31
        // Control
        "rt11-rt12", // r32
        "rt13-rt21", // r33
        "rt22-rt23", // r34
        "rt31-rt32", // r35
        "rt33",      // r36
        "trx",       // r37
        "try",       // r38
        "trz",       // r39
        "l11-l12",   // r40
        "l13-l21",   // r41
        "l22-l23",   // r42
        "l31-l32",   // r43
        "l33",       // r44
        "rbk",       // r45
        "gbk",       // r46
        "bbk",       // r47
        "lr1-lr2",   // r48
        "lr3-lg1",   // r49
        "lg2-lg3",   // r50
        "lb1-lb2",   // r51
        "lb3",       // r52
        "rfc",       // r53
        "gfc",       // r54
        "bfc",       // r55
        "ofx",       // r56
        "ofy",       // r57
        "h",         // r58
        "dqa",       // r59
        "dqb",       // r60
        "zsf3",      // r61
        "zsf4",      // r62
        "flag",      // r63
    };
// clang-format on

bool Instruction::isBranch() const {
    auto sub = (opcode.rt & 0x11);
    if (opcode.op == 1 && (sub == 0 || sub == 1 || sub == 16 || sub == 17)) return true;
    if (opcode.op == 4 || opcode.op == 5 || opcode.op == 6 || opcode.op == 7) return true;
    return false;
}

std::string reg(unsigned int n) {
    if (mapRegisterNames) return regNames[n];
    return fmt::format("r{}", n);
}

std::string cop0reg(unsigned int n) {
    if (mapRegisterNames && n < cop0RegNames.size()) return cop0RegNames.at(n);
    return fmt::format("cop0r{}", n);
}

std::string cop2reg(unsigned int n) {
    if (mapRegisterNames && n < cop2RegNames.size()) return cop2RegNames.at(n);
    return fmt::format("cop2r{}", n);
}

Instruction mapSpecialInstruction(mips::Opcode& i);

std::string hexWithSign(int16_t hex) {
    if (hex >= 0) {
        return fmt::format("0x{:04x}", static_cast<uint16_t>(hex));
    }

    return fmt::format("-0x{:04x}", static_cast<uint16_t>(hex * -1));
}

Instruction decodeInstruction(mips::Opcode& i) {
    Instruction ins;
    ins.opcode = i;

#define U16(x) static_cast<unsigned short>(x)

#define R(x) reg(x).c_str()
#define LOADTYPE fmt::format("{}, {}({})", R(i.rt), hexWithSign(i.offset), R(i.rs));
#define ITYPE fmt::format("{}, {}, 0x{:04x}", R(i.rt), R(i.rs), U16(i.offset))
#define JTYPE fmt::format("0x{:x}", i.target * 4)
#define O(n, m, d)                     \
    case n:                            \
        ins.mnemonic = std::string(m); \
        ins.parameters = d;            \
        break

#define BRANCH_TYPE fmt::format("{}, {}", R(i.rs), U16(i.offset))

    switch (i.op) {
        case 0: ins = mapSpecialInstruction(i); break;

        case 1:
            switch (i.rt & 0x11) {
                O(0, "bltz", BRANCH_TYPE);
                O(1, "bgez", BRANCH_TYPE);
                O(16, "bltzal", BRANCH_TYPE);
                O(17, "bgezal", BRANCH_TYPE);

                default:
                    ins.mnemonic = fmt::format("0x{:08x}", i.opcode);
                    ins.valid = false;
                    break;
            }
            break;

            O(2, "j", JTYPE);
            O(3, "jal", JTYPE);
            O(4, "beq", ITYPE);
            O(5, "bne", ITYPE);
            O(6, "blez", BRANCH_TYPE);
            O(7, "bgtz", BRANCH_TYPE);

            O(8, "addi", ITYPE);
            O(9, "addiu", ITYPE);
            O(10, "slti", ITYPE);
            O(11, "sltiu", ITYPE);
            O(12, "andi", ITYPE);
            O(13, "ori", ITYPE);
            O(14, "xori", ITYPE);
            O(15, "lui", fmt::format("{}, 0x{:04x}", R(i.rt), U16(i.offset)));

        case 16:
            switch (i.rs) {
                case 0:
                    ins.mnemonic = std::string("mfc0");
                    ins.parameters = fmt::format("{}, {}", R(i.rt), cop0reg(i.rd).c_str());
                    ins.valid = i.rd < 16;
                    break;
                case 4:
                    ins.mnemonic = std::string("mtc0");
                    ins.parameters = fmt::format("{}, {}", R(i.rt), cop0reg(i.rd).c_str());
                    ins.valid = i.rd < 16;
                    break;
                    O(16, "rfe", "");
                default:
                    ins.mnemonic = fmt::format("cop0 - 0x{:08x}", i.opcode);
                    ins.valid = false;
                    break;
            }
            break;
            O(17, "cop1", "");

        case 18:
            switch (i.rs) {
                O(0, "mfc2", fmt::format("{}, {}", R(i.rt), cop2reg(i.rd).c_str()));
                O(2, "cfc2", fmt::format("{}, {}", R(i.rt), cop2reg(i.rd + 32).c_str()));
                O(4, "mtc2", fmt::format("{}, {}", R(i.rt), cop2reg(i.rd).c_str()));
                O(6, "ctc2", fmt::format("{}, {}", R(i.rt), cop2reg(i.rd + 32).c_str()));
                default:
                    ins.mnemonic = fmt::format("cop2 - 0x{:08x}", i.opcode);
                    ins.valid = false;
                    break;
            }
            break;
            O(19, "cop3", "");

            O(32, "lb", LOADTYPE);
            O(33, "lh", LOADTYPE);
            O(34, "lwl", LOADTYPE);
            O(35, "lw", LOADTYPE);
            O(36, "lbu", LOADTYPE);
            O(37, "lhu", LOADTYPE);
            O(38, "lwr", LOADTYPE);

            O(40, "sb", LOADTYPE);
            O(41, "sh", LOADTYPE);
            O(42, "swl", LOADTYPE);
            O(43, "sw", LOADTYPE);
            O(46, "swr", LOADTYPE);

            // TODO: Add dummy instructions
            O(50, "lwc2", "");  // TODO: add valid prototype
            O(58, "swc2", "");  // TODO: add valid prototype

        default:
            ins.mnemonic = fmt::format("0x{:08x}", i.opcode);
            ins.valid = false;
            break;
    }
    return ins;
}

Instruction mapSpecialInstruction(mips::Opcode& i) {
    Instruction ins;
    ins.opcode = i;
#define SHIFT_TYPE fmt::format("{}, {}, {}", R(i.rd), R(i.rt), (int)i.sh)
#define ATYPE fmt::format("{}, {}, {}", R(i.rd), R(i.rs), R(i.rt))

    switch (i.fun) {
        case 0:
            if (i.rt == 0 && i.rd == 0 && i.sh == 0) {
                ins.mnemonic = "nop";
            } else {
                ins.mnemonic = "sll";
                ins.parameters = SHIFT_TYPE;
            }
            break;

            O(2, "srl", SHIFT_TYPE);
            O(3, "sra", SHIFT_TYPE);
            O(4, "sllv", SHIFT_TYPE);
            O(6, "srlv", SHIFT_TYPE);
            O(7, "srav", SHIFT_TYPE);

            O(8, "jr", fmt::format("{}", R(i.rs)));
            O(9, "jalr", fmt::format("{}, {}", R(i.rd), R(i.rs)));
            O(12, "syscall", "");
            O(13, "break", "");

            O(16, "mfhi", fmt::format("{}", R(i.rd)));
            O(17, "mthi", fmt::format("{}", R(i.rs)));
            O(18, "mflo", fmt::format("{}", R(i.rd)));
            O(19, "mtlo", fmt::format("{}", R(i.rs)));

            O(24, "mult", fmt::format("{}, {}", R(i.rs), R(i.rt)));
            O(25, "multu", fmt::format("{}, {}", R(i.rs), R(i.rt)));
            O(26, "div", fmt::format("{}, {}", R(i.rs), R(i.rt)));
            O(27, "divu", fmt::format("{}, {}", R(i.rs), R(i.rt)));

            O(32, "add", ATYPE);
            O(33, "addu", ATYPE);
            O(34, "sub", ATYPE);
            O(35, "subu", ATYPE);
            O(36, "and", ATYPE);
            O(37, "or", ATYPE);
            O(38, "xor", ATYPE);
            O(39, "nor", ATYPE);
            O(42, "slt", ATYPE);
            O(43, "sltu", ATYPE);

        default:
            ins.mnemonic = fmt::format("special 0x{:02x}", (int)i.fun);
            ins.valid = false;
            break;
    }
    return ins;
}
};  // namespace debugger

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

template <typename T>
constexpr uint32_t align_mips(uint32_t address) {
    static_assert(std::is_same<T, uint8_t>() || std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>(), "Invalid type used");

    if (sizeof(T) == 1) return address & 0x1fffffff;
    if (sizeof(T) == 2) return address & 0x1ffffffe;
    if (sizeof(T) == 4) return address & 0x1ffffffc;
    return 0;
}

template <uint32_t base, uint32_t size>
constexpr bool in_range(const uint32_t addr) {
    return (addr >= base && addr < base + size);
}

struct System {
    enum class State {
        halted,  // Cannot be run until reset
        stop,    // after reset
        pause,   // if debugger attach
        run      // normal state
    };

    static const int BIOS_BASE = 0x1fc00000;
    static const int RAM_BASE = 0x00000000;
    static const int SCRATCHPAD_BASE = 0x1f800000;
    static const int EXPANSION_BASE = 0x1f000000;
    static const int IO_BASE = 0x1f801000;

    static const int BIOS_SIZE = 512 * 1024;
    static const int RAM_SIZE = 2 * 1024 * 1024;
    static const int SCRATCHPAD_SIZE = 1024;
    static const int EXPANSION_SIZE = 1 * 1024 * 1024;
    static const int IO_SIZE = 0x2000;
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

#include <vector>
#include "fmt/core.h"
#include "imgui.h"

uint32_t readFromMemory(DebugCtx* cpu, uint32_t addr) {
    uint8_t a = cpu->cbRead(NULL/*Do not own the buffer*/,addr  );
    uint8_t b = cpu->cbRead(NULL/*Do not own the buffer*/,addr+1);
    uint8_t c = cpu->cbRead(NULL/*Do not own the buffer*/,addr+2);
    uint8_t d = cpu->cbRead(NULL/*Do not own the buffer*/,addr+3);

    return a | (b<<8) | (c<<16) | (d<<24);
}

int clamp(int v, int minv, int maxv) {
    if (v < minv) { v = minv; }
    if (v > maxv) { v = maxv; }
    return v;
}

void debuggerWindow(DebugCtx* cpu) {
    static bool debuggerWindowOpen;
    static bool followPC;
    static uint32_t  prevPC = -1;

    bool goToPc = false;
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    ImGui::Begin("Debugger", &debuggerWindowOpen, ImGuiWindowFlags_NoScrollbar);

//    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
    /*
    if (ImGui::Button(sys->state == System::State::run ? "Pause" : "Run")) {
        if (sys->state == System::State::run)
            sys->state = System::State::pause;
        else
            sys->state = System::State::run;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step in")) {
        sys->singleStep();
    }
    ImGui::SameLine();
    if (ImGui::Button("Step over")) {
        sys->cpu->addBreakpoint(sys->cpu->PC + 4);
        sys->state = System::State::run;
    }
    ImGui::SameLine();
    */
    if (ImGui::Button("Go to PC")) {
        cpu->viewMemory = cpu->PC;
        goToPc = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Go to Exception PC")) {
        cpu->viewMemory = cpu->EPC;
        goToPc = true;
    }

    /*
    ImGui::SameLine();
    ImGui::Checkbox("Follow PC", &debugger::followPC);
    */
    static bool mapRegisterNames = true;
    ImGui::SameLine();
    ImGui::Checkbox("Map register names", &mapRegisterNames);

    ImGui::Separator();

    auto glyphSize = ImGui::CalcTextSize("F").x + 1;  // We assume the font is mono-space
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const int col_num = 4;
        std::vector<float> columnsWidth(col_num);
        unsigned int n = 0;

        auto column = [&](const char* reg, uint32_t val) {
            auto width = glyphSize * 13 + 8;
            if (width > columnsWidth[n]) columnsWidth[n] = width;

            ImGui::TextUnformatted(reg);
            ImGui::SameLine();

            auto color = ImVec4(1.f, 1.f, 1.f, (val == 0) ? 0.25f : 1.f);
            ImGui::TextColored(color, "0x%08x", val);
            ImGui::NextColumn();

            if (++n >= columnsWidth.size()) n = 0;
        };

        ImGui::Columns(col_num, nullptr, false);

        column(" pc: ", cpu->PC);
        column("epc: ", cpu->EPC);
        column(" hi: ", cpu->hi);
        column(" lo: ", cpu->lo);

        ImGui::NextColumn();
        for (int i = 1; i < 32; i++) {
            column(fmt::format("{:>3}: ", debugger::reg(i)).c_str(), cpu->reg[i]);
        }

        for (int c = 0; c < col_num; c++) {
            ImGui::SetColumnWidth(c, columnsWidth[c]);
        }
        ImGui::Columns(1);
        ImGui::PopStyleVar();
    }

    ImGui::NewLine();

    ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    auto segment = Segment::fromAddress(cpu->viewMemory);  // <-- do that only when following pc
    ImGuiListClipper clipper(segment.size / 4);

    uint32_t base = segment.base & ~0xe0000000;
    base |= cpu->viewMemory & 0xe0000000;

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            uint32_t address = base + i * 4;

            mips::Opcode opcode(readFromMemory(cpu,address));
            auto disasm = debugger::decodeInstruction(opcode);

            int xStart = glyphSize * 3;
            if (disasm.isBranch()) {
                auto color = IM_COL32(255, 255, 255, 192);
                auto lineHeight = ImGui::GetTextLineHeight();
                int16_t branchOffset = disasm.opcode.offset;

                int xEnd = xStart - clamp(abs(branchOffset), (int)glyphSize, xStart);

                ImVec2 src = ImGui::GetCursorScreenPos();
                src.y += lineHeight / 2;
                // Compensate for Branch Delay
                src.y += lineHeight;

                ImVec2 dst = src;
                dst.y += branchOffset * lineHeight;

                // From
                drawList->AddLine(ImVec2(src.x + xStart, src.y), ImVec2(src.x + xEnd, src.y), color);

                // Vertical line
                drawList->AddLine(ImVec2(src.x + xEnd, src.y), ImVec2(dst.x + xEnd, dst.y), color);

                // To
                drawList->AddLine(ImVec2(dst.x + xStart, dst.y), ImVec2(dst.x + xEnd, dst.y), color);

                // Arrow
                drawList->AddTriangleFilled(ImVec2(dst.x + xStart, dst.y), ImVec2(dst.x + xStart - 3, dst.y - 3),
                                            ImVec2(dst.x + xStart - 3, dst.y + 3), color);
            }

            /*
            bool breakpointActive = cpu->breakpoints.find(address) != cpu->breakpoints.end();
            if (breakpointActive) {
                const float size = 4.f;
                ImVec2 src = ImGui::GetCursorScreenPos();
                src.x += size;
                src.y += ImGui::GetTextLineHeight() / 2;
                drawList->AddCircleFilled(src, size, IM_COL32(255, 0, 0, 255));
            }
            */

            bool isCurrentPC = address == cpu->PC;
            bool isCurrentPCE = address == cpu->EPC;
            if (isCurrentPC) {
                auto color = IM_COL32(255, 255, 0, 255);
                const float size = 4;
                ImVec2 src = ImGui::GetCursorScreenPos();
                src.y += ImGui::GetTextLineHeight() / 2;

                // Arrow
                drawList->AddTriangleFilled(ImVec2(src.x + xStart, src.y), ImVec2(src.x + xStart - size, src.y - size),
                                            ImVec2(src.x + xStart - size, src.y + size), color);

                // Line
                drawList->AddRectFilled(ImVec2(src.x + xStart - size * 3, src.y + size / 2),
                                        ImVec2(src.x + xStart - size, src.y - size / 2), color);
            }

            const char* comment = "";
            ImU32 color = IM_COL32(255, 255, 255, 255);
            if (isCurrentPC) {
                color = IM_COL32(255, 255, 0, 255);
            } else if (isCurrentPCE) {
                color = IM_COL32(0, 255, 255, 255);
            } else /* if (breakpointActive) {
                color = IM_COL32(255, 0, 0, 255);
            } else */ if (!disasm.valid) {
                comment = "; invalid instruction";
                color = IM_COL32(255, 255, 255, 64);
            } else if (disasm.opcode.opcode == 0) {  // NOP
                color = IM_COL32(255, 255, 255, 64);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            auto line
                = fmt::format("{} {:{}c} {}", disasm.mnemonic, ' ', std::max(0, 6 - (int)disasm.mnemonic.length()), disasm.parameters);
            if (ImGui::Selectable(fmt::format("    {}:0x{:08x}: {} {:{}c} {}", segment.name, address, line, ' ',
                                              std::max(0, 25 - (int)line.length()), comment)
                                      .c_str())) {
            /*
                auto bp = cpu->breakpoints.find(address);
                if (bp == cpu->breakpoints.end()) {
                    cpu->addBreakpoint(address, {});
                } else {
                    cpu->removeBreakpoint(address);
                }
            */
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGui::GetIO().MouseClicked[1])) {
//              ImGui::OpenPopup("##instruction_options");
//              contextMenuAddress = address;
            }

            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%02x %02x %02x %02x", opcode.opcode & 0xff, (opcode.opcode >> 8) & 0xff, (opcode.opcode >> 16) & 0xff,
                                  (opcode.opcode >> 24) & 0xff);
            }
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::EndChild();

#if 0
    if (ImGui::BeginPopupContextItem("##instruction_options")) {
        auto bp = sys->cpu->breakpoints.find(contextMenuAddress);
        auto breakpointExist = bp != sys->cpu->breakpoints.end();

        /*
        if (ImGui::Selectable("Run to line")) {
            sys->cpu->addBreakpoint(contextMenuAddress);
            sys->state = System::State::run;
        }
        */

        if (breakpointExist && ImGui::Selectable("Remove breakpoint")) sys->cpu->removeBreakpoint(contextMenuAddress);
        if (!breakpointExist && ImGui::Selectable("Add breakpoint")) sys->cpu->addBreakpoint(contextMenuAddress, {});

        ImGui::EndPopup();
    }
#endif

    ImGui::Text("Go to address ");
    ImGui::SameLine();

    ImGui::PushItemWidth(80.f);

    bool doScroll = false;
    uint32_t goToAddr = 0;

    static char addrInputBuffer[256];

    if (ImGui::InputTextWithHint("##addr", "Hex address", (char*)addrInputBuffer, 8 + 1,
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        goToAddr = std::stoul(addrInputBuffer, 0, 16);

        auto segment = Segment::fromAddress(goToAddr);  // <-- do that only when following pc
        base = segment.base & ~0xe0000000;
        base |= goToAddr & 0xe0000000;


        cpu->viewMemory = goToAddr;

        goToAddr = (goToAddr - base) / 4;
        doScroll = true;
        debugger::followPC = false;
        addrInputBuffer[0] = 0;
        // TODO: Change segment if necessary
    }
    ImGui::PopItemWidth();

    if (goToPc) {
        goToAddr = ((cpu->viewMemory - base) / 4);
        doScroll = true;
    }

    if (doScroll) {
        float px = goToAddr * ImGui::GetTextLineHeight();
        ImGui::BeginChild("##scrolling");
        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + px);
        ImGui::EndChild();
    }

//    ImGui::PopFont();

    ImGui::End();
}
