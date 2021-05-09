#include "psxevents.h"
#include "helperString.h"

#include <stdio.h>
#include <string.h>

#include "GPUCommandGen.h"

void error() {
    printf("ERROR\n");
}

u8* findEOL(u8* start) {
	// Stop on next char after EOL
	// + patch EOL with 0
	
	// or stop at char zero.
	// [TODO]
    while (*start!=0 && *start!=0xA) {
        start++;
    }

    if (*start == 0xA) {
        *start = 0;
        start++;
    }

    return start;
}

u64 GetTime(u8* start, u64& lastTime) {
	u64 result = ValueIntAfter(start, (u8*)" @ ");
    if ((result != -1) && result > lastTime) {
        lastTime = result;
    }
    return result;
}

void parserSIMLOG(const char* fileName, u64* pLastTime) {
    FILE* file = fopen(fileName, "rb");

    if (!file) { return; }

	// 50 MB, cut in TWO.
	const int FULL = 50*1024*1024;
	const int HALF = FULL / 2;
	
	u8* block = new u8[FULL + 1];
	block[FULL] = 0;
	u8* halfp = &block[HALF];

    fseek(file,0,SEEK_END);	
	u64 size      = ftell(file);
    fseek(file,0,SEEK_SET);

	u32 blocksize = HALF;
	u8* parse     = block;
	
	u64 lastTime  = 0;
	u32 lastAddr  = 0;
	u32 lastMask  = 0;
    int lastDMAchannel = 0;
    DeviceSingleOp* lastDevice;
    DeviceSingleOp* lastDeviceRead;
    DeviceSwitchFlag* lastDMA;
	
	int state = 0;
	int line  = 0;

    u64 finalTime = 0;

    u64 dmaStartTime = 0;
    GPUCommandGen* gpuGen =	getCommandGen();

    fread(block,size < FULL ? size : FULL, 1, file);
    if (size < FULL) {
        block[size] = 0;
        size = 0;
    } else {
        size -= FULL;
    }

	while (true) {
		line++;

		if ((line & 0xFFF) == 0) {
			printf("Parsed line : %i\n",line);
		}

#if 1
        if (line > 1100000) {
            break;
        }
#endif
	
		// [TODO Parse line]
		// 1. Find end of line
		u8* start = parse;
		u8* param;
		parse = findEOL(parse);
		
		// 2. Find sub string within range
		switch (state) {
		case 0:
			if (param = found(start, (u8*)"IO Access (W) [")) {
				// IO Access (W) [DMA]:     1f8010f0 = 0f6f4b21 [mask=f] [delta=5 @ 390044]
				// Find until ] char, get device with same name.
				u8* p = param;
				u8* p2;
				
				u64 time = GetTime(p, finalTime);
				
				u32 addr = ValueHexAfter(p,(u8*)"]:");
				u32 value= ValueHexAfter(p,(u8*)"= ");
				u8  mask = ValueHexAfter(p,(u8*)"mask=");
				
				if (param=found(p,(u8*)"MC1")) {
					((DeviceSingleOp*)getDeviceFromType(MC1_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"JOY")) {
					((DeviceSingleOp*)getDeviceFromType(JOY_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"SIO")) {
					((DeviceSingleOp*)getDeviceFromType(SIO_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"MC2")) {
					((DeviceSingleOp*)getDeviceFromType(MC2_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"IRQ")) {
					((DeviceSingleOp*)getDeviceFromType(IRQ_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"DMA")) {
					((DeviceSingleOp*)getDeviceFromType(DMA_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"TMR")) {
					((DeviceSingleOp*)getDeviceFromType(TMR_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"CDROM")) {
					((DeviceSingleOp*)getDeviceFromType(CDR_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"GPU")) {
					((DeviceSingleOp*)getDeviceFromType(GPU_T))->Write(time, addr, value, mask);
                    if ((addr == 0x1F801814) && ((value>>24) == 0x05)) {
	                    DeviceSingleOp* pBuffSwitch = (DeviceSingleOp*)getDeviceFromType(DISPLAY_SWITCH);
                        pBuffSwitch->Read(time,0,1,4);
                    }
                    gpuGen->setTime(time);
                    if (addr == 0x1F801814) {
                        gpuGen->writeGP1(value);
                    } else {
	                    gpuGen->writeRaw(value);
                    }
				} else
				if (param=found(p,(u8*)"MDEC")) {
					((DeviceSingleOp*)getDeviceFromType(MDEC_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"SPU")) {
					((DeviceSingleOp*)getDeviceFromType(SPU_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"EXP2")) {
					((DeviceSingleOp*)getDeviceFromType(EXP2_T))->Write(time, addr, value, mask);
				} else
				if (param=found(p,(u8*)"ATCONS")) {
					((DeviceSingleOp*)getDeviceFromType(EXP2_T))->Write(time, addr, value, mask);
				} else 
				if (param=found(p,(u8*)"UNKNOWN")) {
					((DeviceSingleOp*)getDeviceFromType(UNKNOWN_T))->Write(time, addr, value, mask);
				} else {
					error();
				}
			} else
			if (param = found(start, (u8*)"IO Access (R) [")) {
				u8* p = param;

				// IO Access (R) [IRQ]:     1f801074 [delta=901 @ 391012]
				// IO Resp: 00000008
				lastAddr = ValueHexAfter(p,(u8*)"]:");
				lastTime = GetTime(p, finalTime);
				lastMask = 0xF; // Not in stream.

				// Find until ] char, get device with same name.
				if (param=found(p,(u8*)"MC1")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(MC1_T));
				} else
				if (param=found(p,(u8*)"JOY")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(JOY_T));
				} else
				if (param=found(p,(u8*)"SIO")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(SIO_T));
				} else
				if (param=found(p,(u8*)"MC2")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(MC2_T));
				} else
				if (param=found(p,(u8*)"IRQ")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(IRQ_T));
				} else
				if (param=found(p,(u8*)"DMA")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(DMA_T));
				} else
				if (param=found(p,(u8*)"TMR")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(TMR_T));
				} else
				if (param=found(p,(u8*)"CDROM")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(CDR_T));
				} else
				if (param=found(p,(u8*)"GPU")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(GPU_T));
				} else
				if (param=found(p,(u8*)"MDEC")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(MDEC_T));
				} else
				if (param=found(p,(u8*)"SPU")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(SPU_T));
				} else
				if (param=found(p,(u8*)"EXP2")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(EXP2_T));
				} else
				if (param=found(p,(u8*)"ATCONS")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(EXP2_T));
				} else 
				if (param=found(p,(u8*)"UNKNOWN")) {
					lastDevice = ((DeviceSingleOp*)getDeviceFromType(UNKNOWN_T));
				} else {
					error();
				}
                lastDeviceRead = lastDevice;
			} else
			if (param = found(start, (u8*)"IO interrupt SRC=")) {
				// IO interrupt SRC=dma @ 390155
				u8* p = param;

				u64 time = GetTime(p, finalTime);
				
				if (param=found(p,(u8*)"gpu_vbl @ ")) {
					((DeviceSingleOp*)getDeviceFromType(VBL_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"gpu_cmd @ ")) {
					((DeviceSingleOp*)getDeviceFromType(GPU_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"cdrom @ ")) {
					((DeviceSingleOp*)getDeviceFromType(CDROM_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"dma @ ")) {
					((DeviceSingleOp*)getDeviceFromType(DMA_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"timer0 @ ")) {
					((DeviceSingleOp*)getDeviceFromType(TIME0_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"timer1 @ ")) {
					((DeviceSingleOp*)getDeviceFromType(TIME1_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"timer2 @ ")) {
					((DeviceSingleOp*)getDeviceFromType(TIME2_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"joy/mem @ ")) {
					((DeviceSingleOp*)getDeviceFromType(JOYMEM_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"sio @ ")) {
					((DeviceSingleOp*)getDeviceFromType(SIO_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"spu @ ")) {
					((DeviceSingleOp*)getDeviceFromType(SPU_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else
				if (param=found(p,(u8*)"lightpen @ ")) {
					((DeviceSingleOp*)getDeviceFromType(LIGHTPEN_IRQ_T))->Write(time, 0, 1, 0xFF);
				} else {
					error();
				}
			} else
			if (param = found(start, (u8*)"[DMA] Activating transfer CH")) {
                lastDMAchannel = param[0] - '0';

				lastDMA = ((DeviceSwitchFlag*)getDeviceFromType((DeviceType)(DMA_CH0_T + lastDMAchannel)));

				// [DMA] Activating CH2 transfer (memory -> peripheral)         20 bytes
				// Ignore fow now...
                dmaStartTime = GetTime(param, finalTime);
                lastDMA->DoEventType(dmaStartTime, EventType::EVENT_BOOL, true);

			} else
			if (param = found(start, (u8*)"[DMA] Transfer complete")) {
				// Ignore fow now...
                dmaStartTime = GetTime(param, finalTime);
                lastDMA->DoEventType(dmaStartTime, EventType::EVENT_BOOL, false);
			} else
			if (param = found(start, (u8*)"M2P]")) {
				u64 time = GetTime(param, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.
                if (time != 0) { dmaStartTime = time; }
                u32 value = ValueHexAfter(param,(u8*)" ");
                switch (lastDMAchannel) {
                case 0:
                case 1:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(MDEC_T)); break;
                case 2:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(GPU_T));
                    gpuGen->setTime(time);
                    gpuGen->writeRaw(value);
                    break;
                case 3:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(CDR_T));  break;
                    break;
                case 4:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(SPU_T));  break;
                    break;
                case 5:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(PIO_T));  break;
                    break;
                case 6:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(OTC_T));  break;
                    break;
                }
                lastDevice->DMAWrite(dmaStartTime++,value);
				
			} else
			if (param = found(start, (u8*)"P2M] ")) {
				// [MDEC_P2M] 7fff7fff
				u64 time = GetTime(param, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.
                if (time != 0) { dmaStartTime = time; }
                u32 value = ValueHexAfter(param,(u8*)" ");
                switch (lastDMAchannel) {
                case 0:
                case 1:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(MDEC_T)); break;
                case 2:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(GPU_T));  break;
                case 3:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(CDR_T));  break;
                    break;
                case 4:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(SPU_T));  break;
                    break;
                case 5:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(PIO_T));  break;
                    break;
                case 6:
                    lastDevice = ((DeviceSingleOp*)getDeviceFromType(OTC_T));  break;
                    break;
                }
                lastDevice->DMARead(dmaStartTime++,value);
			} else
			if (param = found(start, (u8*)"BIOS")) {
				// BIOS b0:11: TestEvent(event = 0xf1000004)
                u32 valueA = ValueHexAfter(param,(u8*)" ");
                u32 valueB = ValueHexAfter(param,(u8*)":");
				u64 time = GetTime(param, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.

    			((DeviceSingleOp*)getDeviceFromType(BIOS_CALL))->DMARead(time,(valueA<<16) | valueB);
                                
			} else
			if (param = found(start, (u8*)"[GTE]")) {
                u8* param2;
                int op = -1;
                int regID = -1;
                u32 value = 0;
                u64 time;
			    if (param2 = found(param, (u8*)" Write Reg")) {
                    regID = ValueHexAfter(param2,(u8*)"");
                    param2 += 2;
                    value = ValueHexAfter(param2,(u8*)"=");
				    time = GetTime(param2, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.
                    op = 0;
                } else
			    if (param2 = found(param, (u8*)" Read Reg")) {
                    regID = ValueHexAfter(param2,(u8*)"");
                    value = ValueHexAfter(param2,(u8*)"=");
                    param2 += 2;

				    time = GetTime(param2, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.
                    op = 1;
                } else
			    if (param2 = found(param, (u8*)" Opcode")) {
                    value = ValueHexAfter(param2,(u8*)"");
				    time = GetTime(param2, finalTime); // ASKED FOR TIME BUT DID NOT GET IT.
                    op = 2;
                }
          
                switch (op) {
                case 0:
    			    ((DeviceSingleOp*)getDeviceFromType(GTE_T))->Write(time,regID,value,0xF);
                    break;
                case 1:
    			    ((DeviceSingleOp*)getDeviceFromType(GTE_T))->Read(time,regID,value,0xF);
                    break;
                case 2:
    			    ((DeviceSingleOp*)getDeviceFromType(GTE_T))->DMARead(time,value);
                    break;
                }
                                
			} else
			if (param = found(start, (u8*)"[CDROM_CTRL]")) {
				//[CDROM_CTRL]: Write int ack 07 @ 75132824	
				//[CDROM_CTRL]: Read resp FIFO 22 @ 75132946
				//[CDROM_CTRL]: Write request 00 @ 75132983
				//[CDROM_CTRL]: Read int en ff @ 75132986
			} else
			if (param = found(start, (u8*)"IO Resp: ")) {
			    // IO Resp: 00000008
			    u8* p = start;
			    u32 value = ValueHexAfter(p,(u8*)"IO Resp: ");
                lastDeviceRead->Read(lastTime, lastAddr, value, lastMask);
			} else
			if (param = found(start, (u8*)"GPU_BUSY")) {
			    // IO Resp: 00000008
			    u8* p = start;
			    u32 value = ValueHexAfter(param,(u8*)"=");
				u64 time = GetTime(param, finalTime);

                DeviceSwitchFlag* pDev = ((DeviceSwitchFlag*)getDeviceFromType(GPUBUSY_T)); 

                pDev->DoEventType(time,EVENT_BOOL,value ? true : false);


			} else
			if (param = found(start, (u8*)"[DISPLAY] Change active display")) {
                // [DISPLAY] Change active display X=0 Y=240 @ 3064132
                // [DISPLAY] Change active display X=0 Y=0 @ 4849547

	            DeviceSingleOp* pBuffSwitch = (DeviceSingleOp*)getDeviceFromType(DISPLAY_SWITCH);
				u64 time = GetTime(param, finalTime);
                u64 value = 1;
                pBuffSwitch->Write(time,0,value,4);
            } else {
				// Ignore any other lines for now...

				// IO CPU interrupt asserted: 008 @ 390156
				// 
				/*
				  GPU_STAT:   0x144e020a
				  GPU_DEBUG0: 0x13000000
					DISP_AREA_X: 0
					DISP_AREA_Y: 0
				  GPU_DEBUG1: 0x40010c60
				  MDEC_STAT: 0x80000000
				  CD_INDEX: 0x18
				  CD_INT_EN_RD: 0xe0
				  CD_INT_STS_RD: 0xe0
				  CD_DEBUG0: 0x00000000
				  CD_DEBUG1: 0x00000000
				scratch.bin: 1024 bytes
				vram.bin: 1048576 bytes
				main.bin: 2097152 bytes
				End of restore FW
				==============================================
				End of simulation (CTRL-C=0)
				MIPS PC:  0x8007b77c
				Cycles:   749999999
				PSX time: 24.000000s
				Realtime: 2039.097328s
				Effective: 367KHz
				==============================================
				
				*/
				// [DMA] Transfer complete (memory -> peripheral)
                printf((const char*)start);
			}
			break;
		case 1:

			state = 0;
			break;
		}
		
		// 3. Load into correct device.
	
		if (*parse == 0) {
			break;
		}
		
		// parse now point to next line.
		if (parse >= halfp) {
			memcpy(block,halfp,HALF);
			parse -= HALF;
			fread (halfp,blocksize,1,file);
			size  -= blocksize;
			halfp[blocksize] = 0;
			if (size < HALF) {
				blocksize = size;
			}
		}
	}

    *pLastTime = finalTime;
}
