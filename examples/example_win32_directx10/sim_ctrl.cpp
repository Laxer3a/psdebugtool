#include <assert.h>
#include <string.h>
#include "sim_ctrl.h"

#define DRAIN_QUEUE(q) while (q.size()) { q.pop(); }

//-----------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------
sim_ctrl::sim_ctrl(top_rtl *dut, int port)
{
    m_socket = new tb_server_if(this);
    if (!m_socket->start_server(port))
    {
        fprintf(stderr, "ERROR: Could not open sim_ctrl port %d\n", port);
        exit(-1);
    }

    m_dut       = dut;
    m_io_trace  = false;
    m_pc_trace  = false;
    m_m2p_trace = 0;
    m_p2m_trace = 0;
}
//-----------------------------------------------------------------
// write32
//-----------------------------------------------------------------
void sim_ctrl::write32(uint32_t addr, uint32_t data)
{
    switch (addr)
    {
        case SIMCTRL_WAVES:
            if (data)
                m_dut->enable_waves();
            break;
        case SIMCTRL_IO_TRACE_EN:
            m_io_trace = data & 1;
            if (!m_io_trace)
                DRAIN_QUEUE(m_io_trace_q);
            break;
        case SIMCTRL_PC_TRACE_EN:
            m_pc_trace = data & 1;
            if (!m_pc_trace)
                DRAIN_QUEUE(m_pc_trace_q);
            break;
        case SIMCTRL_M2P_TRACE_EN:
            m_m2p_trace = data;
            if (!m_m2p_trace)
                DRAIN_QUEUE(m_m2p_trace_q);
            break;
        case SIMCTRL_P2M_TRACE_EN:
            m_p2m_trace = data;
            if (!m_p2m_trace)
                DRAIN_QUEUE(m_p2m_trace_q);
            break;
    }
}
//-----------------------------------------------------------------
// read32
//-----------------------------------------------------------------
uint32_t sim_ctrl::read32(uint32_t addr)
{
    switch (addr)
    {
        case SIMCTRL_CYCLE_NUM_L:
            return m_dut->get_cycle() >> 0;
        case SIMCTRL_CYCLE_NUM_H:
            return m_dut->get_cycle() >> 32;
        case SIMCTRL_PC:
        {
            uint32_t pc;
            uint32_t opc;
            m_dut->cpu_trace(pc, opc);
            return pc;
        }
        case SIMCTRL_IO_TRACE_EN:
            return m_io_trace;
        case SIMCTRL_IO_TRACE_SIZE:
            return m_io_trace_q.size();
        case SIMCTRL_PC_TRACE_EN:
            return m_pc_trace;
        case SIMCTRL_PC_TRACE_SIZE:
            return m_pc_trace_q.size();
        case SIMCTRL_PC_TRACE_ENTRY:
        {
            uint32_t pc = m_pc_trace_q.front();
            m_pc_trace_q.pop();
            return pc;
        }
        case SIMCTRL_M2P_TRACE_EN:
            return m_m2p_trace;
        case SIMCTRL_M2P_TRACE_SIZE:
            return m_m2p_trace_q.size();
        case SIMCTRL_P2M_TRACE_EN:
            return m_p2m_trace;
        case SIMCTRL_P2M_TRACE_SIZE:
            return m_p2m_trace_q.size();
    }
}
void sim_ctrl::write(uint32_t addr, uint8_t *data, int length) { assert(!"Not supported"); }
void sim_ctrl::read(uint32_t addr, uint8_t *data, int length)
{
    if (addr == SIMCTRL_IO_TRACE_ENTRY)
    {
        assert((length % sizeof(t_io_access)) == 0);
        while (length)
        {
            t_io_access entry = m_io_trace_q.front();

            memcpy(data, &entry, sizeof(t_io_access));
            data += sizeof(t_io_access);

            m_io_trace_q.pop();
            length -= sizeof(t_io_access);
        }
    }
    else if (addr == SIMCTRL_PC_TRACE_ENTRY)
    {
        assert((length % sizeof(uint32_t)) == 0);
        while (length)
        {
            uint32_t entry = m_pc_trace_q.front();

            memcpy(data, &entry, sizeof(uint32_t));
            data += sizeof(uint32_t);

            m_pc_trace_q.pop();
            length -= sizeof(uint32_t);
        }
    }
    else if (addr == SIMCTRL_MIPS_CTX)
    {
        assert(length == sizeof(t_mips_ctx));
        t_mips_ctx ctx;

        ctx.pc     = m_dut->cpu_pc();
        ctx.status = 0; // TODO:
        ctx.cause  = 0; // TODO:
        ctx.lo     = 0; // TODO:
        ctx.hi     = 0; // TODO:
        ctx.epc    = 0; // TODO:
        for (int i=1;i<32;i++)
            ctx.reg[i] = m_dut->cpu_reg(i);
        memcpy(data, &ctx, sizeof(t_mips_ctx));
    }
    else if (addr == SIMCTRL_M2P_TRACE_ENTRY)
    {
        assert((length % sizeof(t_dma_access)) == 0);
        while (length)
        {
            t_dma_access entry = m_m2p_trace_q.front();

            memcpy(data, &entry, sizeof(t_dma_access));
            data += sizeof(t_dma_access);

            m_m2p_trace_q.pop();
            length -= sizeof(t_dma_access);
        }
    }
    else if (addr == SIMCTRL_P2M_TRACE_ENTRY)
    {
        assert((length % sizeof(t_dma_access)) == 0);
        while (length)
        {
            t_dma_access entry = m_p2m_trace_q.front();

            memcpy(data, &entry, sizeof(t_dma_access));
            data += sizeof(t_dma_access);

            m_p2m_trace_q.pop();
            length -= sizeof(t_dma_access);
        }
    }
    else
        assert(!"Not supported");
}

