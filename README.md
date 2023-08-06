## chip-8-e
A basic [CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) emulator

## How it works?
A standard CHIP-8 has memory, registers, stack, timers, input, graphics, and sound. Each individual has its own opcode and in sum, CHIP-8 has 35 opcodes which are all stored as a [big-endian](https://en.wikipedia.org/wiki/Endianness) value.

A standard CHIP-8 has these instructions and opcode
```
0nnn - SYS addr
00E0 - CLS
00EE - RET
1nnn - JP addr
2nnn - CALL addr
3xkk - SE Vx, byte
4xkk - SNE Vx, byte
5xy0 - SE Vx, Vy
6xkk - LD Vx, byte
7xkk - ADD Vx, byte
8xy0 - LD Vx, Vy
8xy1 - OR Vx, Vy
8xy2 - AND Vx, Vy
8xy3 - XOR Vx, Vy
8xy4 - ADD Vx, Vy
8xy5 - SUB Vx, Vy
8xy6 - SHR Vx {, Vy}
8xy7 - SUBN Vx, Vy
8xyE - SHL Vx {, Vy}
9xy0 - SNE Vx, Vy
Annn - LD I, addr
Bnnn - JP V0, addr
Cxkk - RND Vx, byte
Dxyn - DRW Vx, Vy, nibble
Ex9E - SKP Vx
ExA1 - SKNP Vx
Fx07 - LD Vx, DT
Fx0A - LD Vx, K
Fx15 - LD DT, Vx
Fx18 - LD ST, Vx
Fx1E - ADD I, Vx
Fx29 - LD F, Vx
Fx33 - LD B, Vx
Fx55 - LD [I], Vx
Fx65 - LD Vx, [I]

# Taken from: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#0.0
```
SuperCHIP on the other hand has several more instructions, however, there is no clear documentation I could find.

For further information, please see
1. [Guide to making a CHIP-8 emulator](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
2. [CHIP-8 technical reference](http://web.archive.org/web/20230806062708/http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)

Note that, both documentations aren't about only CHIP-8 specifications, for technical judgment, make sure you've verified any ambiguities in these implementations.

### Important notes
This implementation of CHIP-8 only implements the CHIP-8 and doesn't implement all SCHIP instructions. However, most SCHIP ROMS should just work fine. This also isn't the most accurate implementation and is only intended for learning purposes.
