#include "psxevents.h"

/*
    TODO :
    - Have all EventHolder in a pool of free holder (preallocated max memory amount)
    - Also a list of all used holder.
    - When Free holder list is empty, find the holder with the 'oldest' range.
        - Remove from owner, update owner min/max time.
        - Reset the holder and assign it to new owner and record new data with it.
    => Get equivalent of ring buffers.

 */

bool Device::FindEvent(u64 time, PSXEventHolder** ppHolder, PSXEvent** ppEvent) {
	PSXEventHolder*	curr = recordsStart;
	PSXEventHolder*	prev = NULL;
	// Scan block first, find 
	
	while (curr) {
		if ((time >= curr->startTime) && (time <= curr->lastTime) && (curr->eventCount != 0)) {
			*ppHolder = curr;

			// TODO Binary search, take previous record.
            *ppEvent = &curr->events[0];
            /*
            for (int n=0; n < curr->eventCount; n++) {
                if (curr->events[n].time >= time) {
                    if (curr->events[n].time == time) {
                        *ppEvent = &curr->events[n];
                    } else {
                        if (n > 0) {
                            *ppEvent = &curr->events[n-1];
                        } else {
                            *ppEvent = &curr->events[0];
                        }
                    }
                    *ppHolder= curr;
                    return true;
                }
            }
            */

			return true;
		} else {
            // Before or after...

			// Previous block did not find, curr block starts too late
			// Last entry of previous block.
			if ((curr->eventCount == 0) || (time < curr->startTime)) {
                // Before...
				if (prev) {
					// Last record of previous.
					*ppHolder	= prev;
					*ppEvent	= &prev->events[prev->eventCount-1];
					return true;
				} else {
                    if (curr->eventCount != 0) {
					    *ppHolder	= curr;
					    *ppEvent	= &curr->events[0];
                        return true;
                    } else {
					    break;
                    }
				}
			} else {
                // Go Next...
                prev = curr;
                curr = curr->nextHolder;
                if (!curr) {
				    if (prev) {
					    // Last record of previous.
					    *ppHolder	= prev;
					    *ppEvent	= &prev->events[prev->eventCount-1];
					    return true;
				    } else {
					    // Exit
					    break;
				    }
                }
            }

		}
	}

	*ppHolder = NULL;
	*ppEvent  = NULL;
	return false;
}

PSXEvent* PSXEventHolder::GetNextEvent(PSXEvent* event, PSXEventHolder** ppHolder) {
	event++;
	if (event >= &events[EVENT_COUNT]) {
		PSXEventHolder* pHold = this->nextHolder;
		if (pHold) {
			*ppHolder = pHold;
			return pHold->events;			
		} else {
			*ppHolder = this;
			return NULL;
		}
	} else {
		*ppHolder = this;
		return event;
	}
}

PSXEventHolder::PSXEventHolder() {
	nextHolder	= NULL;
	eventCount	= 0;
	startTime	= 0;
	lastTime	= 0;
    poolIndex   = 0xFFFFFFFF; // Not using pool.
}

PSXEvent* PSXEventHolder::Allocate(u64 time) {
	if (eventCount < EVENT_COUNT) {
		PSXEvent* p = &events[eventCount++]; 
		if (eventCount == 1) {
			startTime = time;
		}
		p->time		= time;
        p->filterID = 0;
		lastTime	= time;
		return p;
	}

	return NULL;
}


Device::Device(DeviceType type_, const char* name_, u32 startIncludeAdr, u32 endExcludeAdr):type(type_),name(name_),recordsStart(NULL),recordsLast(NULL),startAdr(startIncludeAdr),endAdr(endExcludeAdr) {
    this->color  = 0xFFFFFFF;
    this->height = 20;
    this->visible = true;
	recordsStart = new PSXEventHolder();
	recordsLast	 = recordsStart;
}

PSXEvent* Device::Allocate(u64 time, EventType type) {
	PSXEvent* rec = recordsLast->Allocate(time);

	if (!rec) {
		PSXEventHolder* newHolder = new PSXEventHolder();
		newHolder->startTime = time;
		recordsLast->nextHolder = newHolder;
		recordsLast				= newHolder;
		rec = newHolder->Allocate(time);
	}

	rec->type = type;
	return rec;
}

void DeviceSingleOp::Write		(u64 time, u32 adr, u32 value, u8 mask) {
	PSXEvent* p = Allocate(time, WRITE_ADR);
	p->adr		= adr;
	p->value	= value;
	p->mask		= mask;
}

void DeviceSingleOp::Read		(u64 time, u32 adr, u32 value, u8 mask) {
	PSXEvent* p = Allocate(time, READ_ADR);
	p->adr		= adr;
	p->value	= value;
	p->mask		= mask;
}

void DeviceSingleOp::DMAWrite	(u64 time, u32 value) {
	PSXEvent* p = Allocate(time, WRITE_DMA);
	p->adr		= 0;
	p->value	= value;
	p->mask		= 0xF;
}

void DeviceSingleOp::DMARead	(u64 time, u32 value) {
	PSXEvent* p = Allocate(time, READ_DMA);
	p->adr		= 0;
	p->value	= value;
	p->mask		= 0xF;
}

void DeviceSwitchValue::DoEventType	(u64 time, EventType type, u32 value) {
	PSXEvent* p = Allocate(time, type);
	p->adr		= 0;
	p->value	= value;
	p->mask		= 0xF;
}

