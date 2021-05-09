// Dear ImGui: standalone example application for DirectX 10
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx10.h"
#include <d3d10_1.h>
#include <d3d10.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#include <stdio.h>

#include "GPUCommandGen.h"
#include "gpu_ref.h"

GPUCommandGen*	gCommandReg = new GPUCommandGen();
GPUCommandGen* getCommandGen() {
	return gCommandReg;
}

/*
DONE 1 - Activate / Deactivate the device.
DONE 4 - Properly display names.
DONE 2 - Time display with graduation in cycle and uS / Proper time scale display.
DONE A - Fix the bug of display ( I think it is not a fetch issue but a stupid flush problem to the rendering)
DONE B - I think I will resolve the issue of hidden/overwritten stuff using color channel : R = Write, W=Green, Blue=DMA.
DONE C - Display filtered events.

TOMORROW
     D - Click on time and display log of events...
 */

// Data
static ID3D10Device*            g_pd3dDevice = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D10RenderTargetView*  g_mainRenderTargetView = NULL;

struct ImageAssetDX10 {
    ID3D10SamplerState*         sampler;
    ID3D10ShaderResourceView*   textureView;
};

/* Load Texture, return handler */
static void* LoadImageRGBA(void* pixels, int width, int height, ImTextureID* textureOut) {
    ImageAssetDX10* pNewAsset = new ImageAssetDX10();
    
    {
        D3D10_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D10_USAGE_DEFAULT;
        desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D10Texture2D* pTexture = NULL;
        D3D10_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = pixels;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

        // Create texture view
        D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
        ZeroMemory(&srv_desc, sizeof(srv_desc));
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        g_pd3dDevice->CreateShaderResourceView(pTexture, &srv_desc, &pNewAsset->textureView);
        pTexture->Release();
    }

    *textureOut = pNewAsset->textureView;

    // Create texture sampler
    {
        D3D10_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        g_pd3dDevice->CreateSamplerState(&desc, &pNewAsset->sampler);
    }
    return pNewAsset;
}

static void  ReleaseImage(void* handler) {
    ImageAssetDX10* pAsset = (ImageAssetDX10*)handler;
    if (pAsset->sampler) { pAsset->sampler->Release(); }
    if (pAsset->textureView) { pAsset->textureView->Release(); }

    delete pAsset;
}

// Forward declarations of helper functions
bool CreateDeviceD3D        (HWND hWnd);
void CleanupDeviceD3D       ();
void CreateRenderTarget     ();
void CleanupRenderTarget    ();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <math.h>
#include "helperString.h"
#include "psxevents.h"
#include "memoryMap.h"
#include "cpu/debugger.h"
#include "FileBrowser/ImGuiFileBrowser.h"

/* Problem :
    - Block have a fixed length, but filling pace may be different.
    Can NOT wait to have a block FILLED until it is added to the queue.
    - Solution : length of life in second for an allocated block.
        If some data are in it already ( != ZERO RECORDS ), the we push it back to the UI.
        0.25 or 0.10 second per block per device seems nice.
        Anyway low fill pace, mean that we waste memory BUT the waste is ridiculous.
 */

class Lock {
public:
    Lock* allocateLock();

    bool init();
    void release();

    void lock();
    void unlock();
protected:
    Lock() {}
};

// --------------- Lock_windows.cpp
#include <windows.h>    
class WindowsLock : public Lock {
public:
    WindowsLock():Lock(),ghMutex(0) {}

    HANDLE ghMutex;
};

Lock* Lock::allocateLock() {
    return new WindowsLock();
}

bool Lock::init() {
    WindowsLock* wLock = (WindowsLock*)this;

    wLock->ghMutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

    return (wLock->ghMutex != NULL);
}

void Lock::release() {
    WindowsLock* wLock = (WindowsLock*)this;
    CloseHandle(wLock->ghMutex);
}

void Lock::lock() {
    WindowsLock* wLock = (WindowsLock*)this;
    DWORD dwWaitResult = WaitForSingleObject( 
            wLock->ghMutex,    // handle to mutex
            INFINITE);  // no time-out interval
}

void Lock::unlock() {
    WindowsLock* wLock = (WindowsLock*)this;
    ReleaseMutex(wLock->ghMutex);
}
// ----------- 


class PSXEventHolderPool {
    bool InitPool(u32 megaByteAlloc);
    void ReleasePool();

    // Full Size (read only , any thread)
    inline
    u32 getTotalPoolSize() { return totalBlockAlloc; }

    // (USB3 Side) Thread Pick rate
    inline
    u32 getCurrentAvailableInPool() {
        return freeIndex;
    }

    // (USB3 Side, lock A) (USB3 will have its own small pool locally without lock.)
    bool allocate(PSXEventHolder** ppStore, u32* amount_InOut);

    // When currentAvailable < xxxx (done by polling by UI thread)
    // UI Thread will gather an amount of block from all the devices and reclaim memory.
    // (UI Thread Side, lock A)
    void backToPool(PSXEventHolder** arrayOfHolders, u32 count);
private:
    u32             totalBlockAlloc;

    PSXEventHolder** freePool;
    PSXEventHolder** usedPool;
    u32             freeIndex;
    u32             usedIndex;
    Lock            lockA;
};

class ExchangeUSB3ToUI {
    bool init       (u32 maxListSize);
    void release    ();

    // (USB3 Side, lock B => Enqueue in list)
    void pushToUI   (Device* pDevice, PSXEventHolder* block);

    // (UI Side, lock B => Swap the list (USB3 gets a nice empty list))
    u32 UIGetNewData(Device** pDevices, PSXEventHolder** blocks);

    Lock lockB;
    PSXEventHolder** listHolder [2];
    Device**         listDevices[2];
    int              indexCurrentList;
    int              sizeCurrentList;
};

bool ExchangeUSB3ToUI::init       (u32 maxListSize) {
    bool res = lockB.init();

    listHolder [0] = new PSXEventHolder*[maxListSize];
    listHolder [1] = new PSXEventHolder*[maxListSize];
    listDevices[0] = new Device*[maxListSize];
    listDevices[1] = new Device*[maxListSize];
    indexCurrentList = 0;
    sizeCurrentList  = 0;

    // TODO Error on new.

    return res;
}

void ExchangeUSB3ToUI::release    () {
    delete[] listHolder[0];
    delete[] listHolder[1];
    delete[] listDevices[0];
    delete[] listDevices[1];
    indexCurrentList = 0;
    sizeCurrentList  = 0;
    lockB.release();
}

// (USB3 Side, lock B => Enqueue in list)
void ExchangeUSB3ToUI::pushToUI(Device* pDevice, PSXEventHolder* block) {
    // By construction, we are sure that there are no more event holder than the list length...
    lockB.lock();
    listHolder [indexCurrentList][sizeCurrentList] = block;
    listDevices[indexCurrentList][sizeCurrentList] = pDevice;
    sizeCurrentList++;
    lockB.unlock();
}

