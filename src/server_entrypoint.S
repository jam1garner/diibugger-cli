
.include "asm_setup.S"

#Define a macro so we don't have to repeat the same
#code in every single function we're patching
.macro patch name, stackOffs, realOffs, returnOffs
    mr r5, r31
    bl Patch_\name      #Call our own implementation
    cmpwi r3, -1        #Is it a patched file?
    bne return_\name    #If yes, return

    mr r3, r29          #Otherwise, restore
    mr r4, r30          #a few registers
    addi r5, r1, \stackOffs
    ba \name + \realOffs    #And call the real function

return_\name:
    ba \name + \returnOffs
.endm

.set FSGetStat, 0x01070810
.set FSOpenFile, 0x0106F9C4
.set FSCloseFile, 0x0106FAD0
.set FSReadFile, 0x0106FB50
.set FSWriteFile, 0x0106FC70
.set FSSetPosFile, 0x0106FF78
.set FSGetStatFile, 0x0106FFE8

_start:
	b _main
	b GetStat
    b OpenFile
    b ReadFile
    b CloseFile
    b SetPosFile
    b GetStatFile

GetStat:
    mr r29, r3  #The patched instruction
    patch FSGetStat, 8, 0x28, 0x5C

OpenFile:
    mr r30, r4
    patch FSOpenFile, 8, 0x2C, 0x64

ReadFile:
    mr r30, r4
    patch FSReadFile, 0xC, 0x34, 0x78

CloseFile:
    mr r29, r3
    patch FSCloseFile, 8, 0x30, 0x60

SetPosFile:
    mr r29, r3
    patch FSSetPosFile, 8, 0x28, 0x5C

GetStatFile:
    mr r29, r3
    patch FSGetStatFile, 8, 0x28, 0x5C
