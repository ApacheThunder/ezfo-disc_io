/*

  io_ezfo.c

  Hardware Routines for reading the EZ Flash Omega filesystem

*/


#ifndef NDS
	#if defined ARM9 || defined ARM7
		#define NDS
	#endif
#endif

#ifdef NDS
	#include <nds/ndstypes.h>
	#include <nds/system.h>
	#include <nds/dma.h>
	#include "tonccpy.h"
#else
	#include "gba_dma.h"
	#include "gba_types.h"
#endif

#ifndef NULL
	#define NULL 0
#endif

#define BYTES_PER_READ 512


#define SET_info_offset		0x7B0000
#define FlashBase_S71		0x08000000
#define FlashBase_S98		0x09000000
#define SDBufferAddress		0x09E00000

#define OMEGA_ROM_UNKNOWN	0x0000
#define OMEGA_ROM_PSRAM		0x9780
#define OMEGA_ROM_NOR		0xFFFF

#define OMEGA_MAGIC1		0xD200
#define OMEGA_MAGIC2		0x1500

#define OMEGA_ROM_PAGE_UNKNOWN		0xFFFF
#define OMEGA_ROM_PAGE_PSRAM		0x0200
// #define OMEGA_ROM_PAGE_KERNEL	0x8000
#define OMEGA_ROM_PAGE_KERNEL		0x8002 // 0x8000 is not what EZ Flash kernel actually sets.... This is the real value from source code. Was 0x8000 a typo?

#define OMEGA_SD_BIT_ENABLE     (0x1 << 0)
#define OMEGA_SD_BIT_READ_STATE (0x1 << 1)

#define OMEGA_SD_CTL_ENABLE		OMEGA_SD_BIT_ENABLE
#define OMEGA_SD_CTL_READ_STATE (OMEGA_SD_BIT_ENABLE | OMEGA_SD_BIT_READ_STATE)
#define OMEGA_SD_CTL_DISABLE	0x0

static volatile u16 OMEGA_SD_WAIT = 0x1388;
static volatile u16 OMEGA_SDWRITE_WAIT = 0x0BB8;
static volatile u32 OMEGA_WAITRESPONSE = 0x100000;


/**
 *
 * Miscellaneous utility functions
 *
 */

static inline void _Spin( u32 _cycles ) {
	while (_cycles--)asm ("nop");
}

/**
 *
 * Omega device functions
 *
 */

static inline void _Omega_SetROMPage(const u16 _page) {
	*(vu16*)0x9fe0000 = OMEGA_MAGIC1;
	*(vu16*)FlashBase_S71 = OMEGA_MAGIC2;
	*(vu16*)0x8020000 = OMEGA_MAGIC1;
	*(vu16*)0x8040000 = OMEGA_MAGIC2;
	*(vu16*)0x9880000 = _page;
	*(vu16*)0x9fc0000 = OMEGA_MAGIC2;
}

/*static inline void _Omega_SetSDControl( const u16 _control ) {
	*(vu16*)0x9fe0000 = 0xd200;
	*(vu16*)0x8000000 = 0x1500;
	*(vu16*)0x8020000 = 0xd200;
	*(vu16*)0x8040000 = 0x1500;
	*(vu16*)0x9400000 = _control;
	*(vu16*)0x9fc0000 = 0x1500;
}*/

u16 Read_S98NOR_ID() {
	*((vu16*)FlashBase_S98) = 0xF0;	
	*((vu16*)(FlashBase_S98 + (0x555 * 2))) = 0xAA;
	*((vu16*)(FlashBase_S98 + (0x2AA * 2))) = 0x55;
	*((vu16*)(FlashBase_S98 + (0x555 * 2))) = 0x90;
	return *((vu16*)(FlashBase_S98 + (0xE * 2)));
}

/*static inline u32 _Omega_WaitSDResponse() {
	vu16 response;
	u32 waitSpin = 0;
	while (waitSpin < 0x100000) {
		response = *(vu16*)0x9E00000;
		if (response != 0xEEE1)return 0;
		waitSpin += 1;
	}
	return 1;
}*/

void SetSDControl(u16 control) {
	*(u16*)0x9fe0000 = OMEGA_MAGIC1;
	*(u16*)FlashBase_S71 = OMEGA_MAGIC2;
	*(u16*)0x8020000 = OMEGA_MAGIC1;
	*(u16*)0x8040000 = OMEGA_MAGIC2;
	*(u16*)0x9400000 = control;
	*(u16*)0x9fc0000 = OMEGA_MAGIC2;
}