// (UI Side, lock B => Swap the list (USB3 gets a nice empty list))
u32 ExchangeUSB3ToUI::UIGetNewData(Device** pDevices, PSXEventHolder** blocks) {
    lockB.lock();

    // Get the list
    blocks   = listHolder [indexCurrentList];
    pDevices = listDevices[indexCurrentList];
    if (indexCurrentList == 1) {
        indexCurrentList = 0;
    } else {
        indexCurrentList = 1;
    }
    u32 result = sizeCurrentList;

    // Swap to empty list and give the UI the list of blocks...
    sizeCurrentList = 0;
    lockB.unlock();

    return result;
}

// (USB3 Side, lock A) (USB3 will have its own small pool locally without lock.)
bool PSXEventHolderPool::allocate(PSXEventHolder** ppStore, u32* amount_inOut) {
    bool res;

    lockA.lock(); // Mutex stuff...
    u32 amount = *amount_inOut;
    if (freeIndex >= amount) {
        // amountIn does not change.
    } else {
        // Take all the reserve.
        amount = freeIndex;
    }

    res = (amount != 0);

    if (amount) {
        // Move holders to the user pool
        memcpy(ppStore,&freePool[freeIndex - amount],amount * sizeof(PSXEventHolder*));
        freeIndex -= amount;

        // TODO : For now, for performance issue we do not track allocated block
        // Normally could do store the pointers in use :
        /*
        for (int n=0; n < amount; n++) {
            usedPool[ppStore[n]->poolIndex] = ppStore[n];
        }
        */
    }

    lockA.unlock();

    return res;
}

void PSXEventHolderPool::backToPool(PSXEventHolder** arrayOfHolders, u32 amount) {
    lockA.lock(); // Mutex stuff...

    if (amount) {
        // Move holders to the user pool
        memcpy(&freePool[freeIndex],arrayOfHolders,amount * sizeof(PSXEventHolder*));
        freeIndex += amount;

        // TODO : For now, for performance issue we do not track allocated block
        // Normally could do store the pointers in use :
        /*
        for (int n=0; n < amount; n++) {
            usedPool[arrayOfHolders[n]->poolIndex] = NULL;
        }
        */
    }

    lockA.unlock();
}


bool PSXEventHolderPool::InitPool(u32 megaByteAlloc) {
    u64 memoryNeeded = ((u64)megaByteAlloc) * 1024 * 1024;
    u64 sizeofBlock  = sizeof(PSXEventHolder);
    u64 blockNeeded  = memoryNeeded / sizeofBlock;

    // Allocate pointer to pool.
    freePool = new PSXEventHolder*[blockNeeded];
    usedPool = new PSXEventHolder*[blockNeeded];
    if (freePool && usedPool) {
        totalBlockAlloc = blockNeeded;
        for (int n=0; n < blockNeeded; n++) {
            freePool[n] = new PSXEventHolder(); // Memory consumption happens here, no check for now.
            freePool[n]->poolIndex = n;
            usedPool[n] = NULL;
        }
        
        freeIndex = blockNeeded;
        usedIndex = 0;
        lockA.init();
        return true;
    } else {
        totalBlockAlloc = 0;
        delete[] freePool;
        delete[] usedPool;
        return false;
    }
}

void PSXEventHolderPool::ReleasePool() {
    for (int n=0; n < freeIndex; n++) {
        delete freePool[n];
    }

    for (int n=0; n < usedIndex; n++) {
        delete usedPool[n];
    }

    delete[] freePool; freePool = NULL;
    delete[] usedPool; usedPool = NULL;
    totalBlockAlloc = 0;
    usedIndex = 0;
    freeIndex = 0;
    lockA.release();
}

// UI thread stuff : 
void ReclaimEventHolderFromDevices(u64 timeLimit, PSXEventHolder** storage, u32* result) {
    u32 resultCount = 0;
    u32 deviceCount;
    Device** pDevices = getDevices(deviceCount);
    for (int n=0; n < deviceCount; n++) {
        PSXEventHolder* record = pDevices[n]->recordsStart;
        while (true) {
            if (record && record->lastTime < timeLimit) {
                storage[resultCount] = record;
                record = record->nextHolder;
                // Make list empty, timing will be reset by alloc of events.
                storage[resultCount]->eventCount = 0;
                resultCount++;
            } else {
                pDevices[n]->recordsStart = record; // TODO NULL becomes possible, not sure UI is handling this.
                // TODO : Check recordLast also updated correctly.
                break;
            }
        }
    }
    *result = resultCount;
}

void Device::clearEvents() {
    PSXEventHolder* pRoot  = this->recordsStart;
    PSXEventHolder* pParse = pRoot ? pRoot->nextHolder : NULL;

    // Keep only one record, make it empty.
    this->recordsLast = pRoot;
    pRoot->nextHolder = NULL;
    pRoot->eventCount = 0;
    pRoot->startTime  = 0;
    pRoot->lastTime   = 0;

    while (pParse) {
        PSXEventHolder* pNext = pParse->nextHolder;
    /*
        storage[resultCount]->eventCount = 0;
        resultCount++;
    */
        delete pParse;  // TODO RELEASE INTO POOL AGAIN.
        pParse = pNext;
    }
}

void ClearAllDevices() {
    u32 deviceCount;
    Device** pDevices = getDevices(deviceCount);
    for (u32 n=0; n < deviceCount; n++) {
        pDevices[n]->clearEvents();
    }
}


// UI thread stuff : New Records to UI
void InsertEventsToDevices(PSXEventHolder** storageList, Device** deviceList, u32 recordCount) {
    // By construction each device in the list is having its timing in order.
    for (int n=0; n < recordCount; n++) {
        PSXEventHolder* pHolder = *storageList++;
        Device* pDevice         = *deviceList++;

        // TODO : Put assert if last record in device does not match time.

        pDevice->recordsLast->nextHolder = pHolder;
        pDevice->recordsLast             = pHolder;
        pHolder->nextHolder              = NULL; // In case.
    }
}

// TODO : USB3 will have to send PSXEventHolder blocks even if not full with a timelimit
//        => Low frequency event registering still need to have UI reflecting the changes.

class ConvertScale {
	float  width;
	float  timePerPixel;
    double startTime;
    double endTime;
    float  clock;
public:
    void setClock(int clockMhz) { clock = clockMhz; }
	void setDisplaySize(float w) {	width = w;	}

    void setTime(u64 start, u64 end) {
        startTime = start;
        endTime = end;
        setTimeScaleForWidth(end-start);
    }

    inline float getWidth() { return width; }

    // In Clock
    inline float getClock() { return clock; }

    // In Clock
    inline float getStartTime() { return startTime; }

    // In Clock
    inline float getEndTime() { return endTime; }

