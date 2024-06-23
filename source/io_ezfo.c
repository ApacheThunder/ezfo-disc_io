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

#define OMEGA_ROM_UNKNOWN	0x0000
#define OMEGA_ROM_PSRAM		0x9780
#define OMEGA_ROM_NOR		0xFFFF

#define OMEGA_ROM_PAGE_UNKNOWN		0xFFFF
#define OMEGA_ROM_PAGE_PSRAM		0x0200
// #define OMEGA_ROM_PAGE_KERNEL	0x8000
#define OMEGA_ROM_PAGE_KERNEL		0x8002 // 0x8000 is not what EZ Flash kernel actually sets.... This is the real value from source code. Was 0x8000 a typo?

#define OMEGA_SD_BIT_ENABLE     (0x1 << 0)
#define OMEGA_SD_BIT_READ_STATE (0x1 << 1)

#define OMEGA_SD_CTL_ENABLE		OMEGA_SD_BIT_ENABLE
#define OMEGA_SD_CTL_READ_STATE (OMEGA_SD_BIT_ENABLE | OMEGA_SD_BIT_READ_STATE)
#define OMEGA_SD_CTL_DISABLE	0x0

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

static inline void _Omega_SetROMPage( const u16 _page ) {
	*(vu16*)0x9fe0000 = 0xd200;
	*(vu16*)0x8000000 = 0x1500;
	*(vu16*)0x8020000 = 0xd200;
	*(vu16*)0x8040000 = 0x1500;
	*(vu16*)0x9880000 = _page;
	*(vu16*)0x9fc0000 = 0x1500;
}

static inline void _Omega_SetSDControl( const u16 _control ) {
	*(vu16*)0x9fe0000 = 0xd200;
	*(vu16*)0x8000000 = 0x1500;
	*(vu16*)0x8020000 = 0xd200;
	*(vu16*)0x8040000 = 0x1500;
	*(vu16*)0x9400000 = _control;
	*(vu16*)0x9fc0000 = 0x1500;
}

static inline u16 Read_S98NOR_ID() {
	*((vu16*)(FlashBase_S98)) = 0xF0;	
	*((vu16*)(FlashBase_S98+0x555*2)) = 0xAA;
	*((vu16*)(FlashBase_S98+0x2AA*2)) = 0x55;
	*((vu16*)(FlashBase_S98+0x555*2)) = 0x90;
	return *((vu16*)(FlashBase_S98+0xE*2));
}

static inline u32 _Omega_WaitSDResponse() {
	vu16 response;
	u32 waitSpin = 0;
	while ( waitSpin < 0x100000 ) {
		response = *(vu16* )0x9E00000;
		if ( response != 0xEEE1 )return 0;
		waitSpin += 1;
	}
	return 1;
}

/**
 *
 * Disc interface functions
 *
 */

bool _EZFO_startUp() {
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL);
	_Spin(5000);
	if (Read_S98NOR_ID() == 0x223D) {
	#ifdef NDS
		_Omega_SetSDControl(OMEGA_SD_CTL_ENABLE);
	#endif
		return true;
	}
	return false;
}

bool _EZFO_isInserted() { return true; }

bool _EZFO_readSectors(u32 _address, u32 _count, void* _buffer) {
#ifndef NDS
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL); // Change to OS mode
	_Omega_SetSDControl(OMEGA_SD_CTL_ENABLE);
#endif

	u32 readsRemain = 2;
	for (u16 ii = 0; ii < _count; ii += 4) {
		const u16 blocks = (_count - ii > 4) ? 4 : (_count - ii);

		while (readsRemain) {
			*(vu16*)0x9fe0000 = 0xd200;
			*(vu16*)0x8000000 = 0x1500;
			*(vu16*)0x8020000 = 0xd200;
			*(vu16*)0x8040000 = 0x1500;
			*(vu16*)0x9600000 = ((_address + ii) & 0x0000FFFF);
			*(vu16*)0x9620000 = ((_address + ii) & 0xFFFF0000) >> 16;
			*(vu16*)0x9640000 = blocks;
			*(vu16*)0x9fc0000 = 0x1500;
			
			_Omega_SetSDControl(OMEGA_SD_CTL_READ_STATE);
			const u32 response = _Omega_WaitSDResponse();
			_Omega_SetSDControl(OMEGA_SD_CTL_ENABLE);
			if (response && --readsRemain) {
				_Spin(5000);
			} else {
			#ifndef NDS
				dmaCopy((void*)0x9E00000, (void*)(_buffer + ii * 512), (blocks * BYTES_PER_READ));
			#else
				tonccpy((void*)(_buffer + ii * 512), (void*)0x9E00000, (blocks * BYTES_PER_READ));
			#endif
				break;
			}
		}
	}
#ifndef NDS
	_Omega_SetSDControl(OMEGA_SD_CTL_DISABLE);
	_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
#endif
	return true;
}

bool _EZFO_writeSectors(u32 _address, u32 _count, void* _buffer ) {
#ifndef NDS
	_Omega_SetROMPage(OMEGA_ROM_PAGE_KERNEL); // Change to OS mode
#endif

#ifdef _IO_USE_DMA
	DC_FlushRange(_buffer, (_count * BYTES_PER_READ));
#endif
	
	// _Omega_SetSDControl(OMEGA_SD_CTL_READ_STATE);
	for (u16 ii = 0; ii < _count; ii++) {
		const u16 blocks = (_count - ii > 4) ? 4 : (_count - ii);
	#ifndef NDS
		dmaCopy((_buffer + ii * 512), (void*)0x9E00000, (blocks * BYTES_PER_READ));
	#else
		tonccpy((void*)0x9E00000, (_buffer + ii * 512), (blocks * BYTES_PER_READ));
	#endif
		*(vu16*)0x9fe0000 = 0xd200;
		*(vu16*)0x8000000 = 0x1500;
		*(vu16*)0x8020000 = 0xd200;
		*(vu16*)0x8040000 = 0x1500;
		*(vu16*)0x9600000 = ((_address + ii) & 0x0000FFFF);
		*(vu16*)0x9620000 = ((_address + ii) & 0xFFFF0000) >> 16;
		*(vu16*)0x9640000 = (0x8000 + blocks);
		*(vu16*)0x9fc0000 = 0x1500;
		
		_Omega_WaitSDResponse();
	}
	_Spin(3000);
#ifndef NDS
	_Omega_SetSDControl(OMEGA_SD_CTL_DISABLE);
	_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
#endif
	return true;
}

bool _EZFO_clearStatus() { return true; }

bool _EZFO_shutdown() {	
	_Omega_SetSDControl(OMEGA_SD_CTL_DISABLE);
	_Omega_SetROMPage(OMEGA_ROM_PAGE_PSRAM); // Return to original mode
	return true;
}