//-----------------------------------------------------------------
// process
//-----------------------------------------------------------------
void sim_ctrl::process(void)
{
    m_socket->process();

    // IO access log
    if (m_io_trace)
    {
        uint32_t addr;
        bool     we;
        uint8_t  mask;
        uint32_t wr_data;
        uint32_t rd_data;

        if (m_dut->trace_io_access(addr, we, mask, wr_data, rd_data))
        {
            t_io_access entry;
            entry.cycle = m_dut->get_cycle();
            entry.addr  = addr;
            entry.data  = we ? wr_data : rd_data;
            entry.mask  = mask;
            entry.flags = we ? IOLOG_IS_WR : 0;

            m_io_trace_q.push(entry);

            // Drop data if queue size becomes excessive
            if (m_io_trace_q.size() > 10000)
                m_io_trace_q.pop();
        }
    }

    // PC access log
    if (m_pc_trace)
    {
        uint32_t pc, opc;
        if (m_dut->cpu_trace(pc, opc))
        {
            m_pc_trace_q.push(pc);

            // Drop data if queue size becomes excessive
            if (m_pc_trace_q.size() > 10000)
                m_pc_trace_q.pop();
        }
    }

    // M2P access log
    if (m_m2p_trace)
    {
        uint32_t data;
        uint8_t  mask;

        if (m_dut->trace_dma_m2p_access(data, mask))
        {
            t_dma_access entry;
            entry.cycle      = m_dut->get_cycle();
            entry.data       = data;
            entry.ch_bitmap  = mask;

            if (mask & m_m2p_trace)
                m_m2p_trace_q.push(entry);

            // Drop data if queue size becomes excessive
            if (m_m2p_trace_q.size() > 10000)
                m_m2p_trace_q.pop();
        }
    }

    // P2M access log
    if (m_p2m_trace)
    {
        uint32_t data;
        uint8_t  mask;

        if (m_dut->trace_dma_p2m_access(data, mask))
        {
            t_dma_access entry;
            entry.cycle      = m_dut->get_cycle();
            entry.data       = data;
            entry.ch_bitmap  = mask;

            if (mask & m_m2p_trace)
                m_p2m_trace_q.push(entry);

            // Drop data if queue size becomes excessive
            if (m_p2m_trace_q.size() > 10000)
                m_p2m_trace_q.pop();
        }
    }
}