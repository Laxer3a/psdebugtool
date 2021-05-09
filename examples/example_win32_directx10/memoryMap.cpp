/*
Expansion Region 1

  1F000000h 80000h Expansion Region (default 512 Kbytes, max 8 MBytes)
  1F000000h 100h   Expansion ROM Header (IDs and Entrypoints)

Scratchpad

  1F800000h 400h Scratchpad (1K Fast RAM) (Data Cache mapped to fixed address)
*/
#include "memoryMap.h"

// sprintf
#include <stdio.h>
// strdup
#include <string.h>

const int READ_FLAG  = 16;
const int WRITE_FLAG = 8;


MemoryMapEntry* lastEntry;
BitDesc creator[33];
int creatorCount = 0;

void NewBitDetail(int startPos, int bitCount, const char* desc, BitDescFormat format = FORMAT_HEXA) {
    BitDesc& ref = creator[creatorCount];
    ref.count = bitCount;
    ref.desc  = desc;
    ref.pos   = startPos;
    ref.format= format;
    ref.enums = NULL;
    ref.onlyConstant= false;
    ref.constantValue=0;
    creatorCount++;
}

void Flush(MemoryMapEntry* nextEntry) {
    if (lastEntry) {
        if (creatorCount != 0) {
            NewBitDetail(-1, -1, NULL); // End record.
            lastEntry->bitDescArray = new BitDesc[creatorCount];
            memcpy(lastEntry->bitDescArray,creator,sizeof(BitDesc)*creatorCount);
        }
    }
    lastEntry = nextEntry; creatorCount = 0;
}

void NewEntry (u32 adr, u32 range, const char* desc, int sizeAndAccessMask = 4 | READ_FLAG | WRITE_FLAG /*RW + 4 byte*/) {
    MemoryMapEntry* pEntry = GetBaseEntry(adr);
    if (pEntry == NULL) {
        MemoryMapEntry* pEntry = new MemoryMapEntry();
        pEntry->addr = adr;
        pEntry->desc = desc;
        pEntry->sizeMask = sizeAndAccessMask;
        pEntry->defined= false;

        Flush(pEntry);

        for (int n=0; n < range; n++) {
            SetBaseEntry(pEntry,adr+n);
        }
    } else {
        // Search for last entry...
        while (pEntry->nextIdentical) {
            pEntry = pEntry->nextIdentical;
        }

        // Add record at the end.
        pEntry->nextIdentical = new MemoryMapEntry();
        pEntry->nextIdentical->Reset(desc,adr,sizeAndAccessMask);
        Flush(pEntry->nextIdentical);
    }
}

void Default(int constant) {
    // Mark that if value not zero could assert.
    BitDesc& ref = creator[creatorCount-1];
    ref.onlyConstant = true;
    ref.constantValue= constant;
}

void BitEnum(int value, const char* desc, bool invalid = false) {    
    BitDesc& ref = creator[creatorCount-1];
    EnumDesc* edesc = new EnumDesc();
    edesc->next = NULL;
    edesc->value= value;
    edesc->desc = desc;
    edesc->invalid = invalid;

    // Insert at the end.
    EnumDesc* pEntry = ref.enums;
    if (pEntry) {
        while (pEntry->next) {
            pEntry = pEntry->next;
        }
        pEntry->next = edesc;
    } else {
        ref.enums = edesc;
    }
}

const char* getString(const char* format, int value) {
    char buff[256];
    sprintf(buff,format,value);
    return strdup(buff);
}

void NewDMAChannelDetails() {
        NewBitDetail(0,1,"Transfer Direction");
            BitEnum(0,"To Main RAM");
            BitEnum(1,"From Main RAM");
        NewBitDetail(1,1,"Memory Address Step");
            BitEnum(0,"Forward +4");
            BitEnum(1,"Backward -4");
        NewBitDetail(2,6,"Not used (always zero)");
            Default(0);
        NewBitDetail(8,1,"Chopping Enable");
            BitEnum(0,"Normal");
            BitEnum(1,"Chopping(run CPU during DMA gaps)");
        NewBitDetail(9,2,"SyncMode, Transfer Synchronisation/Mode (0-3): 0=Start imm. / 1=Sync blocks to DMA requests / 2=Linklist / 3=Res.");
        NewBitDetail(11,5,"Not used(always zero)");
        NewBitDetail(16,3,"Chopping DMA Window Size (1 SHL N words)");
        NewBitDetail(19,1,"Not used(always zero)");
        NewBitDetail(20,3,"Chopping CPU Window Size (1 SHL N clks)");
        NewBitDetail(23,1,"Not used(always zero)");
        NewBitDetail(24,1,"Start/Busy(0=Stopped/Completed, 1=Start/Enable/Busy)");
        NewBitDetail(25,3,"Not used(always zero)");
        NewBitDetail(28,1,"Start/Trigger(0=Normal, 1=Manual Start; use for SyncMode=0)");
        NewBitDetail(29,1,"Unknown (R/W) Pause?  (0=No, 1=Pause?)     (For SyncMode=0 only?)");
        NewBitDetail(30,1,"Unknown (R/W)");
        NewBitDetail(31,1,"Not used(always zero)");
}

void NewDMABlockControlDetail(int dmaChannel0_6) {
    NewBitDetail(0,16,"BC or BS");
    NewBitDetail(16,16,"[Not used] or BA");
    NewBitDetail(0,32,"[Not used]");
}

void timerMode(int timerID) {
    NewBitDetail(0,1,"Sync Enabled");
        BitEnum(0,"Free run");
        BitEnum(1,"Sync via mode");
    NewBitDetail(1,2,"Sync Mode");
        if (timerID == 0) {
            BitEnum(0,"Pause during HBlank");
            BitEnum(1,"Reset Counter to 0000 at HBlank");
            BitEnum(2,"Reset Counter to 0000 at HBlank & Pause outside HBlank");
            BitEnum(3,"Pause until HBlank, then free run");
        } else {
            if (timerID == 1) {
                BitEnum(0,"Pause during VBlank");
                BitEnum(1,"Reset Counter to 0000 at VBlank");
                BitEnum(2,"Reset Counter to 0000 at VBlank & Pause outside VBlank");
                BitEnum(3,"Pause until VBlank, then free run");
            } else {
                BitEnum(0,"Stop counter at current value");
                BitEnum(3,"Stop counter at current value");
                BitEnum(1,"Free run (same as no sync mode)");
                BitEnum(2,"Free run (same as no sync mode)");
            }
        }
    NewBitDetail(3,1,"Reset to 0000 when");
        BitEnum(0,"Reach FFFF");
        BitEnum(1,"Equal Target");
    NewBitDetail(4,1,"IRQ when Counter == Target");
    NewBitDetail(5,1,"IRQ when Counter == FFFF");
    NewBitDetail(6,1,"IRQ Once/Repeat");
        BitEnum(0,"Once");
        BitEnum(1,"Repeat");
    NewBitDetail(7,1,"IRQ Pulse vs Toggle");
        BitEnum(0,"Bit 10 Pulse");
        BitEnum(1,"Bit 10 Toggle On/Off");
    NewBitDetail(8,2,"ClockSource");
        if (timerID == 0 || timerID == 1) {
            BitEnum(0,"System Clock");
            BitEnum(2,"System Clock");
            if (timerID == 0) {
                BitEnum(1,"Dot Clock");
                BitEnum(3,"Dot Clock");
            } else {
                BitEnum(1,"HBlank");
                BitEnum(3,"HBlank");
            }
        } else {
            BitEnum(0,"System Clock");
            BitEnum(1,"System Clock");
            BitEnum(2,"System Clock / 8");
            BitEnum(3,"System Clock / 8");
        }
        BitEnum(1,"Reset Counter to 0000 at HBlank");
        BitEnum(3,"Pause until HBlank, then free run");
}