void DeviceSwitchFlag::DoEventType	(u64 time, EventType type, bool value) {
	PSXEvent* p = Allocate(time, type);
	p->adr		= 0;
	p->value	= value ? 1 : 0;
	p->mask		= 0xF;
}

Device* devices[200];
int     deviceCount = 0;

#include "memoryMap.h"

const int RANGE_IO = 0x3000;
Device*        adrIO [RANGE_IO]; // 1F801000h~1F804000

Device*     getDeviceFromAddr   (u32 adr) {
	u32 offset	= adr - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        return adrIO[offset];
    } else {
        // Unknown.
        return devices[0];
    }
}

Device*     getDeviceFromType   (DeviceType type) {
    for (int n=0; n < deviceCount; n++) {
        if (devices[n]->type==type) {
            return devices[n];
        }
    }
    return NULL;
}

void createDevices() {
	// [UNKNOWN IS ZERO]
	devices[deviceCount++] = new DeviceSingleOp(UNKNOWN_T,"UNKNOWN",    0x0,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(MC1_T    ,"MC1" ,0x1F801000,0x1F801024);
	devices[deviceCount++] = new DeviceSingleOp(JOY_T    ,"JOY" ,0x1F801040,0x1F801050);
	devices[deviceCount++] = new DeviceSingleOp(SIO_T    ,"SIO" ,0x1F801050,0x1F801060);
	devices[deviceCount++] = new DeviceSingleOp(MC2_T    ,"MC2" ,0x1F801060,0x1F801064);
	devices[deviceCount++] = new DeviceSingleOp(IRQ_T    ,"IRQ" ,0x1F801070,0x1F801078);
	devices[deviceCount++] = new DeviceSingleOp(DMA_T    ,"DMA" ,0x1F801080,0x1F801100);
	devices[deviceCount++] = new DeviceSingleOp(TMR_T    ,"TMR" ,0x1F801100,0x1F801130);
	devices[deviceCount++] = new DeviceSingleOp(CDR_T    ,"CDR" ,0x1F801800,0x1F801804);
	devices[deviceCount++] = new DeviceSingleOp(GPU_T    ,"GPU" ,0x1F801810,0x1F801818);
	devices[deviceCount++] = new DeviceSwitchFlag(GPUBUSY_T,"GPU_BUSY" ,0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(MDEC_T   ,"MDEC",0x1F801820,0x1F801828);
	devices[deviceCount++] = new DeviceSingleOp(SPU_T    ,"SPU" ,0x1F801C00,0x1F802000);
	devices[deviceCount++] = new DeviceSingleOp(EXP2_T   ,"EXP2/ATCONS",0x1F802000,0x1F804000);
	devices[deviceCount++] = new DeviceSingleOp(PIO_T    ,"PIO" ,0x1F000000,0x1F080000);
	devices[deviceCount++] = new DeviceSingleOp(OTC_T    ,"OTC" ,0xFFFFFFFF,0xFFFFFFFF);
    
//	devices[deviceCount++] = new DeviceSingleOp(ATCONS_T ,"ATCONS",0x1F802000,0x1F804000);

	// Build reverse table for IO section only.
	for (int n=0; n<RANGE_IO; n++) {
		adrIO[n] = devices[0]; // Default to UNKNOWN first.
	}

	for (int n=1; n<deviceCount; n++) {
		Device* pDev = devices[n];
		u32 start	= pDev->startAdr;
		u32 end		= pDev->endAdr;
		u32 offset	= start - 0x1F801000;
		if (offset >= 0 && offset < RANGE_IO) {
			for (int adr=start; adr < end; adr++) {
				adrIO[adr - 0x1F801000] = pDev;
			}
		}
	}

	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH0_T,"DMA_CH0",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH1_T,"DMA_CH1",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH2_T,"DMA_CH2",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH3_T,"DMA_CH3",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH4_T,"DMA_CH4",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH5_T,"DMA_CH5",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSwitchFlag(DMA_CH6_T,"DMA_CH6",0xFFFFFFFF,0xFFFFFFFF);


	devices[deviceCount++] = new DeviceSingleOp(DISPLAY_SWITCH,"GPU_DISPLAY",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(BIOS_CALL,"BIOS_CALL",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(GTE_T    ,"GTE"      ,0xFFFFFFFF,0xFFFFFFFF);

    // Other 'devices'
	devices[deviceCount++] = new DeviceSingleOp(VBL_IRQ_T     ,"VBL_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(GPU_IRQ_T     ,"GPU_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(CDROM_IRQ_T   ,"CDROM_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(DMA_IRQ_T     ,"DMA_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(TIME0_IRQ_T   ,"TIMER0_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(TIME1_IRQ_T   ,"TIMER1_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(TIME2_IRQ_T   ,"TIMER2_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(JOYMEM_IRQ_T  ,"JOYMEM_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(SIO_IRQ_T     ,"SIO_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(SPU_IRQ_T     ,"SPU_IRQ",0xFFFFFFFF,0xFFFFFFFF);
	devices[deviceCount++] = new DeviceSingleOp(LIGHTPEN_IRQ_T,"LIGHTPEN_IRQ",0xFFFFFFFF,0xFFFFFFFF);
}

Device** getDevices(u32& deviceCount_) {
	deviceCount_ = deviceCount;
	return &devices[0];
}
