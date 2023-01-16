# tmce64
Terminal Mode Commodore 64 Emulator

This is an emulator to run Commodore 64 programs, especially those made in BASIC, in a Linux terminal.

Some features:
* 6510 CPU emulation passes [Dormann tests](https://github.com/Klaus2m5/6502_65C02_functional_tests) and some Lorenz tests.
* Curses based UI with full 256-color support if available.
* Approximate PETSCII to ASCII conversion, for both upper/lower case sets.
* Debugger with CPU trace, stack trace and breakpoint support.
* CIA timer support, as needed for random numbers in games.
* Commodore IEC serial bus emulation, used for disk drives.
* Limited support for D64 disk images. (Read-only.)
* Run emulation in full speed (warp mode) or closer to original C64 speed.
* Can load PRG programs directly by injecting them into memory.
* Needs the ROMs from the [VICE emulator](https://vice-emu.sourceforge.io/).

Screenshot:  
![Commodore 64 BASIC](tmce64-basic.png)

Information on my blog:
* [Terminal Mode Commodore 64 Emulator](https://kobolt.github.io/article-177.html)
* [Terminal Mode Commodore 64 Emulator Update](https://kobolt.github.io/article-195.html)
* [Terminal Mode Commodore 64 Emulator Disk Support](https://kobolt.github.io/article-199.html)

YouTube videos:
* [Commodore 64 Rolling Demo](https://www.youtube.com/watch?v=o3XeKIJRKow)
* [Attack of the PETSCII Robots](https://www.youtube.com/watch?v=_gk-gmeht_M)