const int RANGE_IO = 0x3000;
MemoryMapEntry* adrMap  [RANGE_IO];
u8              adrValue[RANGE_IO];

u32 GetValueIO(MemoryMapEntry* entry) {
	u32 offset	= entry->addr - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        int size = 4; // Default
        switch (entry->addr & 3) {
        case 0:
            // 4/2/1
            if (entry->sizeMask & 1) {
                size = 1;
            }
            if (entry->sizeMask & 2) {
                size = 2;
            }
            if (entry->sizeMask & 4) {
                size = 4;
            }
            break;
        case 1:
        case 3:
            size = 1;
            break;
        case 2:
            size = 2;
            break;
        }

        u32 v = 0;
        for (int n=offset; n < offset+size; n++) {
            v = (v<<8) + adrValue[n];
        }

        return v;
    }
    
    return 0;
}

u8              GetIOsValue(u32 addr_) {
	u32 offset	= addr_ - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        return adrValue[offset];
    } else {
        return 0;
    }
}

void            SetIOsValueByte(u32 addr_, u8 v) {
	u32 offset	= addr_ - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        adrValue[offset] = v;
    }
}

void CreateMap() {
	for (int n=0; n<RANGE_IO; n++) {
        adrMap[n] = NULL;
	}

	// Memory Control 1
	NewEntry(0x1F801000,4,"Expansion 1 Base Address (usually 1F000000h)");
	NewEntry(0x1F801004,4,"Expansion 2 Base Address (usually 1F802000h)");
	NewEntry(0x1F801008,4,"Expansion 1 Delay/Size (usually 0013243Fh; 512Kbytes 8bit-bus)");
	NewEntry(0x1F80100C,4,"Expansion 3 Delay/Size (usually 00003022h; 1 byte)");
	NewEntry(0x1F801010,4,"BIOS ROM    Delay/Size (usually 0013243Fh; 512Kbytes 8bit-bus)");
	NewEntry(0x1F801014,4,"SPU_DELAY   Delay/Size (usually 200931E1h)");
	NewEntry(0x1F801018,4,"CDROM_DELAY Delay/Size (usually 00020843h or 00020943h)");
	NewEntry(0x1F80101C,4,"Expansion 2 Delay/Size (usually 00070777h; 128-bytes 8bit-bus)");
	NewEntry(0x1F801020,4,"COM_DELAY / COMMON_DELAY (00031125h or 0000132Ch or 00001325h)");

	// Peripheral I/O Ports
	NewEntry(0x1F801040,4,"JOY_DATA Joypad/Memory Card Data (R/W)", 5 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801044,4,"JOY_STAT Joypad/Memory Card Status (R)", 4 | READ_FLAG);
    {
        NewBitDetail(0,1,"TX READY FLAG");
        NewBitDetail(1,1,"RX FIFO NOT EMPTY");
            BitEnum(0,"Empty");
            BitEnum(1,"Not Empty");
        NewBitDetail(2,1,"TX READY FLAG 2");
            BitEnum(0,"Ready");
            BitEnum(1,"Finished");
        NewBitDetail(3,1,"RX PARITY ERROR");
            BitEnum(0,"None");
            BitEnum(1,"Error");
        NewBitDetail(4,1,"Unknown");
            Default(0);
        NewBitDetail(5,1,"Unknown");
            Default(0);
        NewBitDetail(6,1,"Unknown");
            Default(0);
        NewBitDetail(7,1,"/ACK Input Level");
            BitEnum(0,"Hi");
            BitEnum(1,"Lo");
        NewBitDetail(8,1,"Unknown");
            Default(0);
        NewBitDetail(9,1,"IRQ");
            BitEnum(0,"None");
            BitEnum(1,"IRQ7");
        NewBitDetail(10,1,"Unknown");
            Default(0);
        NewBitDetail(11,21,"Baud Rate Timer");
    }

	NewEntry(0x1F801048,2,"JOY_MODE Joypad/Memory Card Mode (R/W)", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,2,"Baudrate reload factor");
            BitEnum(0,"* 1");
            BitEnum(1,"* 1");
            BitEnum(2,"* 16");
            BitEnum(3,"* 64");
        NewBitDetail(2,2,"Char Length");
            BitEnum(0,"5 bits");
            BitEnum(1,"6 bits");
            BitEnum(2,"7 bits");
            BitEnum(3,"8 bits");
        NewBitDetail(4,1,"Parity Enable");
        NewBitDetail(5,1,"Parity Type");
            BitEnum(0,"Even");
            BitEnum(1,"Odd");
        NewBitDetail(6,2,"Unknown");
            Default(0);
        NewBitDetail(8,1,"CLK Output Polarity");
            BitEnum(0,"Std=>High=idle");
            BitEnum(1,"Inv=>Low =idle");
        NewBitDetail(9,7,"Unknown");
            Default(0);
    }

	NewEntry(0x1F80104A,2,"JOY_CTRL Joypad/Memory Card Control (R/W)", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,1,"TX Enable (TXEN)");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(1,1,"/JOYn Output");
            BitEnum(0,"0=High");
            BitEnum(1,"1=Low/Select");
        NewBitDetail(2,1,"RX Enable (RXEN)");
            BitEnum(0,"Normal /JOYn low");
            BitEnum(1,"Force Enable once");
        NewBitDetail(3,1,"Unknown");
        NewBitDetail(4,1,"Acknowledge");
            BitEnum(0,"No change");
            BitEnum(1,"Reset JOYSTAT bits3,9");
        NewBitDetail(5,1,"Unknown");
        NewBitDetail(6,1,"Reset");
            BitEnum(0,"No change");
            BitEnum(1,"Reset most JOY reg to zero");
        NewBitDetail(7,1,"Not used");
            Default(0);
        NewBitDetail(8,2,"RX Interrupt Mode");
            BitEnum(0,"FIFO has 1 Byte");
            BitEnum(1,"FIFO has 2 Byte");
            BitEnum(2,"FIFO has 4 Byte");
            BitEnum(3,"FIFO has 8 Byte");
        NewBitDetail(10,1,"TX Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(11,1,"RX Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(12,1,"ACK Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(13,1,"Desired Slot Number (set to LOW when Bit1=1)");
            BitEnum(0,"JOY 1");
            BitEnum(1,"JOY 2");
        NewBitDetail(14,2,"Unused");
            Default(0);
    }

	NewEntry(0x1F80104E,2,"JOY_BAUD Joypad/Memory Card Baudrate (R/W)", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801050,4,"SIO_DATA Serial Port Data (R/W)", 5 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801054,4,"SIO_STAT Serial Port Status (R)", 4 | READ_FLAG);
    {
        NewBitDetail(0,1,"TX READY FLAG");
        NewBitDetail(1,1,"RX FIFO NOT EMPTY");
            BitEnum(0,"Empty");
            BitEnum(1,"Not Empty");
        NewBitDetail(2,1,"TX READY FLAG 2");
            BitEnum(0,"Ready");
            BitEnum(1,"Finished");
        NewBitDetail(3,1,"RX PARITY ERROR");
            BitEnum(0,"None");
            BitEnum(1,"Error");
        NewBitDetail(4,1,"RX FIFO OVERRUN");
            BitEnum(0,"None");
            BitEnum(1,"Error");
        NewBitDetail(5,1,"RX BAD STOP BIT");
            BitEnum(0,"None");
            BitEnum(1,"Error");
        NewBitDetail(6,1,"RX Input Level");
            BitEnum(0,"Normal");
            BitEnum(1,"Inverted");
        NewBitDetail(7,1,"DSR Input Level");
            BitEnum(0,"Off");
            BitEnum(1,"On");
        NewBitDetail(8,1,"CTS Input Level");
            BitEnum(0,"Off");
            BitEnum(1,"On");
        NewBitDetail(9,1,"IRQ");
            BitEnum(0,"None");
            BitEnum(1,"IRQ");
        NewBitDetail(10,1,"Unknown");
            Default(0);
        NewBitDetail(11,15,"Baud Rate Timer");
        NewBitDetail(26,6,"Unknown");
    }
	NewEntry(0x1F801058,2,"SIO_MODE Serial Port Mode (R/W)", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,2,"Baudrate reload factor");
            BitEnum(0,"* 1");
            BitEnum(1,"* 1");
            BitEnum(2,"* 16");
            BitEnum(3,"* 64");
        NewBitDetail(2,2,"Char Length");
            BitEnum(0,"5 bits");
            BitEnum(1,"6 bits");
            BitEnum(2,"7 bits");
            BitEnum(3,"8 bits");
        NewBitDetail(4,1,"Parity Enable");
        NewBitDetail(5,1,"Parity Type");
            BitEnum(0,"Even");
            BitEnum(1,"Odd");
        NewBitDetail(6,2,"Stop bit Length");
            BitEnum(0,"Reserved / 1 bit");
            BitEnum(1,"1 bit");
            BitEnum(2,"1.5 bit");
            BitEnum(3,"2 bits");
        NewBitDetail(8,8,"Unused");
            Default(0);
    }

	NewEntry(0x1F80105A,2,"SIO_CTRL Serial Port Control (R/W)", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,1,"TX Enable (TXEN)");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(1,1,"DTR Output");
            BitEnum(0,"0=Off");
            BitEnum(1,"1=On");
        NewBitDetail(2,1,"RX Enable (RXEN)");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(3,1,"TX Output Level");
            BitEnum(0,"Normal");
            BitEnum(1,"Inverted");
        NewBitDetail(4,1,"Acknowledge");
            BitEnum(0,"No change");
            BitEnum(1,"Reset JOYSTAT bits3,9");
        NewBitDetail(5,1,"RTS Output Level");
            BitEnum(0,"Off");
            BitEnum(1,"On");

        NewBitDetail(6,1,"Reset");
            BitEnum(0,"No change");
            BitEnum(1,"Reset most SIO reg to zero");
        NewBitDetail(7,1,"Not used");
            Default(0);
        NewBitDetail(8,2,"RX Interrupt Mode");
            BitEnum(0,"FIFO has 1 Byte");
            BitEnum(1,"FIFO has 2 Byte");
            BitEnum(2,"FIFO has 4 Byte");
            BitEnum(3,"FIFO has 8 Byte");
        NewBitDetail(10,1,"TX Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(11,1,"RX Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(12,1,"DSR Interrupt Enable");
            BitEnum(0,"Disable");
            BitEnum(1,"Enable");
        NewBitDetail(13,3,"Unused");
            Default(0);
    }

	NewEntry(0x1F80105C,2,"SIO_MISC Serial Port Internal Register (R/W)", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F80105E,2,"SIO_BAUD Serial Port Baudrate (R/W)", 2 | READ_FLAG | WRITE_FLAG);


	// Memory Control 2
	NewEntry(0x1F801060,4,"RAM_SIZE (usually 00000B88h; 2MB RAM mirrored in first 8MB)", 4 | 2 | READ_FLAG | WRITE_FLAG);

	// Interrupt Control
	NewEntry(0x1F801070,4,"I_STAT - Interrupt status register", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,1,"IRQ0 VBLANK");
        NewBitDetail(1,1,"IRQ1 GPU");
        NewBitDetail(2,1,"IRQ2 CDROM");
        NewBitDetail(3,1,"IRQ3 DMA");
        NewBitDetail(4,1,"IRQ4 TMR0");
        NewBitDetail(5,1,"IRQ5 TMR1");
        NewBitDetail(6,1,"IRQ6 TMR2");
        NewBitDetail(7,1,"IRQ7 JOY/MEMCARD");
        NewBitDetail(8,1,"IRQ8 SIO");
        NewBitDetail(9,1,"IRQ9 SPU");
        NewBitDetail(10,1,"IRQ10 Lightgun");
        NewBitDetail(11,5,"Unused");
        NewBitDetail(16,16,"Garbage");
    }

	NewEntry(0x1F801074,4,"I_MASK - Interrupt mask register", 2 | READ_FLAG | WRITE_FLAG);
    {
        NewBitDetail(0,1,"IRQ0 VBLANK");
        NewBitDetail(1,1,"IRQ1 GPU");
        NewBitDetail(2,1,"IRQ2 CDROM");
        NewBitDetail(3,1,"IRQ3 DMA");
        NewBitDetail(4,1,"IRQ4 TMR0");
        NewBitDetail(5,1,"IRQ5 TMR1");
        NewBitDetail(6,1,"IRQ6 TMR2");
        NewBitDetail(7,1,"IRQ7 JOY/MEMCARD");
        NewBitDetail(8,1,"IRQ8 SIO");
        NewBitDetail(9,1,"IRQ9 SPU");
        NewBitDetail(10,1,"IRQ10 Lightgun");
        NewBitDetail(11,5,"Unused");
        NewBitDetail(16,16,"Garbage");
    }


	// DMA Registers
	NewEntry(0x1F801080,4,"DMA0 MDECin Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F801084,4,"DMA0 MDECin Block Control");
        NewDMABlockControlDetail(0);
	NewEntry(0x1F801088,4,"DMA0 MDECin Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F801090,4,"DMA1 MDECout Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F801094,4,"DMA1 MDECout Block Control");
        NewDMABlockControlDetail(1);
	NewEntry(0x1F801098,4,"DMA1 MDECout Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010A0,4,"DMA2 GPU Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F8010A4,4,"DMA2 GPU Block Control");
        NewDMABlockControlDetail(2);
	NewEntry(0x1F8010A8,4,"DMA2 GPU Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010B0,4,"DMA3 CDRom Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F8010B4,4,"DMA3 CDRom Block Control");
        NewDMABlockControlDetail(3);
	NewEntry(0x1F8010B8,4,"DMA3 CDRom Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010C0,4,"DMA4 SPU Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F8010C4,4,"DMA4 SPU Block Control");
        NewDMABlockControlDetail(4);
	NewEntry(0x1F8010C8,4,"DMA4 SPU Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010D0,4,"DMA5 PIO Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F8010D4,4,"DMA5 PIO Block Control");
        NewDMABlockControlDetail(5);
	NewEntry(0x1F8010D8,4,"DMA5 PIO Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010E0,4,"DMA6 OTC Base Addr");
        NewBitDetail(0,24,"Memopry Addr");
        NewBitDetail(25,8,"[Not used]");
	NewEntry(0x1F8010E4,4,"DMA6 OTC Block Control");
        NewDMABlockControlDetail(6);
	NewEntry(0x1F8010E8,4,"DMA6 OTC Channel Control");
        NewDMAChannelDetails();

	NewEntry(0x1F8010F0,4,"DPCR - DMA Control register");
    {
        NewBitDetail(0,3,"DMA0 MDECin  Priority (0=Hi,7=Lo)");
        NewBitDetail(3,1,"DMA0 MDECin  Master Enable");
        NewBitDetail(4,3,"DMA1 MDECout Priority (0=Hi,7=Lo)");
        NewBitDetail(7,1,"DMA1 MDECout Master Enable");
        NewBitDetail(8,3,"DMA2 GPU     Priority (0=Hi,7=Lo)");
        NewBitDetail(11,1,"DMA2 GPU     Master Enable");
        NewBitDetail(12,3,"DMA3 CDROM   Priority (0=Hi,7=Lo)");
        NewBitDetail(15,1,"DMA3 CDROM   Master Enable");
        NewBitDetail(16,3,"DMA4 SPU     Priority (0=Hi,7=Lo)");
        NewBitDetail(19,1,"DMA4 SPU     Master Enable");
        NewBitDetail(20,3,"DMA5 PIO     Priority (0=Hi,7=Lo)");
        NewBitDetail(23,1,"DMA5 PIO     Master Enable");
        NewBitDetail(24,3,"DMA6 OTC     Priority (0=Hi,7=Lo)");
        NewBitDetail(27,1,"DMA6 OTC     Master Enable");
        NewBitDetail(28,4,"[Unknown]");
    }

	NewEntry(0x1F8010F4,4,"DICR - DMA Interrupt register");
    {
        NewBitDetail(0,5,"[Unknown]");
        NewBitDetail(6,14,"Not Used");
        NewBitDetail(15,1,"Force IRQ (set bit 31)");
        NewBitDetail(16,1,"IRQ Enable DMA0 MDECin");
        NewBitDetail(17,1,"IRQ Enable DMA1 MDECout");
        NewBitDetail(18,1,"IRQ Enable DMA2 GPU");
        NewBitDetail(19,1,"IRQ Enable DMA3 CDROM");
        NewBitDetail(20,1,"IRQ Enable DMA4 SPU");
        NewBitDetail(21,1,"IRQ Enable DMA5 PIO");
        NewBitDetail(22,1,"IRQ Enable DMA6 OTC");
        NewBitDetail(23,1,"Master IRQ Enable");
        NewBitDetail(24,1,"IRQ Flag DMA0 MDECin");
        NewBitDetail(25,1,"IRQ Flag DMA1 MDECout");
        NewBitDetail(26,1,"IRQ Flag DMA2 GPU");
        NewBitDetail(27,1,"IRQ Flag DMA3 CDROM");
        NewBitDetail(28,1,"IRQ Flag DMA4 SPU");
        NewBitDetail(29,1,"IRQ Flag DMA5 PIO");
        NewBitDetail(30,1,"IRQ Flag DMA6 OTC");
        NewBitDetail(31,1,"Master IRQ Flag");
    }

	NewEntry(0x1F8010F8,4,"unknown");
	NewEntry(0x1F8010FC,4,"unknown");

	// Timer, (aka Root counters)
	NewEntry(0x1F801100,4,"Timer 0 Counter Dotclock");
        NewBitDetail(0,16,"Counter");
        NewBitDetail(16,16,"[Garbage]");

	NewEntry(0x1F801104,4,"Timer 0 Mode Dotclock");
        timerMode(0);

	NewEntry(0x1F801108,4,"Timer 0 Target Dotclock");
        NewBitDetail(0,16,"Value");
        NewBitDetail(16,16,"[Garbage]");

	NewEntry(0x1F801110,4,"Timer 1 Counter");
        NewBitDetail(0,16,"Counter");
        NewBitDetail(16,16,"[Garbage]");

	NewEntry(0x1F801114,4,"Timer 1 Mode");
        timerMode(1);

	NewEntry(0x1F801118,4,"Timer 1 Target");
        NewBitDetail(0,16,"Value");
        NewBitDetail(16,16,"[Garbage]");

	NewEntry(0x1F801120,4,"Timer 2 Counter");
        NewBitDetail(0,16,"Counter");
        NewBitDetail(16,16,"[Garbage]");

	NewEntry(0x1F801124,4,"Timer 2 Mode");
        timerMode(2);

	NewEntry(0x1F801128,4,"Timer 2 Target");
        NewBitDetail(0,16,"Value");
        NewBitDetail(16,16,"[Garbage]");

	// CDROM,Registers (Address.Read/Write.Index)

	NewEntry(0x1F801800,1,"CD Index/Status Register (Bit0-1 R/W, Bit2-7 Read Only)"); //		 .x.x   1   
        // TODO

	NewEntry(0x1F801801,1,"CD Response Fifo (R) (usually with Index1)", 1 | READ_FLAG);
	NewEntry(0x1F801801,1,"CD Command Register (W)", 1 | WRITE_FLAG);
	NewEntry(0x1F801801,1,"Unknown/unused W.1/W.2", 1 | WRITE_FLAG);
	NewEntry(0x1F801801,1,"CD Audio Volume for Right-CD-Out to Right-SPU-Input (W)", 1 | WRITE_FLAG);

	NewEntry(0x1F801802,1,"CD Data Fifo - 8bit/16bit (R) (usually with Index0..1)", 1|2| READ_FLAG);
	NewEntry(0x1F801802,1,"CD Parameter Fifo (W)", 1 | WRITE_FLAG);
	NewEntry(0x1F801802,1,"CD Interrupt Enable Register (W)", 1 | WRITE_FLAG);
	NewEntry(0x1F801802,1,"CD Audio Volume for Left-CD-Out to Left-SPU-Input (W)", 1 | WRITE_FLAG);
	NewEntry(0x1F801802,1,"CD Audio Volume for Right-CD-Out to Left-SPU-Input (W)", 1 | WRITE_FLAG);

	NewEntry(0x1F801803,1,"CD Interrupt Enable Register (R)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Interrupt Flag Register (R/W)", 1 | READ_FLAG);   
	NewEntry(0x1F801803,1,"CD Interrupt Enable Register (R) (Mirror)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Interrupt Flag Register (R/W) (Mirror)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Request Register (W)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Interrupt Flag Register (R/W)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Audio Volume for Left-CD-Out to Right-SPU-Input (W)", 1 | READ_FLAG);
	NewEntry(0x1F801803,1,"CD Audio Volume Apply Changes (by writing bit5=1)", 1 | READ_FLAG);

	// GPU Registers
	NewEntry(0x1F801810,4,"GP0 Send GP0 Commands/Packets (Rendering and VRAM Access)", 4 | WRITE_FLAG);
	NewEntry(0x1F801814,4,"GP1 Send GP1 Commands (Display Control)", 4 | WRITE_FLAG);
	NewEntry(0x1F801810,4,"GPUREAD Read responses to GP0(C0h) and GP1(10h) commands", 4 | READ_FLAG);

	NewEntry(0x1F801814,4,"GPUSTAT Read GPU Status Register", 4 | READ_FLAG);
    {
        NewBitDetail(0,4,"Texture X Base (N*64)");
        NewBitDetail(4,1,"Texture Y Base (N*256)");
        NewBitDetail(5,2,"SemiTrasp (0=Alpha,1=Additive,2=Sub,3=50% Additive");
            BitEnum(0,"Alpha");
            BitEnum(1,"Additive");
            BitEnum(2,"Sub");
            BitEnum(3,"Add 50%");
        NewBitDetail(7,2,"Texture Page Color");
            BitEnum(0,"4bit");
            BitEnum(1,"8bit");
            BitEnum(2,"15bit");
            BitEnum(3,"Reserved",true);
        NewBitDetail(9,1,"Dither");
            BitEnum(0,"Disabled");
            BitEnum(1,"Enabled");
        NewBitDetail(10,1,"DrawDisplayArea(0=Prohibited,1=Allowed)");
            BitEnum(0,"Prohibited");
            BitEnum(1,"Allowed");
        NewBitDetail(11,1,"Set Mask bit.");
            BitEnum(0,"Don't touch");
            BitEnum(1,"Set");
        NewBitDetail(12,1,"Draw where Mask not set (0=Always, 1=Not marked only)");
            BitEnum(0,"Always");
            BitEnum(1,"Check Mask");
        NewBitDetail(13,1,"Interlace Field");
        NewBitDetail(14,1,"Reverse Flag");
        NewBitDetail(15,1,"TextureDisable");
        NewBitDetail(16,1,"Horiz Res 2");
            BitEnum(0,"256/320/512/640");
            BitEnum(1,"368");
        NewBitDetail(17,2,"Horiz Res 1");
            BitEnum(0,"256");
            BitEnum(1,"320");
            BitEnum(2,"512");
            BitEnum(3,"640");
        NewBitDetail(19,1,"Vert. Res");
            BitEnum(0,"240");
            BitEnum(1,"480");
        NewBitDetail(20,1,"Vert. Res");
            BitEnum(0,"NTSC/60Hz");
            BitEnum(1,"PAL/50Hz");
        NewBitDetail(21,1,"Depth");
            BitEnum(0,"15 Bit RGB");
            BitEnum(1,"24 Bit RGB");
        NewBitDetail(22,1,"Vert. Interlace");
            BitEnum(0,"Off");
            BitEnum(1,"On");
        NewBitDetail(23,1,"Display Enable");
            BitEnum(0,"Enabled");
            BitEnum(1,"Disabled");
        NewBitDetail(24,1,"IRQ1");
            BitEnum(0,"Off");
            BitEnum(1,"IRQ Set");
        NewBitDetail(25,1,"Data Request(Meaning based on GP1 0x04 DMA dir)");
            // Meaning change based on GP1 0x04 DMA dir.
        NewBitDetail(26,1,"Ready Cmd Receive");
        NewBitDetail(27,1,"Ready VRAM->CPU");
        NewBitDetail(28,1,"Ready Receive DMA");
        NewBitDetail(29,2,"DMA Direction");
            BitEnum(0,"Off");
            BitEnum(1,"???");
            BitEnum(2,"CPU->GP0");
            BitEnum(3,"GPU->CPU");
        NewBitDetail(31,1,"Odd/Even line");
            BitEnum(0,"Even | VBlank");
            BitEnum(1,"Odd");
    }

	// MDEC Registers
	NewEntry(0x1F801820,4,"MDEC Command/Parameter Register (W)", 4 | WRITE_FLAG);
	NewEntry(0x1F801820,4,"MDEC Data/Response Register (R)", 4 | READ_FLAG);
	NewEntry(0x1F801824,4,"MDEC Control/Reset Register (W)", 4 | WRITE_FLAG);
        NewBitDetail(31,1,"Reset MDEC"); // (0=No change, 1=Abort any command, and set status=80040000h)
        NewBitDetail(30,1,"Enable Data-In Req (Enable DMA0 and Status.bit28)");
        NewBitDetail(29,1,"Enable Data-In Req (Enable DMA1 and Status.bit27)");
        NewBitDetail(0,29,"Unknown");
	NewEntry(0x1F801824,4,"MDEC Status Register (R)", 4 | READ_FLAG);
        NewBitDetail(0,16,"Number of work param remain-1");
        NewBitDetail(16,3,"CurrentBlock");
            BitEnum(0,"Y1");
            BitEnum(1,"Y2");
            BitEnum(2,"Y3");
            BitEnum(3,"Y4");
            BitEnum(4,"Cr");
            BitEnum(5,"Cb");
            BitEnum(6,"UNDEF",true);
            BitEnum(7,"UNDEF",true);
        NewBitDetail(19,4,"Not Used");
            Default(0); // Mark that if value not zero could assert.
        NewBitDetail(23,1,"Out Bit 15 Value");
        NewBitDetail(24,1,"Output Sign");
            BitEnum(0,"Unsigned");
            BitEnum(1,"Signed");
        NewBitDetail(25,2,"Output Depth");
            BitEnum(0,"4bit");
            BitEnum(1,"8bit");
            BitEnum(2,"24bit");
            BitEnum(3,"15bit");
        NewBitDetail(27,1,"Data-Out Req");
        NewBitDetail(28,1,"Data-In  Req");
        NewBitDetail(29,1,"Command Busy");
        NewBitDetail(30,1,"Data-In Full");
        NewBitDetail(31,1,"DataOut Empty");


	// SPU Voice 0..23 Registers
	for (int n=0; n < 24; n++) {
		NewEntry(0x1F801C00+n*0x10,2,getString("Voice %i Volume Left", n),2|4|READ_FLAG|WRITE_FLAG);
            NewBitDetail(0,15,"Volume Signed (vol/2)");
            NewBitDetail(15,1,"Sweep Mode");
                BitEnum(0,"Standard");
                BitEnum(1,"Sweep Mode",true); // UNSUPPORTED BY SPU FOR NOW, NO GAME IS SUPPOSED TO USE IT.
    /*
            NewBitDetail(0,2,"Sweep Step");
                BitEnum(0,"+7 or -8");
                BitEnum(1,"+6 or -7");
                BitEnum(2,"+5 or -6");
                BitEnum(3,"+4 or -5");
            NewBitDetail(2,5,"Sweep Shift");
            NewBitDetail(7,5,"Unused");
                Default(0); // Mark that if value not zero could assert.
            NewBitDetail(12,1,"Sweep Phase");
                BitEnum(0,"Pos");
                BitEnum(1,"Neg");
            NewBitDetail(13,1,"Sweep Direction");
                BitEnum(0,"Inc++");
                BitEnum(1,"Dec--");
            NewBitDetail(31,1,"Sweep Mode");
                BitEnum(0,"Linear");
                BitEnum(1,"Exponential");
    */
		NewEntry(0x1F801C02+n*0x10,2,getString("Voice %i Volume Right", n),2|4|READ_FLAG|WRITE_FLAG);
            NewBitDetail(0,15,"Volume Signed (vol/2)");
            NewBitDetail(15,1,"Sweep Mode");
                BitEnum(0,"Standard");
                BitEnum(1,"Sweep Mode",true); // UNSUPPORTED BY SPU FOR NOW, NO GAME IS SUPPOSED TO USE IT.
    /*
            NewBitDetail(0,2,"Sweep Step");
                BitEnum(0,"+7 or -8");
                BitEnum(1,"+6 or -7");
                BitEnum(2,"+5 or -6");
                BitEnum(3,"+4 or -5");
            NewBitDetail(2,5,"Sweep Shift");
            NewBitDetail(7,5,"Unused");
                Default(0); // Mark that if value not zero could assert.
            NewBitDetail(12,1,"Sweep Phase");
                BitEnum(0,"Pos");
                BitEnum(1,"Neg");
            NewBitDetail(13,1,"Sweep Direction");
                BitEnum(0,"Inc++");
                BitEnum(1,"Dec--");
            NewBitDetail(31,1,"Sweep Mode");
                BitEnum(0,"Linear");
                BitEnum(1,"Exponential");
    */
 
		NewEntry(0x1F801C04+n*0x10,2,getString("Voice %i ADPCM Sample Rate", n),2|READ_FLAG|WRITE_FLAG);   
		NewEntry(0x1F801C06+n*0x10,2,getString("Voice %i ADPCM Start Address", n),2|READ_FLAG|WRITE_FLAG);   
		NewEntry(0x1F801C08+n*0x10,4,getString("Voice %i ADSR", n),4|READ_FLAG|WRITE_FLAG);
            NewBitDetail(0,4,"Sustain Level");
            NewBitDetail(4,4,"Decay Shift (Decay Step fixed -8)");
            NewBitDetail(8,2,"Attack Step");
                BitEnum(0,"+7");
                BitEnum(1,"+6");
                BitEnum(2,"+5");
                BitEnum(3,"+4");
            NewBitDetail(10,5,"Attack Shift");
            NewBitDetail(15,1,"Attack Mode");
                BitEnum(0,"Linear");
                BitEnum(1,"Exponential");
            NewBitDetail(16,5,"Release Shift");
            NewBitDetail(21,1,"Release Mode");
                BitEnum(0,"Linear");
                BitEnum(1,"Exponential");
            NewBitDetail(22,2,"Sustain Step (inc/dec)");
                BitEnum(0,"+7 or -8");
                BitEnum(1,"+6 or -7");
                BitEnum(2,"+5 or -6");
                BitEnum(3,"+4 or -5");
            NewBitDetail(24,5,"Sustain Shift");
            NewBitDetail(29,1,"[Unused]");
            NewBitDetail(30,1,"Sustain Direction");
                BitEnum(0,"Inc++");
                BitEnum(1,"Dec--");
            NewBitDetail(31,1,"Sustain Mode");
                BitEnum(0,"Linear");
                BitEnum(1,"Exponential");

		NewEntry(0x1F801C0C+n*0x10,2,getString("Voice %i ADSR Curr Vol", n),2|READ_FLAG|WRITE_FLAG);   
		NewEntry(0x1F801C0E+n*0x10,2,getString("Voice %i ADPCM Repeat Address", n),2|READ_FLAG|WRITE_FLAG);   
		NewEntry(0x1F801E00+n*0x4,4,getString("Voice %i Current Volume Left/Right", n));
            NewBitDetail(0,16,"Left Volume");
            NewBitDetail(16,16,"Right Volume");
	}


	// SPU Control Registers
	NewEntry(0x1F801D80,2,"Main Volume Left");
        NewBitDetail(0,15,"Volume Signed (vol/2)");
        NewBitDetail(15,1,"Sweep Mode");
            BitEnum(0,"Standard");
            BitEnum(1,"Sweep Mode",true); // UNSUPPORTED BY SPU FOR NOW, NO GAME IS SUPPOSED TO USE IT.
/*
        NewBitDetail(0,2,"Sweep Step");
            BitEnum(0,"+7 or -8");
            BitEnum(1,"+6 or -7");
            BitEnum(2,"+5 or -6");
            BitEnum(3,"+4 or -5");
        NewBitDetail(2,5,"Sweep Shift");
        NewBitDetail(7,5,"Unused");
            Default(0); // Mark that if value not zero could assert.
        NewBitDetail(12,1,"Sweep Phase");
            BitEnum(0,"Pos");
            BitEnum(1,"Neg");
        NewBitDetail(13,1,"Sweep Direction");
            BitEnum(0,"Inc++");
            BitEnum(1,"Dec--");
        NewBitDetail(31,1,"Sweep Mode");
            BitEnum(0,"Linear");
            BitEnum(1,"Exponential");
*/

	NewEntry(0x1F801D82,2,"Main Volume Right");
        NewBitDetail(0,15,"Volume Signed (vol/2)");
        NewBitDetail(15,1,"Sweep Mode");
            BitEnum(0,"Standard");
            BitEnum(1,"Sweep Mode",true); // UNSUPPORTED BY SPU FOR NOW, NO GAME IS SUPPOSED TO USE IT.
/*
        NewBitDetail(0,2,"Sweep Step");
            BitEnum(0,"+7 or -8");
            BitEnum(1,"+6 or -7");
            BitEnum(2,"+5 or -6");
            BitEnum(3,"+4 or -5");
        NewBitDetail(2,5,"Sweep Shift");
        NewBitDetail(7,5,"Unused");
            Default(0); // Mark that if value not zero could assert.
        NewBitDetail(12,1,"Sweep Phase");
            BitEnum(0,"Pos");
            BitEnum(1,"Neg");
        NewBitDetail(13,1,"Sweep Direction");
            BitEnum(0,"Inc++");
            BitEnum(1,"Dec--");
        NewBitDetail(31,1,"Sweep Mode");
            BitEnum(0,"Linear");
            BitEnum(1,"Exponential");
*/

	NewEntry(0x1F801D84,4,"Reverb Output Volume Left/Right");
	NewEntry(0x1F801D88,4,"Voice 0..23 Key ON (Start Attack/Decay/Sustain) (W)", 4 | WRITE_FLAG);
        for (int n=0; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
        }
        NewBitDetail(24,8,"Unused");
	NewEntry(0x1F801D8C,4,"Voice 0..23 Key OFF (Start Release) (W)", 4 | WRITE_FLAG);
        for (int n=0; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
        }
        NewBitDetail(24,8,"Unused");

	NewEntry(0x1F801D90,4,"Voice 0..23 Channel FM (pitch lfo) mode (R/W)");
        NewBitDetail(0,1,"Unknown/Unused");
        for (int n=1; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
        }
        NewBitDetail(24,8,"Unused");
	NewEntry(0x1F801D94,4,"Voice 0..23 Channel Noise mode (R/W)");
        for (int n=0; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
                BitEnum(0,"ADPCM");
                BitEnum(1,"NOISE");
        }
        NewBitDetail(24,8,"Unused");
	NewEntry(0x1F801D98,4,"Voice 0..23 Channel Reverb mode (R/W)");
        for (int n=0; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
        }
        NewBitDetail(24,8,"Unused");

	NewEntry(0x1F801D9C,4,"Voice 0..23 Channel ON/OFF (status) (R)", 4 | READ_FLAG);
        for (int n=0; n < 24; n++) {
            NewBitDetail(n,1,getString("Voice %i",n));
        }
        NewBitDetail(24,8,"Unused");

	NewEntry(0x1F801DA0,2,"Unknown? (R) or (W)", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DA2,2,"Sound RAM Reverb Work Area Start Address", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DA4,2,"Sound RAM IRQ Address", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DA6,2,"Sound RAM Data Transfer Address", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DA8,2,"Sound RAM Data Transfer Fifo", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DAA,2,"SPU Control Register (SPUCNT)", 2 | READ_FLAG | WRITE_FLAG);
        NewBitDetail(0,1,"CD  Audio Enable");
        NewBitDetail(1,1,"Ext Audio Enable");
        NewBitDetail(2,1,"CD  Audio Reverb");
        NewBitDetail(3,1,"Ext Audio Reverb");
        NewBitDetail(4,2,"Sound XFer Mode");
            BitEnum(0,"Stop");
            BitEnum(1,"Manual Write");
            BitEnum(2,"DMA Write");
            BitEnum(3,"DMA Read");
        NewBitDetail(6,1,"IRQ9 Enable");
        NewBitDetail(7,1,"Reverb Master Enable");
        NewBitDetail(8,2,"Noise Frequ Step");
            BitEnum(0,"+4");    // TODO CHECK : Reverse of step of ADSR ?
            BitEnum(1,"+5");
            BitEnum(2,"+6");
            BitEnum(3,"+7");
        NewBitDetail(10,4,"Noise Frequ Shift");
        NewBitDetail(14,1,"Mute SPU");
        NewBitDetail(15,1,"Enable SPU");

	NewEntry(0x1F801DAC,2,"Sound RAM Data Transfer Control", 2 | READ_FLAG | WRITE_FLAG);
        NewBitDetail(0,1,"Unknown");
            Default(0); // Mark that if value not zero could assert.
        NewBitDetail(1,3,"Transfer Type");
            BitEnum(0,"Fill");
            BitEnum(1,"Fill");
            BitEnum(2,"Normal");
            BitEnum(3,"Rep2");
            BitEnum(4,"Rep4");
            BitEnum(5,"Rep8");
            BitEnum(6,"Fill");
            BitEnum(7,"Fill");

        NewBitDetail(2,1,"CD  Audio Reverb");

	NewEntry(0x1F801DAE,2,"SPU Status Register (SPUSTAT) (R)", 2 | READ_FLAG);
        // Same as SPUCNT here
        NewBitDetail(0,1,"CD  Audio Enable");
        NewBitDetail(1,1,"Ext Audio Enable");
        NewBitDetail(2,1,"CD  Audio Reverb");
        NewBitDetail(3,1,"Ext Audio Reverb");
        NewBitDetail(4,2,"Sound XFer Mode");
            BitEnum(0,"Stop");
            BitEnum(1,"Manual Write");
            BitEnum(2,"DMA Write");
            BitEnum(3,"DMA Read");
        //------------------------------------
        NewBitDetail(6,1,"IRQ9 Flag");
        NewBitDetail(7,1,"XFer DMA R/W Req");
        NewBitDetail(8,1,"XFer DMA (W) Req");
        NewBitDetail(9,1,"XFer DMA (R) Req");
        NewBitDetail(10,1,"XFer Busy");
        NewBitDetail(11,1,"Use 2nd part (or 1st) capture buffer");
            BitEnum(0,"1st");
            BitEnum(1,"2nd");
        NewBitDetail(12,4,"Unused");
            Default(0); // Mark that if value not zero could assert.

	NewEntry(0x1F801DB0,4,"CD Volume Left/Right");
        NewBitDetail(0,16,"Left Volume");
        NewBitDetail(16,16,"Right Volume");

	NewEntry(0x1F801DB4,4,"Extern Volume Left/Right");
        NewBitDetail(0,16,"Left Volume");
        NewBitDetail(16,16,"Right Volume");

	NewEntry(0x1F801DB8,4,"Current Main Volume Left/Right");
        NewBitDetail(0,16,"Left Volume");
        NewBitDetail(16,16,"Right Volume");

	NewEntry(0x1F801DBC,4,"Unknown? (R/W)");

	// SPU Reverb Configuration Area
	NewEntry(0x1F801DC0,2,"dAPF1  Reverb APF Offset 1", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DC2,2,"dAPF2  Reverb APF Offset 2", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DC4,2,"vIIR   Reverb Reflection Volume 1", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DC6,2,"vCOMB1 Reverb Comb Volume 1", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DC8,2,"vCOMB2 Reverb Comb Volume 2", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DCA,2,"vCOMB3 Reverb Comb Volume 3", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DCC,2,"vCOMB4 Reverb Comb Volume 4", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DCE,2,"vWALL  Reverb Reflection Volume 2", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DD0,2,"vAPF1  Reverb APF Volume 1", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DD2,2,"vAPF2  Reverb APF Volume 2", 2 | READ_FLAG | WRITE_FLAG);
	NewEntry(0x1F801DD4,4,"mSAME  Reverb Same Side Reflection Address 1 Left/Right");
	NewEntry(0x1F801DD8,4,"mCOMB1 Reverb Comb Address 1 Left/Right");
	NewEntry(0x1F801DDC,4,"mCOMB2 Reverb Comb Address 2 Left/Right");
	NewEntry(0x1F801DE0,4,"dSAME  Reverb Same Side Reflection Address 2 Left/Right");
	NewEntry(0x1F801DE4,4,"mDIFF  Reverb Different Side Reflection Address 1 Left/Right");
	NewEntry(0x1F801DE8,4,"mCOMB3 Reverb Comb Address 3 Left/Right");
	NewEntry(0x1F801DEC,4,"mCOMB4 Reverb Comb Address 4 Left/Right");
	NewEntry(0x1F801DF0,4,"dDIFF  Reverb Different Side Reflection Address 2 Left/Right");
	NewEntry(0x1F801DF4,4,"mAPF1  Reverb APF Address 1 Left/Right");
	NewEntry(0x1F801DF8,4,"mAPF2  Reverb APF Address 2 Left/Right");
	NewEntry(0x1F801DFC,4,"vIN    Reverb Input Volume Left/Right");

	// SPU Internal Registers
	NewEntry(0x1F801E60,0x20,"Unknown? (R/W)");
	NewEntry(0x1F801E80,0x180,"Unknown? (Read: FFh-filled) (Unused or Write only?)");

	/*

	Expansion Region 2 (default 128 bytes, max 8 KBytes)

	  1F802000h      80h Expansion Region (8bit data bus, crashes on 16bit access?)

	Expansion Region 2 - Dual Serial Port (for TTY Debug Terminal)

	  1F802020h/1st    DUART Mode Register 1.A (R/W)
	  1F802020h/2nd    DUART Mode Register 2.A (R/W)
	  1F802021h/Read   DUART Status Register A (R)
	  1F802021h/Write  DUART Clock Select Register A (W)
	  1F802022h/Read   DUART Toggle Baud Rate Generator Test Mode (Read=Strobe)
	  1F802022h/Write  DUART Command Register A (W)
	  1F802023h/Read   DUART Rx Holding Register A (FIFO) (R)
	  1F802023h/Write  DUART Tx Holding Register A (W)
	  1F802024h/Read   DUART Input Port Change Register (R)
	  1F802024h/Write  DUART Aux. Control Register (W)
	  1F802025h/Read   DUART Interrupt Status Register (R)
	  1F802025h/Write  DUART Interrupt Mask Register (W)
	  1F802026h/Read   DUART Counter/Timer Current Value, Upper/Bit15-8 (R)
	  1F802026h/Write  DUART Counter/Timer Reload Value,  Upper/Bit15-8 (W)
	  1F802027h/Read   DUART Counter/Timer Current Value, Lower/Bit7-0 (R)
	  1F802027h/Write  DUART Counter/Timer Reload Value,  Lower/Bit7-0 (W)
	  1F802028h/1st    DUART Mode Register 1.B (R/W)
	  1F802028h/2nd    DUART Mode Register 2.B (R/W)
	  1F802029h/Read   DUART Status Register B (R)
	  1F802029h/Write  DUART Clock Select Register B (W)
	  1F80202Ah/Read   DUART Toggle 1X/16X Test Mode (Read=Strobe)
	  1F80202Ah/Write  DUART Command Register B (W)
	  1F80202Bh/Read   DUART Rx Holding Register B (FIFO) (R)
	  1F80202Bh/Write  DUART Tx Holding Register B (W)
	  1F80202Ch/None   DUART Reserved Register (neither R nor W)
	  1F80202Dh/Read   DUART Input Port (R)
	  1F80202Dh/Write  DUART Output Port Configuration Register (W)
	  1F80202Eh/Read   DUART Start Counter Command (Read=Strobe)
	  1F80202Eh/Write  DUART Set Output Port Bits Command (Set means Out=LOW)
	  1F80202Fh/Read   DUART Stop Counter Command (Read=Strobe)
	  1F80202Fh/Write  DUART Reset Output Port Bits Command (Reset means Out=HIGH)

	Expansion Region 2 - Int/Dip/Post

	  1F802000h 1 DTL-H2000: ATCONS STAT (R)
	  1F802002h 1 DTL-H2000: ATCONS DATA (R and W)
	  1F802004h 2 DTL-H2000: Whatever 16bit data ?
	  1F802030h 1/4 DTL-H2000: Secondary IRQ10 Flags
	  1F802032h 1 DTL-H2000: Whatever IRQ Control ?
	  1F802040h 1 DTL-H2000: Bootmode "Dip switches" (R)
	  1F802041h 1 PSX: POST (external 7 segment display, indicate BIOS boot status)
	  1F802042h 1 DTL-H2000: POST/LED (similar to POST) (other addr, 2-digit wide)   1F802070h 1 PS2: POST2 (similar to POST, but PS2 BIOS uses this address)

	Expansion Region 2 - Nocash Emulation Expansion

	  1F802060h Emu-Expansion ID1 "E" (R)
	  1F802061h Emu-Expansion ID2 "X" (R)
	  1F802062h Emu-Expansion ID3 "P" (R)
	  1F802063h Emu-Expansion Version (01h) (R)
	  1F802064h Emu-Expansion Enable1 "O" (R/W)
	  1F802065h Emu-Expansion Enable2 "N" (R/W)
	  1F802066h Emu-Expansion Halt (R)
	  1F802067h Emu-Expansion Turbo Mode Flags (R/W)

	Expansion Region 3 (default 1 byte, max 2 MBytes)

	  1FA00000h - Not used by BIOS or any PSX games
	  1FA00000h - POST3 (similar to POST, but PS2 BIOS uses this address)

	BIOS Region (default 512 Kbytes, max 4 MBytes)

	  1FC00000h 80000h   BIOS ROM (512Kbytes) (Reset Entrypoint at BFC00000h)

	Memory Control 3 (Cache Control)

	  FFFE0130h 4        Cache Control


	Coprocessor Registers

	  COP0 System Control Coprocessor           - 32 registers (not all used)
	  COP1 N/A
	  COP2 Geometry Transformation Engine (GTE) - 64 registers (most are used)
	  COP3 N/A
	*/
}

void ReleaseMap() {
    // Leak for now, dont care...
}

void SetBaseEntry(MemoryMapEntry* entry,u32 addr_) {
	u32 offset	= addr_ - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        adrMap[offset] = entry;
    }
}

MemoryMapEntry* GetBaseEntry(u32 addr_) {
	u32 offset	= addr_ - 0x1F801000;
	if (offset >= 0 && offset < RANGE_IO) {
        return adrMap[offset];
    } else {
        // Unknown.
        return NULL;
    }
}

