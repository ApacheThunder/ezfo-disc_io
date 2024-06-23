# ezfo-disc_io DLDI for DS Homebrew

This is a port of the EZ Flash libfat code for DLDI spec used by NDS homebrew and is meant to be used by homebrew booted from a slot1 device like an R4, etc.


Changes:

* DMA code limited to GBA compile flag as it will not operate safely in DS mode due to arm9 cache system.
* tonccpy used in place of dmaCopy for NDS mode.
* Altered detection routine in startup function. Should be more reliable (for DS mode at least. Maybe it worked better for GBA homebrew but I found this to be unreliable for DS mode stuff)
* setRomPage value for Kernel mode alrted to be 0x8002 as rerferenced in the EZFlash Kernel source code. 0x8000....not sure what the difference is with that one. Maybe 0x8000 is actually bootloader mode?
* Disabling of SD and setting back to PSRam mode will no only occur if DLDI is compiled for GBA as the constant mode switching is not nessecery.


Original readme posted below. Issues listed originally might still apply if used for GBA homebrew but as this DLDI port of the code is meant for DS homebrew the issues mentioned aren't relevent for this version of the driver unless compiled for GBA homebrew. ;)

# ezfo-disc_io
libfat Gameboy Advance Disc Interface for EZ Flash Omega flash cartridge

# Usage #

Compile io_ezfo.c and include io_ezfo.h in your FAT initialisation.

The EZ Flash Omega SD can be mounted with libfat using:
```c
const bool ismount = fatMountSimple( "fat", &_io_ezfo ); // _io_ezfo from io_ezfo.h
if ( ismount ) {
  const int cderr = chdir( "fat:/" ); // Change working directory to fat:/ device
  if ( cderr == 0 ) {
    // Mount success
  } else {
    // Change directory fail
  }
} else {
  // Mount fail
}
```

fatInitDefault can still be used to keep compatibility with other flash-carts:
```c
if ( fatInitDefault() || ( fatMountSimple( "fat", &_io_ezfo ) && chdir( "fat:/" ) == 0 ) ) {
  // Mount success
} else {
  // Mount fail
}
```

# Known issues #

## PSRAM only ##

For now, only ROMs copies to EZ Flash Omega's PSRAM are supported. NOR ROMs definitely can work, however there is a lot of complexity involved with switching back to NOR game mode from  OS kernel mode related to detecting ROM binaries.

When a better method for retrieving a game's NOR page is discovered this issue can be easily solved.

## Code is copied to EWRAM ##

Switching to OS kernel mode changes the contents of the ROM, so it cannot be used with the memory map. This means execution of EZ Flash Omega FAT read/write must happen in memory, not in ROM.

This is not ideal as it means some of your EWRAM will be taken up by EZFO disc_io, even for other flash carts.

The solution to this is to pre-compile the read/write routines as a binary and copy that binary from ROM into memory when it is needed (and then free it after).

## Working directory is not binary's location on disk ##

This is a general problem with flash carts and homebrew. Not much can be done about this, unless flash carts start exposing the executable path.

Means everything generally must be done at the root of the filesystem. 