	void setTimeScaleForWidth(float duration) {
		timePerPixel = width / duration;
	}

	void setTimeScalePerPixel(float duration) { timePerPixel = duration; }

    inline double PixelToTimeDistance(float pixel) {
        return pixel / timePerPixel;
    }

	inline double PixelToTime(float pixel) {
		return (pixel / timePerPixel) + startTime;
	}
	
	/* Rounded to nearest left pixel */
	inline float TimeToPixel(double time) {
		return trunc((time-startTime) * timePerPixel);
	}

	inline float TimeToPixelDistance(double time) {
        return trunc((time / (endTime-startTime)* width));
	}
};

ConvertScale timeScale;

const float DEVICE_TITLE_W = 100.0f;

struct Filter {

    void Reset() {
        deleteMe    = false;
        filterAddr  = false;
        filterValue = false;
        filterRead  = false;
        filterWrite = false;
        filterCondition = false;
        compAddr    = 0;
        compValue   = 0;
        activeFilterColor.x = 1.0f;
        activeFilterColor.y = 1.0f;
        activeFilterColor.z = 1.0f;
        activeFilterColor.w = 1.0f;
        compAddrTxt[0] = 0;
        compValueTxt[0] = 0;
        compCount   = 0;
    }

    bool deleteMe;

    bool filterAddr;
    bool filterValue;
    bool filterRead;
    bool filterWrite;
    bool filterCondition;
    u32 compAddr;
    u32 compValue;
    char compAddrTxt[10];
    char compValueTxt[10];

    ImVec4 activeFilterColor;
    u32    activeFilterColor32;
    int compCount;
    int runCount;
};

PSXEvent** eventList = new PSXEvent*[50000];
int eventListCount = 0;

// Edition Filters
Filter filters[100];
int filterCount = 0;

// Display Filters
Filter displayfilters[100];
int displayfilterCount = 0;

void U32ToHex(u8* buff, u32 v) {
    *buff++ = 'h';
    for (int t=28; t <= 0; t-=4) {
        int p = v>>t & 0xF;
        if (p < 10) {
            *buff++ = '0' + p;
        } else {
            *buff++ = 'A' + p;
        }
    }
    *buff++ = 0;
}

u8* found(u8* start, u8* sub) {
    const char* res;
    if (res = strstr((const char*)start,(const char*)sub)) {
        res += strlen((const char*)sub);
        return (u8*)res;
    }
    return NULL;
}

u64 ValueHexAfter(u8* start, u8* pattern) {
	// Only space and hexa.
	// Stop on others.
	// Only space and 0..9
	// Stop on others.
    u8* res = found(start, pattern);
    u64 result = -1;
    if (res) {
        result = 0;

        while (*res == ' ') {
            res++;
        }

        // parse all spaces
        while ((*res >= '0' && *res <= '9') || (*res >= 'a' && *res <='f') || (*res >= 'A' && *res <= 'F')) {
            int v = 0;
            if (*res >= '0' && *res <= '9') {
                v = (*res - '0');
            } else {
                if (*res >= 'A' && *res <= 'F') {
                    v = (*res - 'A') + 10;
                } else {
                    v = (*res - 'a') + 10;
                }
            }
            result = (result * 16) + v;
            res++;
        }
    }

    return result;
}

u64 ValueIntAfter(u8* start, u8* pattern) {
	// Only space and 0..9
	// Stop on others.
    u8* res = found(start, pattern);
    u64 result = -1;
    if (res) {
        result = 0;

        while (*res == ' ') {
            res++;
        }

        // parse all spaces
        while (*res >= '0' && *res <= '9') {
            result = (result * 10) + (*res - '0');
            res++;
        }
    }
    return result;
}

u32 TextAddrToU32(const char* text) {
    // Remove white space.
    while (*text != 0 && *text <= ' ') {
        text++;
    }
    // Support Hex
    if (*text == 'x' || *text == 'h') {
        text++;
        return ValueHexAfter((u8*)text,(u8*)"");        
    }

    // Support Int
    return ValueIntAfter((u8*)text,(u8*)"");

    // Support Constant name for ADR.
    // ?;

    // Error / undecoded.
    return 0xFFFFFFFF;
}

u32 TextValueToU32(const char* text) {
    // Remove white space.
    while (*text != 0 && *text <= ' ') {
        text++;
    }
    // Support Hex
    if (*text == 'x' || *text == 'h') {
        text++;
        return ValueHexAfter((u8*)text,(u8*)"");        
    }

    // Support Int
    return ValueIntAfter((u8*)text,(u8*)"");
}

void ApplyFilters() {
    u32 deviceCount;
    Device** pDevices = getDevices(deviceCount);
    for (int n=0; n < deviceCount; n++) {

        // Reset per device
        for (int f=0; f < filterCount; f++) {
            // At least one parameter is active.
            filters[f].runCount = 0;
        }

        PSXEvent* pEvent;
        PSXEventHolder* pHolder;
        pDevices[n]->FindEvent(0,&pHolder,&pEvent);
        while (pEvent) {
            pEvent->filterID = 0; // Reset Event default.

            // If multiple filter matches, last filter override.
            for (int f=0; f < filterCount; f++) {
                // At least one parameter is active.
                if (filters[f].filterAddr | filters[f].filterValue | filters[f].filterRead | filters[f].filterWrite) {
                    bool addrMatch  = ((filters[f].filterAddr  &&  (pEvent->adr   == filters[f].compAddr )) || (!filters[f].filterAddr ));
                    bool ValueMatch = ((filters[f].filterValue &&  (pEvent->value == filters[f].compValue)) || (!filters[f].filterValue));
                    bool readMatch  =  (filters[f].filterRead  && ((pEvent->type  == READ_ADR ) || (pEvent->type  == READ_DMA )));
                    bool writeMatch =  (filters[f].filterWrite && ((pEvent->type  == WRITE_ADR) || (pEvent->type  == WRITE_DMA)));

                    bool countMatch = ((filters[f].filterCondition && (filters[f].compCount == filters[f].runCount)) || !filters[f].filterCondition);

                    if (addrMatch && ValueMatch && (readMatch || writeMatch)) {
                        if (countMatch) {
                            pEvent->filterID = f + 1;
                        }
                        filters[f].runCount++;
                    }
                }
            }

            pEvent = pHolder->GetNextEvent(pEvent,&pHolder);
        }
    }

    // [Copy filters for display]
    memcpy(displayfilters, filters, sizeof(Filter) * 100);
    displayfilterCount = filterCount;
}

