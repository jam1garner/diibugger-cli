
OUTPUT_FORMAT("binary")

SECTIONS {
    entryPoint = 0x1005E040;

    . = 0x011DD000;
    .text : {}
    .rodata : {}
}
