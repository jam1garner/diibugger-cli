@echo off

cd "%~dp0/src"

set CFLAG=-O2 -Wall -ffreestanding -mrvl -mcpu=750 -meabi -mhard-float -fshort-wchar -msdata=none -memb -ffunction-sections -fdata-sections -Wno-strict-aliasing -Wno-write-strings

powerpc-eabi-as -o server_entrypoint.o server_entrypoint.S
powerpc-eabi-gcc %CFLAG% -c -o server.o server.cpp
powerpc-eabi-ld -o server.bin -T server.ld server.o server_entrypoint.o
python build_header.py

powerpc-eabi-as -o installer_entrypoint.o installer_entrypoint.S
powerpc-eabi-gcc %CFLAG% -c -o installer.o installer.cpp
powerpc-eabi-ld -o code.bin installer.o -T installer.ld installer_entrypoint.o

copy code.bin C:\xampp\htdocs\diibugger\code550.bin
cd ..
