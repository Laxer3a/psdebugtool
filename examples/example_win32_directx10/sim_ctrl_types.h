#ifndef SIM_CTRL_TYPES_H
#define SIM_CTRL_TYPES_H

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define SIMCTRL_CYCLE_NUM_L     0x00
#define SIMCTRL_CYCLE_NUM_H     0x04
#define SIMCTRL_WAVES           0x08
#define SIMCTRL_PC              0x10
#define SIMCTRL_MIPS_CTX        0x80
#define SIMCTRL_IO_TRACE_EN     0x100
#define SIMCTRL_IO_TRACE_SIZE   0x104
#define SIMCTRL_IO_TRACE_ENTRY  0x110
#define SIMCTRL_PC_TRACE_EN     0x200
#define SIMCTRL_PC_TRACE_SIZE   0x204
#define SIMCTRL_PC_TRACE_ENTRY  0x210
#define SIMCTRL_M2P_TRACE_EN    0x300
#define SIMCTRL_M2P_TRACE_SIZE  0x304
#define SIMCTRL_M2P_TRACE_ENTRY 0x310
#define SIMCTRL_P2M_TRACE_EN    0x400
#define SIMCTRL_P2M_TRACE_SIZE  0x404
#define SIMCTRL_P2M_TRACE_ENTRY 0x410

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
typedef struct io_log_entry
{
    uint64_t cycle;
    uint32_t addr;
    uint32_t data;
    uint32_t mask;
    uint32_t flags;
    #define IOLOG_IS_WR (1 << 0)
} t_io_access;

typedef struct dma_log_entry
{
    uint64_t cycle;
    uint32_t data;
    uint32_t ch_bitmap;
} t_dma_access;

typedef struct mips_ctx
{
    uint32_t pc;
    uint32_t status;
    uint32_t cause;
    uint32_t lo;
    uint32_t hi;
    uint32_t epc;
    uint32_t reg[31];
} t_mips_ctx;

#endif