void ApplyGTEFilter() {
    DeviceSingleOp* pGTE = ((DeviceSingleOp*)getDeviceFromType(GTE_T));

    PSXEvent* pEvent;
    PSXEventHolder* pHolder;
    pGTE->FindEvent(0,&pHolder,&pEvent);
    while (pEvent) {
        pEvent->filterID = 1; // Put near black color by default.(Make then transparent)
        if (pEvent->type == READ_DMA) {
            // 32 instructions.
            u32 opcode = pEvent->value & 0x1F;
            u32 color;
            switch (opcode) {
            case 0x01: color = 0xFF0000FF; break;  // RTPS   15  Perspective Transformation single
            case 0x06: color = 0xFF00FF00; break;  // NCLIP  8   Normal clipping
            case 0x0C: color = 0xFFFF0000; break;  // OP(sf) 6   Outer product of 2 vectors
            case 0x10: color = 0xFFFF00FF; break;  // DPCS   8   Depth Cueing single
            case 0x11: color = 0xFFFFFF00; break;  // INTPL  8   Interpolation of a vector and far color vector
            case 0x12: color = 0xFF00FFFF; break;  // MVMVA  8   Multiply vector by matrix and add vector (see below)
            case 0x13: color = 0xFF00008F; break;  // NCDS   19  Normal color depth cue single vector
            case 0x14: color = 0xFF008F00; break;  // CDP    13  Color Depth Que
            case 0x16: color = 0xFF8F0000; break;  // NCDT   44  Normal color depth cue triple vectors
            case 0x1B: color = 0xFF8F00FF; break;  // NCCS   17  Normal Color Color single vector
            case 0x1C: color = 0xFF8F8F00; break;  // CC     11  Color Color
            case 0x1E: color = 0xFF008F8F; break;  // NCS    14  Normal color single
            case 0x20: color = 0xFF0000FF; break;  // NCT    30  Normal color triple
            case 0x28: color = 0xFF00FF00; break;  // SQR(sf)5   Square of vector IR
            case 0x29: color = 0xFFFF0000; break;  // DCPL   8   Depth Cue Color light
            case 0x2A: color = 0xFFFF00FF; break;  // DPCT   17  Depth Cueing triple (should be fake=08h, but isn't)
            case 0x2D: color = 0xFFFFFF00; break;  // AVSZ3  5   Average of three Z values
            case 0x2E: color = 0xFF00FFFF; break;  // AVSZ4  6   Average of four Z values
            case 0x30: color = 0xFF00008F; break;  // RTPT   23  Perspective Transformation triple
            case 0x3D: color = 0xFF008F00; break;  // GPF(sf)5   General purpose interpolation
            case 0x3E: color = 0xFF8F0000; break;  // GPL(sf)5   General purpose interpolation with base
            case 0x3F: color = 0xFF8F00FF; break;   // NCCT   39  Normal Color Color triple vector
            default  : color = 0xFFFFFFFF; break; // 
            }                   

            pEvent->filterID = color;
        }
        pEvent = pHolder->GetNextEvent(pEvent,&pHolder);
    }
}

#include "imgui_memory_editor.h"
static MemoryEditor mem_edit;
static bool firstMemEdit = true;

u8   bufferRAM [1024*1024*2]; // PSX RAM, consider as cache.
bool bufferRAML[1024*1024*2];
u8   scratchPad[1024];

u16  bufferVRAM[1024*512];
static DebugCtx cpu;

ImU8 ReadCallBack(const ImU8* data, size_t off) {
    // TODO : If data not present, request through protocol... and cache using bufferRAML bit.
    off &= 0x1FFFFFFF;
    if ((off >= 0 && off < 0x200000)) {
        return bufferRAM[off & 0x1FFFFF];
    } else {
        return off & 1 ? 0xAD : 0xDE;
    }
}

void UIMemoryEditorWindow() {
    if (firstMemEdit) {
        firstMemEdit = false;
        mem_edit.ReadFn = ReadCallBack;
    }
//    static int g = 0;
//    g+=4;
//    mem_edit.GotoAddr = g;

    mem_edit.DrawWindow("Memory Editor", bufferRAM, 1024*1024*2);
}

// pEvent->isRead()

void RenderIOAdr(bool isRead, bool isWrite, u32 addr, u32 value) {
    MemoryMapEntry* pEntries = GetBaseEntry(addr);
    if (pEntries) {
        bool isReadEvent  = isRead;
        bool isWriteEvent = isWrite;
        while (pEntries) {
            int rights = pEntries->sizeMask; // 8 / 16
            if ((rights == 0) || ((rights & 8) && isWriteEvent) || ((rights & 16) && isReadEvent)) {
                if (isWriteEvent) {
                    // WRITE RED
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), pEntries->desc);
                } else {
                    // READ GREEN
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), pEntries->desc);
                }
                if (pEntries->bitDescArray) {
                    BitDesc* bitDescArray = pEntries->bitDescArray;
                    while (bitDescArray->desc) {
                        u32 v = (value>>bitDescArray->pos) & ((1<<bitDescArray->count)-1);
                        const char* optDesc = "";
                        EnumDesc* pEnum = bitDescArray->enums;
                        while (pEnum) {
                            if (pEnum->value == v) {
                                optDesc = pEnum->desc;
                                break;
                            }
                            pEnum = pEnum->next;
                        }

                        if (bitDescArray->count == 1) {
                            bool vb = v ? true : false;
                            char buff[256];
                            sprintf(buff,"[%i %s] %s (Bit %i)",v,optDesc,bitDescArray->desc, bitDescArray->pos);
                            ImGui::Checkbox(buff,&vb);
                        } else {
                            if (bitDescArray->format == FORMAT_HEXA) {
                                ImGui::LabelText("","[%x %s] - %s (Bit %i~%i)",v,optDesc,bitDescArray->desc, bitDescArray->pos,bitDescArray->pos + bitDescArray->count - 1);
                            } else {
                                ImGui::LabelText("","[%i %s] - %s (Bit %i~%i)",v,optDesc,bitDescArray->desc, bitDescArray->pos,bitDescArray->pos + bitDescArray->count - 1);
                            }
                        }
                        bitDescArray++;
                    }
                }
            }
            pEntries = pEntries->nextIdentical;
        }
    } else {
        ImGui::LabelText("","Undefined Addr %p",addr);
    }
    ImGui::Separator();
}

MemoryMapEntry* selectedIO    = NULL;
PSXEvent*       selectedEvent = NULL;

void RenderDeviceIOs(DeviceSingleOp* device) {
    u32 sa = device->startAdr;
    u32 ea = device->endAdr;

    MemoryMapEntry* lastEntry = NULL;
    
    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody;
    if (ImGui::BeginTable("table",5, flags)) {
        ImGui::TableSetupColumn("ADDR");
        ImGui::TableSetupColumn("DESC");
        ImGui::TableHeadersRow();

        for (u32 n=sa; n < ea; n++) {
            MemoryMapEntry* entry = GetBaseEntry(n);
            if (entry && entry != lastEntry) {
                ImGui::TableNextRow();
                ImGui::PushID(n);

                // --------- ADDR -----
                ImGui::TableSetColumnIndex(0);
                ImGui::LabelText("","A=%08x",entry->addr);

                // --------- DESC -----
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("SEL")) {
                    selectedIO    = entry;
                    selectedEvent = NULL;
                }
                ImGui::SameLine();
                ImGui::LabelText("",entry->desc);

                ImGui::PopID();

                lastEntry = entry;
            }
        }

        ImGui::EndTable();
    }
}

