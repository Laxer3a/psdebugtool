#ifndef PSX_EVENTS
#define PSX_EVENTS

#include "myTypes.h"

/*
struct PSXEventData {
	// Sub classes stores info...
};
*/

enum EventType {
    UNDEF_EVENT = -1,
	READ_ADR	= 0,
	WRITE_ADR	= 1,
	READ_DMA	= 2,
	WRITE_DMA	= 3,

	EVENT_CHANGE= 8,
	EVENT_BOOL  = 16,
};

enum DeviceType {
    UNKNOWN_T       = 0,
    MC1_T           = 1,
    JOY_T           = 2,
    SIO_T           = 3,
    MC2_T           = 4,
    IRQ_T           = 5,
    DMA_T           = 6,
    TMR_T           = 7,
    CDR_T           = 8,
    GPU_T           = 9,
    GPUBUSY_T       = 10,
    MDEC_T          = 11,
    SPU_T           = 12,
    EXP2_T          = 13,
    PIO_T           = 14,
    OTC_T           = 15,
    ATCONS_T        = 16,

    DMA_CH0_T       = ATCONS_T + 1,
    DMA_CH1_T       = DMA_CH0_T + 1,
    DMA_CH2_T       = DMA_CH0_T + 2,
    DMA_CH3_T       = DMA_CH0_T + 3,
    DMA_CH4_T       = DMA_CH0_T + 4,
    DMA_CH5_T       = DMA_CH0_T + 5,
    DMA_CH6_T       = DMA_CH0_T + 6,

    DISPLAY_SWITCH   = DMA_CH6_T + 1,
    BIOS_CALL       = DISPLAY_SWITCH + 1,
    GTE_T           = BIOS_CALL + 1,

    VBL_IRQ_T       = GTE_T+1,
    GPU_IRQ_T       = VBL_IRQ_T + 1,
    CDROM_IRQ_T     = VBL_IRQ_T + 2,
    DMA_IRQ_T       = VBL_IRQ_T + 3,
    TIME0_IRQ_T     = VBL_IRQ_T + 4,
    TIME1_IRQ_T     = VBL_IRQ_T + 5,
    TIME2_IRQ_T     = VBL_IRQ_T + 6,
    JOYMEM_IRQ_T    = VBL_IRQ_T + 7,
    SIO_IRQ_T       = VBL_IRQ_T + 8,
    SPU_IRQ_T       = VBL_IRQ_T + 9,
    LIGHTPEN_IRQ_T  = VBL_IRQ_T + 10,
};

struct PSXEvent {
	u64				time;
	u32				adr;
	u32				value;
	// Some room (constant pool ?) 6 bytes.
	u8				type;
	u8				mask;
    u32             filterID;

    inline bool isRead()  { return (type == READ_ADR)  || (type ==  READ_DMA); }
    inline bool isWrite() { return (type == WRITE_ADR) || (type == WRITE_DMA); }
};

// Block of PSX Event
// Allocated by block, allow to expand memory...
struct PSXEventHolder {
    static const int EVENT_COUNT = 16384;
	PSXEventHolder();

	PSXEventHolder*	nextHolder;

	u64				startTime;
	u64				lastTime;
	u32				eventCount;
    u32             poolIndex;

	PSXEvent		events[EVENT_COUNT];

	PSXEvent*		Allocate(u64 time);

	PSXEvent*		GetNextEvent(PSXEvent* event, PSXEventHolder** ppHolder);
	// We never disallocate for now.
};

/*	ACCELERATION STRUCTURE TO FIND START...
	For now binary search ?
struct PSXEventKey {
	uint
	
};
*/

class Device {
protected:
	Device(DeviceType type_, const char* name_, u32 startIncludeAdr, u32 endExcludeAdr);
public:
    void clearEvents();

	const char*			name;
	u32					color;
	u32					height;
	u32					startAdr;
	u32					endAdr;
    bool                visible;
    DeviceType          type;

	PSXEventHolder*		recordsStart;
	PSXEventHolder*		recordsLast;


	bool	FindEvent	(u64 time, PSXEventHolder** ppHolder, PSXEvent** ppEvent);
protected:
	PSXEvent*	Allocate(u64 time,EventType type);
};

class DeviceSingleOp : public Device {
public:
	DeviceSingleOp(DeviceType type_, const char* name_, u32 startIncludeAdr, u32 endExcludeAdr):Device(type_,name_,startIncludeAdr,endExcludeAdr) {}
	
	void	Write		(u64 time, u32 adr, u32 value, u8 mask);
	void	Read		(u64 time, u32 adr, u32 value, u8 mask);
	void	DMAWrite	(u64 time, u32 value);
	void	DMARead		(u64 time, u32 value);
};

class DeviceSwitchValue : public Device {
public:
	DeviceSwitchValue(DeviceType type_, const char* name_, u32 startIncludeAdr, u32 endExcludeAdr):Device(type_,name_,startIncludeAdr,endExcludeAdr) {}
	void	DoEventType	(u64 time, EventType type, u32 value);
};

class DeviceSwitchFlag  : public Device {
public:
	DeviceSwitchFlag(DeviceType type_, const char* name_, u32 startIncludeAdr, u32 endExcludeAdr):Device(type_,name_,startIncludeAdr,endExcludeAdr) {}
	void	DoEventType	(u64 time, EventType type, bool value);
};

Device*     getDeviceFromAddr   (u32 adr);
Device*     getDeviceFromType   (DeviceType type);
Device**	getDevices		    (u32& deviceCount);

void        createDevices	    ();

void        parserSIMLOG        (const char* fileName, u64* pLastTime);

#endif
