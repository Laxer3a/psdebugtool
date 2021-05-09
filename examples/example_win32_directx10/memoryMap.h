#ifndef MEMORY_MAP_PSX_H
#define MEMORY_MAP_PSX_H

#include "myTypes.h"

enum BitDescFormat {
    FORMAT_HEXA,
    FORMAT_INT,
};

struct EnumDesc {
    EnumDesc*   next;
    int         value;
    const char* desc;
    bool        invalid;
};

struct BitDesc {
	int			pos;
	int			count;
    bool        onlyConstant;
    unsigned int constantValue;
    BitDescFormat format;
	const char*	desc; // Can be NULL. (last entry)
    EnumDesc*   enums;
};

struct MemoryMapEntry {

    void Reset(const char* desc_, u32 addr_, u32 accessMask)
    { nextIdentical = NULL;
      bitDescArray = NULL;
      desc = desc_;
      addr = addr_;
      sizeMask = accessMask;
    }

    MemoryMapEntry* nextIdentical;

	const char*	desc;
	u32			addr;
	u32			sizeMask;
    u32         defined;

	BitDesc*	bitDescArray;
};

MemoryMapEntry* GetBaseEntry(u32 addr_);
u8              GetIOsValue(u32 addr_);
void            SetIOsValueByte(u32 addr_, u8 v);
u32             GetValueIO(MemoryMapEntry* entry);

void            SetBaseEntry(MemoryMapEntry* pEntry, u32 addr_);
void            CreateMap();
void            ReleaseMap();

#endif
