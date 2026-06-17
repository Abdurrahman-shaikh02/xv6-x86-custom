//
// assembler macros to create x86 segments
//

#define SEG_NULLASM                                             \
        .word 0, 0;                                             \
        .byte 0, 0, 0, 0

// The 0xC0 means the limit is in 4096-byte units
// and (for executable segments) 32-bit mode.
#define SEG_ASM(type,base,lim)                                  \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);      \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)),         \
                (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

/*
 * IMPORTANT:
 * So the gdt looks like the following
 * | lim[27:12] (16bits) | base[15:0] (16 bits) |
 * | base[23:16] (8 bits) | permissions , flags (8 bits) | flags, lim[31:28] (8 bits) | base[31:24]
 * Why store only the upper 20 bits of lim ? Because final 12 bits are the 4KB page offset... They are copied as it is...
 * Also fyi : word = 16, long = 32
*/

#define STA_X     0x8       // Executable segment
#define STA_W     0x2       // Writeable (non-executable segments)
#define STA_R     0x2       // Readable (executable segments)
