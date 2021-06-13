# tmce64
Terminal Mode Commodore 64 Emulator

So far this is just a proof of concept and not so usable yet.
But the 6502 CPU emulation should be complete, and C64 BASIC can be started.

To support the C64 architecture, some simple memory bank switching support has been implemented, and hooks are placed when reading and writing to certain memory locations to capture the user input and output.

