#!/bin/sh

cd src

#TODO: use the new devkitPPC Wii U support (-mwup etc.)
export CFLAG="-O2 -Wall -ffreestanding -mrvl -mcpu=750 -meabi -mhard-float -fshort-wchar -msdata=none -memb -ffunction-sections -fdata-sections -Wno-strict-aliasing -Wno-write-strings"

echo server_entrypoint.S
powerpc-eabi-as -o server_entrypoint.o server_entrypoint.S
echo server.cpp
powerpc-eabi-gcc $CFLAG -c -o server.o server.cpp
echo -n "linking ... "
powerpc-eabi-ld -o server.bin -T server.ld server.o server_entrypoint.o
echo server.o
echo code.h
python build_header.py

cd ..
make
