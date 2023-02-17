# tmce64
Terminal Mode Commodore 64 Emulator

This is an emulator to run Commodore 64 programs, especially those made in BASIC, in a Linux terminal.

Features:
* 6510 CPU emulation passes [Dormann tests](https://github.com/Klaus2m5/6502_65C02_functional_tests) and some Lorenz tests.
* Curses based UI with full 256-color support if available.
* Approximate PETSCII to ASCII conversion, for both upper/lower case sets.
* SID support through [reSID version 0.16](http://www.zimmers.net/anonftp/pub/cbm/crossplatform/emulators/resid/index.html) if available.
* Joystick support through [SDL2](https://www.libsdl.org/).
* Debugger with CPU trace, stack trace and breakpoint support.
* CIA timer support, as needed for random numbers in games.
* Commodore IEC serial bus emulation, used for disk drives.
* Limited support for D64 disk images. (Read-only.)
* Run emulation in full speed (warp mode) or closer to original PAL C64 speed.
* VIC-II raster interrupt, to help some demos work.
* Can load PRG programs directly by injecting them into memory.
* Needs the ROMs from the [VICE emulator](https://vice-emu.sourceforge.io/) or similar.

Known issues and missing features:
* Sprites are not supported, so many games are probably completely unplayable.
* Keyboard input only works through KERNAL routines since the CIA keyboard matrix is not fully emulated.
* VIC-II versus CPU timings are off since the VIC cannot "stun" the CPU.
* reSID buffers sometimes become unsynchronized after warp mode, causing bad scratching audio.
* Only joystick in port 2 currently handled.
* Accuracy in general... Lots of hit and miss with demos and games.

Information on my blog:
* [Terminal Mode Commodore 64 Emulator](https://kobolt.github.io/article-177.html)
* [Terminal Mode Commodore 64 Emulator Update](https://kobolt.github.io/article-195.html)
* [Terminal Mode Commodore 64 Emulator Disk Support](https://kobolt.github.io/article-199.html)
* [Terminal Mode Commodore 64 Emulator SID Support](https://kobolt.github.io/article-212.html)

YouTube videos:
* [Commodore 64 Rolling Demo](https://www.youtube.com/watch?v=o3XeKIJRKow)
* [Attack of the PETSCII Robots](https://www.youtube.com/watch?v=_gk-gmeht_M)
* [Future Composer 2.0 Intro](https://www.youtube.com/shorts/u5-h6eYAdSM)
* [In Full Gear](https://www.youtube.com/shorts/0011a7Ipztk)
* [Digiloi](https://www.youtube.com/shorts/vJPg_WGLFfQ)