void UIMemoryIOWindow(PSXEvent* pEvent) {
    ImGui::Begin("IO");                          // Create a window called "Hello, world!" and append into it.
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    if (selectedIO) {
        RenderIOAdr(true,true,selectedIO->addr,GetValueIO(selectedIO));
    } else {
        if (pEvent) {
            RenderIOAdr(pEvent->isRead(), pEvent->isWrite(), pEvent->adr, pEvent->value);
        }
    }

    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        // || device[MC1]->CheckAdr(adr) => Auto display tab.
        if (ImGui::BeginTabItem("MC1"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(MC1_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("JOY/SIO"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(JOY_T));
            RenderDeviceIOs(device);
			device = ((DeviceSingleOp*)getDeviceFromType(SIO_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MC2"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(MC2_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("INT CTRL"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(IRQ_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DMA"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(DMA_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("TMR"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(TMR_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CDR"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(CDR_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GPU"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(GPU_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MDEC"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(MDEC_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("SPU"))
        {
			DeviceSingleOp* device = ((DeviceSingleOp*)getDeviceFromType(SPU_T));
            RenderDeviceIOs(device);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void UIFilterList() {
    ImGui::PushItemWidth(100.0f);
    bool noDelete = true;

    for (int n=0; n < filterCount; n++) {
        ImGui::PushID(n);

        if (ImGui::Button("[DEL]")) {
            filters[n].deleteMe = true;
            noDelete = false;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Match Adr:",&filters[n].filterAddr);
        ImGui::SameLine();
        if (filters[n].filterAddr) {
            ImGui::PushID("A");
            ImGui::InputText("", filters[n].compAddrTxt, 10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            // Hex to int
            filters[n].compAddr = ValueHexAfter((u8*)filters[n].compAddrTxt,(u8*)"");
            ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::Checkbox("Match Val:",&filters[n].filterValue);
        ImGui::SameLine();
        if (filters[n].filterValue) {
            ImGui::PushID("B");
            ImGui::InputText("", filters[n].compValueTxt, 10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            // Hex to int
            filters[n].compValue = ValueHexAfter((u8*)filters[n].compValueTxt,(u8*)"");
            ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::Checkbox("[R]",&filters[n].filterRead);
        ImGui::SameLine();
        ImGui::Checkbox("[W]",&filters[n].filterWrite);
        ImGui::SameLine();
        ImGui::Checkbox("Hit Count:",&filters[n].filterCondition);
        if (filters[n].filterCondition) {
            ImGui::PushID("C");
            ImGui::SameLine();
            ImGui::InputInt("",&filters[n].compCount);
            ImGui::PopID();
        }

        ImGui::SameLine();
        ImGuiColorEditFlags misc_flags = 0;
        if (ImGui::ColorEdit4("MyColor##3", (float*)&filters[n].activeFilterColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | misc_flags)) {
            u32 color32 = 0xFF000000;
            int r = filters[n].activeFilterColor.x * 255;
            int g = filters[n].activeFilterColor.y * 255;
            int b = filters[n].activeFilterColor.z * 255;
            filters[n].activeFilterColor32 = color32 | r | (g<<8) | (b<<16);
        }

        ImGui::PopID();
    }
    ImGui::PopItemWidth();
    if (ImGui::Button("+")) {
        filters[filterCount].Reset();
        filterCount++;
    }

    if (ImGui::Button("Apply Filters")) {
        ApplyFilters();        
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply GTE OpCode Filter")) {
        ApplyGTEFilter();
    }


    if (!noDelete) {
        // Compaction of filters.
        int w = 0;
        for (int src=0; src < filterCount; src++) {
            filters[w] = filters[src];
            if (!filters[src].deleteMe) {
                w++;
            }
        }
        filterCount--;
    }
}

void DrawTimeLine(ImDrawList* draw_list, float x, float y) {
    // Select time scale :
    //  S
    // mS
    // uS
    // Cycle

    // Default to mS
    float deltaT         = ((timeScale.getEndTime()-timeScale.getStartTime())*1000.0) / timeScale.getClock();
    float deltaTPerPixel = deltaT / timeScale.getWidth();
    double divider        = 1000.0 / timeScale.getClock();
    const char* unit = "uS";
    if (deltaT > 1000) {
        unit = "S";
        divider         = timeScale.getClock();
    } else if (deltaT > 1) {
        unit            = "mS";
        divider         = timeScale.getClock() / 1000.0;
    } else if (deltaT > 0.001) {
        unit = "Cycle";
        divider         = 1.0;
    }

    draw_list->AddText(ImVec2(x+5,y + 15.0f),0xFFFFFFFF,unit);

    int markCount = ((int)truncf(timeScale.getEndTime()-timeScale.getStartTime()) / divider) + 2;

    double startMark = truncf(timeScale.getStartTime() / divider) * divider;

    int step;
    if (markCount < 50) {
        step = 1;
    } else if (markCount < 200) {
        step = 2;
    } else if (markCount < 400) {
        step = 4;
    } else if (markCount < 500) {
        step = 10;
    } else if (markCount < 1000) {
        step = 20;
    } else {
        step = markCount / 50;
    }

    int cnt = 0;
    for (int n=0; n <= markCount; n += step) {
        float pixel = timeScale.TimeToPixel(startMark) + DEVICE_TITLE_W + x;
        draw_list->AddLine(ImVec2(pixel,y),ImVec2(pixel,y+50),0xFFFF00FF);
//        if (n & 1) {
            char txt[500];
            sprintf(txt,"%i",(int)(truncf(startMark / divider)));
            draw_list->AddText(ImVec2(pixel,y + ((cnt & 1)*25.0f)),0xFFFFFFFF,txt);
//        }
        startMark += divider * step;
        cnt++;
    }
}

void DrawDevices(ImDrawList* draw_list, u64 startTime, u64 endTime, float baseX, float baseY) {
	u32 deviceCount;

    // Cycle to pixel
    float pixelWidth = timeScale.TimeToPixel(1.0);
    if (pixelWidth < 1.0f) { pixelWidth = 1.0f; }

    float start = -1.0f;
    
	Device** devices = getDevices(deviceCount);

    // -------------------------------------------------
    // Display Device Name
    // -------------------------------------------------

//    u32 n = 0; // deviceCount = 6;
    float devPosY = baseY + 5.0f;

    float totalHeight = 0.0f;
    for (u32 n=0; n < deviceCount; n++) {
		Device*	dev = devices[n];
        totalHeight += dev->height;
    }

    draw_list->AddRectFilled(ImVec2(baseX,baseY),ImVec2(baseX + DEVICE_TITLE_W,baseY + totalHeight),0xFF404040);

    for (u32 n=0; n < deviceCount; n++) {
		Device*	dev = devices[n];
        float heightDevice = dev->height;
        if (dev->visible) {
            draw_list->AddText(ImVec2(baseX,devPosY),0xFFFFFFFF,dev->name);
            devPosY += heightDevice;
        }
    }

    // -------------------------------------------------
    // Display Device Events.
    // -------------------------------------------------
	for (u32 n=0; n < deviceCount; n++) {
		Device*	dev = devices[n];

        if (!dev->visible) { continue; }

        float heightDevice = dev->height;
        float bottomY      = heightDevice + baseY;
        float graphX       = baseX + DEVICE_TITLE_W;

        PSXEventHolder* pHolder;
        PSXEvent*       pEvent;
        float           lastPixel = 0.0f;
        float           firstPixel= 0.0f;

        u32             lastColor = 0;

        u32 colorTable[8192];
        memset(colorTable,0,8192*sizeof(u32));

        bool lockPixel = false;

		if (dev->FindEvent(startTime,&pHolder,&pEvent)) {
            EventType lastType = UNDEF_EVENT;

            while (pEvent) {
                u64 time    = pEvent->time;
                bool before = false;
                if (time < startTime) { before = true; time = startTime; } // Clip for display

                if (time >= endTime) { break; }

                // Convert time to display
                float pixel = timeScale.TimeToPixel(time);
                bool  changeRect = pixel > (lastPixel+1.0f);

                int   pixelI = pixel < 0.0f ? 0 : pixel;

                // Event override color.
                if (pEvent->filterID) {
                    if (pEvent->filterID & 0xFF000000) {
                        // color given by direct event.
                        colorTable[pixelI] = pEvent->filterID;
                    } else {
                        colorTable[pixelI] = displayfilters[pEvent->filterID-1].activeFilterColor32;
                    }
                    lockPixel = true;
                } else {
                    if (!lockPixel) {
                        // Event Base Color.
                        u32 eventColor = 0xFF000000 | ((pEvent->type & 1) ? 0x0000FF : 0x00FF00) | ((pEvent->type & 2) ? 0xFF0000 : 0x0);
                        colorTable[pixelI] |= eventColor;
                    }
                }

                if (lastPixel != pixelI) {
                    lockPixel = false;
                }
                lastPixel = pixelI;

                pEvent = pHolder->GetNextEvent(pEvent,&pHolder);
            }
 //           printf("---------------\n");

            u32 prevColor = 0;
            int startRect = 0;
            int endRect   = 0;
            float pixel = timeScale.TimeToPixel(endTime);
            lastPixel = pixel + 1;
            for (int n=0; n <= lastPixel; n++) {
                u32 cCol = colorTable[n];
                if (cCol==prevColor) {
                    endRect = n+1;
                } else {
                    // Is rect colored ?
                    if (prevColor) {
                        draw_list->AddRectFilled(ImVec2(graphX+startRect, baseY),ImVec2(graphX+endRect,bottomY), prevColor);
                    }
                    startRect = n;
                    endRect   = n+1;
                }
                prevColor = cCol;
            }
        }
        baseY += heightDevice;
	}
}

void SelectionWindow(bool computeSelection, double startSelectionTime, double endSelectionTime, PSXEvent** pEventSelected) {

    static u32 selectionCount = 0;

    if (computeSelection) {
        selectionCount = 0;
        eventListCount = 0;
        if (startSelectionTime < 0.0) { startSelectionTime = 0.0; }
        if (endSelectionTime   < startSelectionTime) { endSelectionTime = startSelectionTime; }
        u64 iStartTime = startSelectionTime;
        u64 iEndTime   = endSelectionTime;

        u32 deviceCount;
        Device** pDevices = getDevices(deviceCount);
        for (int n=0; n < deviceCount; n++) {
            PSXEvent* pEvent;
            PSXEventHolder* pHolder;
            if (pDevices[n]->visible) {
                pDevices[n]->FindEvent(0,&pHolder,&pEvent);
                while (pEvent) {
                    if (pEvent->time >= startSelectionTime) {
                        if (pEvent->time > endSelectionTime) {
                            break;
                        }

                        if (eventListCount < 50000) {
                            eventList[eventListCount++] = pEvent;
                        } else {
                            // Full -> Early stop
                        }
                        selectionCount++;
                    }
                    pEvent = pHolder->GetNextEvent(pEvent,&pHolder);
                }
            }
        }
    }

    ImGui::Begin("Event Detail Window");                          // Create a window called "Hello, world!" and append into it.
    // 50 event per page.
    int pageCount = (eventListCount+49) / 50;
    static int currPage = 0;
    ImGui::SliderInt("Page:",&currPage,0,pageCount-1);
    int pageStart = currPage*50;
    int pageEnd   = pageStart + 50;
    if (pageEnd > eventListCount) { pageEnd = eventListCount; }

    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody;
    if (ImGui::BeginTable("table",5, flags)) {
        ImGui::TableSetupColumn("R/W");
        ImGui::TableSetupColumn("CYCLE");
        ImGui::TableSetupColumn("ADDR");
        ImGui::TableSetupColumn("VALUE");
        ImGui::TableSetupColumn("BASIC DESCRIPTION");
        ImGui::TableHeadersRow();

        // Read/Write
        if (currPage >= 0) {
        for (int n=pageStart; n < pageEnd; n++) {
            PSXEvent* pEvent = eventList[n];
            const char* wr;

            ImGui::TableNextRow();

            if (pEvent->type & WRITE_ADR) {
                wr = "W";
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, 0xFF000080);
            } else {
                wr = "R";
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, 0xFF008000);
            }

            ImGui::PushID(n);

            // --------- Write / Size -----
            ImGui::TableSetColumnIndex(0);
            const char* size="";
            ImGui::LabelText("",wr);
/*
            MemoryMapEntry* pEntry = GetBaseEntry(pEvent->adr);
            if (pEntry) {
            } else {
                ImGui::LabelText("LblAdr","UNKNOWN %p",pEvent->adr);
            }

            ImGui::LabelText("Label",s);
*/
            // --------- Cycle -----
            ImGui::TableSetColumnIndex(1);
            ImGui::LabelText("","@%ull",pEvent->time);
//        if (ImGui::IsItemHovered())

            // --------- ADDR -----
            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button("SEL")) {
                selectedIO        = NULL; // Unselect last displayed IO.
                *pEventSelected   = pEvent;
            }
            ImGui::SameLine();
            ImGui::LabelText("","A=%08x",pEvent->adr);
//        if (ImGui::IsItemHovered())

            // --------- Value -----
            ImGui::TableSetColumnIndex(3);
            ImGui::LabelText("","D=%08x",pEvent->value);

            // --------- Desc -----
            ImGui::TableSetColumnIndex(4);
            MemoryMapEntry* pEntry = GetBaseEntry(pEvent->adr);
            if (pEntry) {
                ImGui::LabelText("",pEntry->desc);
            } else {
                ImGui::LabelText("","UNKNOWN %p",pEvent->adr);
            }

            ImGui::PopID();
        }
        }
        ImGui::EndTable();
    }
    ImGui::LabelText("", "Total Count : %i, Record Count : %i", selectionCount, eventListCount);
    ImGui::End();
}

ImTextureID RenderCommandSoftware(u8* srcBuffer, u64 maxTime) {
    GPUCommandGen* commandGenerator = getCommandGen();

    // Copy VRAM to local
	u8* swBuffer = new u8[1024*1024];
	memcpy(swBuffer, srcBuffer, 1024*1024);

	u32 commandCount;
	u32* p      = commandGenerator->getRawCommands(commandCount);
	u64* pStamp = commandGenerator->getRawTiming(commandCount);

	u8* pGP1 = commandGenerator->getGP1Args();

	// PSX Context.
	GPURdrCtx psxGPU;
	psxGPU.swBuffer		= (u16*)swBuffer;

    if (commandCount != 0) {
	    // Run the rendering of the commands...
	    psxGPU.commandDecoder(p,pStamp,pGP1,commandCount,NULL,NULL, maxTime);
    }

    // Convert 16 bit to 32 bit
    u8* buff32 = new u8[4*512*1024];

	for (int y=0; y < 512; y++) {
		for (int x=0; x < 1024; x++) {
			int adr = (x*2 + y*2048);
			int lsb = swBuffer[adr];
			int msb = swBuffer[adr+1];
			int c16 = lsb | (msb<<8);
			int r   = (     c16  & 0x1F);
			int g   = ((c16>>5)  & 0x1F);
			int b   = ((c16>>10) & 0x1F);
			r = (r >> 2) | (r << 3);
			g = (g >> 2) | (g << 3);
			b = (b >> 2) | (b << 3);
			int base = (x + y*1024)*4;
			buff32[base  ] = r;
			buff32[base+1] = g;
			buff32[base+2] = b;
			buff32[base+3] = 255;
		}
	}

    // Upload to texture.
    static ImTextureID  currTexId = NULL;
    static void* texHandler       = NULL;
    if (currTexId) {
        ReleaseImage(texHandler);
    }

    texHandler = LoadImageRGBA(buff32,1024,512,&currTexId);

    //
    delete[] buff32;
	delete[] swBuffer;

    return currTexId;
}

void VRAMWindow(u64 endTime) {
    ImGui::Begin("VRAM Window");
    static ImTextureID tex = 0;
    if (ImGui::Button("SIM GPU RENDER")) {
        tex = RenderCommandSoftware((u8*)bufferVRAM,endTime);
    }
    ImGui::Image((void*)(intptr_t)tex, ImVec2(1024, 512));
    ImGui::End();
}


//
// Loader Stuff
//
typedef struct
{
    char     name[16];
    uint32_t size;
} t_log_fs_entry;

typedef struct
{
    uint32_t pc;
    uint32_t status;
    uint32_t cause;
    uint32_t lo;
    uint32_t hi;
    uint32_t epc;
    uint32_t reg[31];
} t_log_mips_ctx;

void displayThings() {

    static u64 startTime = 0;
    static u64 endTime   = 1;
    static u64 fullLength= 1;

    u32 deviceCount;
    Device** pDevices = getDevices(deviceCount);

    ImGui::Begin("Timeline");                          // Create a window called "Hello, world!" and append into it.

    bool openDump = false;
    bool openLog = false;

    static imgui_addons::ImGuiFileBrowser browser;
    if (ImGui::Button("LOAD DUMP")) { openDump = true; }
    ImGui::SameLine();
    if (ImGui::Button("LOAD LOG"))  { openLog  = true; }
    if (openDump) { ImGui::OpenPopup("LOAD_DUMP"); }
    if (openLog ) { ImGui::OpenPopup("LOAD_LOG");  }

    if (browser.showFileDialog("LOAD_DUMP",imgui_addons::ImGuiFileBrowser::DialogMode::OPEN,ImVec2(500,300),".dump")) {
        const char* fileFullPath = browser.selected_path.c_str();
        FILE* f = fopen(fileFullPath,"rb");
        fseek(f,0,SEEK_END);
        int size = ftell(f);
        u8* data = new u8[size];

        fseek(f,SEEK_SET,0);
        fread(data,1,size,f);

        // Load Each chunk.
        u8* parse = data;
        while (parse < &data[size]) {
            t_log_fs_entry* pEntry = (t_log_fs_entry*)parse;

            if (!strcmp(pEntry->name,"mips_ctx.bin")) {
                t_log_mips_ctx* pMips = (t_log_mips_ctx*)(parse + sizeof(t_log_fs_entry));
                cpu.PC  = pMips->pc;
                cpu.EPC = pMips->epc;
                cpu.viewMemory = cpu.PC;

                cpu.reg[0] = 0;
                for (int n=1; n <= 31; n++) { // Only 0..30 (1..31
                    cpu.reg[n] = pMips->reg[n-1];
                }
                cpu.lo = pMips->lo;
                cpu.hi = pMips->hi;
                cpu.cause  = pMips->cause;
                cpu.status = pMips->status;
            }

            if (!strcmp(pEntry->name,"io.bin")) {
                u8* src = &parse[sizeof(t_log_fs_entry)];
                for (int n=0; n < pEntry->size; n++) {
                    SetIOsValueByte(n + 0x1F801000,src[n]);
                }
            }

            if (!strcmp(pEntry->name,"scratch.bin")) {
                memcpy(scratchPad,&parse[sizeof(t_log_fs_entry)],2048);
            }

            if (!strcmp(pEntry->name,"vram.bin")) {
                memcpy(bufferVRAM,&parse[sizeof(t_log_fs_entry)],1024*1024);
            }

            if (!strcmp(pEntry->name,"main.bin")) {
                memcpy(bufferRAM,&parse[sizeof(t_log_fs_entry)],1024*1024*2);
                // all Cached
                memset(bufferRAML,1,1024*1024*2);
            }

            if (strcmp(pEntry->name,"bios.bin")) {
            }

            parse += pEntry->size + 16 + 4;            
        }

        delete[] data;
        fclose(f);
    }

    if (browser.showFileDialog("LOAD_LOG",imgui_addons::ImGuiFileBrowser::DialogMode::OPEN,ImVec2(500,300),".txt")) {
        ClearAllDevices();
        const char* fileFullPath = browser.selected_path.c_str();
        parserSIMLOG(fileFullPath,&fullLength);
        endTime = fullLength;
    }

    for (int n=0; n < deviceCount; n++) {
        if (n != 0) { ImGui::SameLine(); }
        ImGui::Checkbox(pDevices[n]->name,&pDevices[n]->visible);
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO io = ImGui::GetIO();

    // io.MouseWheel -6..+6 , normal is -1..+1
    // 
    if (ImGui::IsMousePosValid())
        ImGui::Text("Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
    else
        ImGui::Text("Mouse Position: <invalid>");

    bool     leftDown = io.MouseDown[0];
    bool     rightDown= io.MouseDown[1];
    ImVec2 RenderSize = ImGui::GetWindowSize();

    // Time Display
const float TIMESCALE_H = 50.0f;
    float totalHeight = TIMESCALE_H;

    for (int n=0; n < deviceCount; n++) {
        if (pDevices[n]->visible) {
            totalHeight += pDevices[n]->height;
        }
    }

    // 
    ImVec2 p = ImGui::GetCursorScreenPos();
    float mouseWheel = io.MouseWheel;
    float  posZoom = (io.MousePos.x - (p.x + DEVICE_TITLE_W)); 

    float zoom = 1.0f;

    if (mouseWheel > 0.0f) {
        zoom = 1.05f;
    }

    if (mouseWheel < 0.0f) {
        zoom = 0.95f;
    }


    static bool dragActive = false;
    static float dragStart;
    static float dragST, dragET;

    bool validPosZoom = (posZoom>=0) && (posZoom <= RenderSize.x);

    if (zoom != 1.0f && validPosZoom) {
        float zoomTime = timeScale.PixelToTime(posZoom);
        float distTimeL= zoomTime - startTime;
        float distTimeR= endTime  - zoomTime;
        float distTimeZL= distTimeL * (zoom - 1.0f);
        float distTimeZR= distTimeR * (zoom - 1.0f);

        float zoomTimeS= zoomTime * zoom;
        
        float dL       = zoom * 1000.0f;

        if ((distTimeZL > 0.0f) || ((distTimeZL < 0.0f) && (startTime >= distTimeZL))) {
            distTimeZL += startTime;
            if (distTimeZL < 0.0f) {
                distTimeZL = 0.0f;
            }
            startTime = (u64)distTimeZL;
        }
        if (((distTimeZR > 0.0f)  && (endTime >= distTimeZR)) || ((distTimeZR < 0.0f))) {
            distTimeZR = (float)endTime - distTimeZR;
            if (distTimeZR > fullLength) {
                distTimeZR = fullLength;
            }
            endTime = (u64)distTimeZR;
        }
    }

    timeScale.setDisplaySize(RenderSize.x);
    timeScale.setClock(30000000);
    timeScale.setTime(startTime,endTime);

//    draw_list->PushClipRect(ImVec2(p.x,p.y),ImVec2(p.x+RenderSize.x,p.y+totalHeight));
    DrawTimeLine(draw_list,p.x,p.y);
    DrawDevices(draw_list,startTime,endTime, p.x,p.y + TIMESCALE_H);
//    draw_list->PopClipRect();

    static bool  selectActive = false;
    static double startSelectionTime = 0.0f;
    static double startPosSelect = 0.0f;
    static float endSelectionTime = 0.0f;
    static float endPosSelect = 0.0f;

    bool computeSelection = false;

    // End Drag
    if (!leftDown) {
        dragActive = false;
    }
    if (!rightDown && selectActive) {
        selectActive = false;
        // Update / Recompute Display selection...
        computeSelection = true;
    }

    // --------------------------------
    // Display Selection on timeline
    // --------------------------------
    float displayXS = timeScale.TimeToPixel(startSelectionTime);
    float displayXE = timeScale.TimeToPixel(endSelectionTime);
    if (displayXS == displayXE) {
        displayXE += 1.0f;
    }
    draw_list->AddRectFilled(
        ImVec2((p.x + DEVICE_TITLE_W) + displayXS,p.y),
        ImVec2((p.x + DEVICE_TITLE_W) + displayXE,p.y+totalHeight),
        0x80808080
    );


    ImGui::InvisibleButton("##game", ImVec2(4096, totalHeight));

    if (rightDown && selectActive) {
        endSelectionTime = timeScale.PixelToTime(posZoom);
        endPosSelect = posZoom;
    }

    if (ImGui::IsItemActive() && validPosZoom) {

        // Drag
        if (leftDown && dragActive) {
            float dragPixel = posZoom - dragStart;
            float timeDrag  = timeScale.PixelToTimeDistance(dragPixel);
            if (timeDrag > dragST) {
                timeDrag = dragST;
            }

            if (dragET - timeDrag > fullLength) {
                timeDrag = fullLength - dragET;
            }

            float newStartTime = dragST - timeDrag;
            if (newStartTime < 0.0f)       { newStartTime = 0.0f; }
            if (newStartTime > fullLength) { newStartTime = fullLength; }

            float newEndTime   = dragET - timeDrag;

            if (newEndTime   < 0.0f)       { newEndTime   = 0.0f; }
            if (newEndTime   > fullLength) {
                newEndTime   = fullLength;
            }

            startTime = newStartTime;
            endTime   = newEndTime;
        }
    }

    // Start Drag
    if (validPosZoom) {
        if (leftDown) {
            dragActive = true;
            dragStart  = posZoom;
            dragST     = startTime;
            dragET     = endTime;
        }
        if (rightDown && !selectActive) {
            selectActive = true;
            startPosSelect  = posZoom;
            startSelectionTime = timeScale.PixelToTime(posZoom);
        }
    }

//    ImGui::Dummy();

    ImGui::LabelText("Label","Start:%i End:%i",(int)startTime,(int)endTime);

    UIFilterList();

    ImGui::End();

    UIMemoryEditorWindow();

    static u32 selectAdr   = -1;
    static u32 selectValue = -1;
    SelectionWindow(computeSelection, startSelectionTime, endSelectionTime, &selectedEvent);

    UIMemoryIOWindow(selectedEvent);

    VRAMWindow(startSelectionTime);

    cpu.cbRead = ReadCallBack;
    debuggerWindow(&cpu);
}

// Main code
int main(int, char**)
{
    createDevices();
    // After create device, register the register map.
    CreateMap();

/*
    u32 devCount;
    Device** pDevices = getDevices(devCount);
    for (int n = 0; n < 10000; n++) {
        ((DeviceSingleOp*)pDevices[0])->Write(n * n, 0,0,0xF);
        ((DeviceSingleOp*)pDevices[0])->Read (n * n + 50, 0,0,0xF);
    }
    lastTime = 10001 * 100;
*/
    // parserSIMLOG("sim-82.txt", &lastTime);
    // Default reset.
    ApplyFilters();

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX10 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX10_Init(g_pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX10_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        displayThings();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
#if 0
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }
#endif

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        g_pd3dDevice->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDevice->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    ImGui_ImplDX10_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
    if (D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, D3D10_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D10Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