void SD_Enable(void) { SetSDControl(1); }
void SD_Read_state(void) { SetSDControl(3); }
void SD_Disable(void) { SetSDControl(0); }
u16 SD_Response(void) {	return *(vu16*)SDBufferAddress; }

u32 Wait_SD_Response() {
	vu16 res;
	u32 count = 0;
	while(1) {
		res = SD_Response();
		if (res != 0xEEE1)return 0;
		count++;
		if (count > OMEGA_WAITRESPONSE)return 1;
	}	
}

/**
 *
 * Disc interface functions
 *
 */

bool _EZFO_startUp() {
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL);
	_Spin(OMEGA_SD_WAIT);
	if (Read_S98NOR_ID() == 0x223D) {
		#ifndef NDS
			_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
		#endif
		return true;
	}
	#ifndef NDS
		_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
	#endif
	return false;
}

bool _EZFO_isInserted() { return true; }

bool _EZFO_readSectors(u32 address, u32 count, void* SDbuffer) {
	
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL); // Change to OS mode
	SD_Enable();

	u16 i;
	u16 blocks;
	u32 res;
	u32 times=2;
	for(i = 0; i < count; i += 4) {
		blocks = (count - i > 4) ? 4: (count - i);
		read_again:
		*(vu16*)0x9fe0000 = OMEGA_MAGIC1;
		*(vu16*)FlashBase_S71 = OMEGA_MAGIC2;
		*(vu16*)0x8020000 = OMEGA_MAGIC1;
		*(vu16*)0x8040000 = OMEGA_MAGIC2;
		*(vu16*)0x9600000 = ((address + i) & 0x0000FFFF);
		*(vu16*)0x9620000 = (((address + i) & 0xFFFF0000) >> 16);
		*(vu16*)0x9640000 = blocks;
		*(vu16*)0x9fc0000 = OMEGA_MAGIC2;
		SD_Read_state();
		res = Wait_SD_Response();
		SD_Enable();
		if(res == 1) {
			times--;
			if(times) {
				_Spin(OMEGA_SD_WAIT);
				goto read_again;
			}			
		}
		#ifndef NDS
			dmaCopy((void*)SDBufferAddress, (SDbuffer + i * BYTES_PER_READ), (blocks * BYTES_PER_READ));
		#else
			tonccpy((void*)(SDbuffer + i * BYTES_PER_READ), (void*)SDBufferAddress, (blocks * BYTES_PER_READ));
		#endif
	}
	SD_Disable();
	#ifndef NDS
		_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
	#endif
	return true;
}


bool _EZFO_writeSectors(u32 address, u32 count, const void* SDbuffer) {
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL); // Change to OS mode
	
	for(int I = 0; I < count; ++I, SDbuffer += 512, ++address) {
		SD_Enable();
		SD_Read_state();
		u16 blocks;
		u32 res;
		if(true) {
			blocks = 1;;
		#ifdef NDS
			tonccpy((u16*)SDBufferAddress, (void*)(SDbuffer), (BYTES_PER_READ));
		#else
			dmaCopy((SDbuffer + I * BYTES_PER_READ), (void*)SDBufferAddress, (blocks * BYTES_PER_READ));
		#endif
			*(vu16*)0x9fe0000 = OMEGA_MAGIC1;
			*(vu16*)FlashBase_S71 = OMEGA_MAGIC2;
			*(vu16*)0x8020000 = OMEGA_MAGIC1;
			*(vu16*)0x8040000 = OMEGA_MAGIC2;
			*(vu16*)0x9600000 = ((address) & 0x0000FFFF);
			*(vu16*)0x9620000 = (((address) & 0xFFFF0000) >> 16);
			*(vu16*)0x9640000 = (0x8000 + blocks);
			*(vu16*)0x9fc0000 = OMEGA_MAGIC2;
			res = Wait_SD_Response();
			if(res == 1)return false;
		}
		_Spin(0x0BB8);
		
		SD_Disable();
	}
	
	#ifndef NDS
		_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
	#endif
	return true;
}

bool _EZFO_clearStatus() { return true; }

bool _EZFO_shutdown() {
	SD_Disable();
	#ifndef NDS
		// _Omega_SetSDControl(OMEGA_SD_CTL_DISABLE);
		_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
	#endif
	return true;
}

