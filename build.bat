@echo off

cd "%~dp0/src"

set CFLAG=-O2 -Wall -ffreestanding -mrvl -mcpu=750 -meabi -mhard-float -fshort-wchar -msdata=none -memb -ffunction-sections -fdata-sections -Wno-strict-aliasing -Wno-write-strings

powerpc-eabi-as -o server_entrypoint.o server_entrypoint.S
powerpc-eabi-gcc %CFLAG% -c -o server.o server.cpp
powerpc-eabi-ld -o server.bin -T server.ld server.o server_entrypoint.o
python build_header.py

cd ..
make
