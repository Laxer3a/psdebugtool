#ifndef SIM_CTRL_H
#define SIM_CTRL_H

#include <stdio.h>
#include <stdint.h>
#include <queue>

#include "tb_server_if.h"
#include "top_rtl.h"
#include "sim_ctrl_types.h"

//-------------------------------------------------------------
// sim_ctrl: Simulation control server
//-------------------------------------------------------------
class sim_ctrl: public tb_driver_api
{
public:
    // Constructor
    sim_ctrl(top_rtl *dut, int port);

    void      process(void);

    // tb_driver_api
    void      write32(uint32_t addr, uint32_t data);
    uint32_t  read32(uint32_t addr);
    void      write(uint32_t addr, uint8_t *data, int length);
    void      read(uint32_t addr, uint8_t *data, int length);

protected:
    top_rtl      *m_dut;
    tb_server_if *m_socket;

    bool          m_io_trace;
    std::queue  <t_io_access> m_io_trace_q;

    bool          m_pc_trace;
    std::queue  <uint32_t>    m_pc_trace_q;

    uint8_t       m_m2p_trace;
    std::queue  <t_dma_access> m_m2p_trace_q;

    uint8_t       m_p2m_trace;
    std::queue  <t_dma_access> m_p2m_trace_q;
};

#endif