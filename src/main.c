#include <stddef.h>
#include <gctypes.h>
#include "common/common.h"
#include "kernel/kernel.h"

#include "cafe.h"
#include "code.h"

#define Acquire(module) \
int module; \
OSDynLoad_Acquire(#module ".rpl", &module);

#define Import(module, function) \
function ##_t function; \
OSDynLoad_FindExport(module, 0, #function, &function);

#define INSTALL_ADDR 0x011DD000
#define MAIN_JMP_ADDR 0x0101C56C

#define FSGetStat 0x01070810
#define FSOpenFile 0x0106F9C4
#define FSCloseFile 0x0106FAD0
#define FSReadFile 0x0106FB50
#define FSSetPosFile 0x0106FF78
#define FSGetStatFile 0x0106FFE8

u32 doBL(u32 dst, u32 src)
{
	u32 instr = (dst - src);
	instr &= 0x03FFFFFC;
	instr |= 0x48000001;
	return instr;
}

u32 doB(u32 dst, u32 src)
{
	u32 instr = (dst - src);
	instr &= 0x03FFFFFC;
	instr |= 0x48000000;
	return instr;
}

void insertBranchL(u32 addr, u32 target, OSEffectiveToPhysical_t OSEffectiveToPhysical)
{
	u32 loc = (u32)OSEffectiveToPhysical((void*)addr);
	RunCodeAsKernel(&KernWritePhys, loc, doBL(target, addr));
}

void insertBranch(u32 addr, u32 target, OSEffectiveToPhysical_t OSEffectiveToPhysical)
{
	u32 loc = (u32)OSEffectiveToPhysical((void*)addr);
	RunCodeAsKernel(&KernWritePhys, loc, doB(target, addr));
}

/* Entry point */
int Menu_Main(void)
{
	Acquire(coreinit)

	Import(coreinit, DCFlushRange)
	Import(coreinit, ICInvalidateRange)
	Import(coreinit, OSEffectiveToPhysical)
	Import(coreinit, _Exit);

	Acquire(sysapp)
	Import(sysapp, SYSCheckTitleExists)
	Import(sysapp, SYSLaunchTitle)
	Import(sysapp, _SYSLaunchTitleByPathFromLauncher)

	u32 baseAddr = (u32)OSEffectiveToPhysical((void*)INSTALL_ADDR);
	//kernel-mode memcpy. You'll love how slow it is!
	for (size_t i = 0; i < code_length; i+=4) {
		unsigned int val = *((unsigned int*)(code_bin + i));
		RunCodeAsKernel(&KernWritePhys, baseAddr + i, val);
	}

	insertBranchL(MAIN_JMP_ADDR, INSTALL_ADDR, OSEffectiveToPhysical);
	/*insertBranch(FSGetStat + 0x24, INSTALL_ADDR + 4, OSEffectiveToPhysical);
	insertBranch(FSOpenFile + 0x28, INSTALL_ADDR + 8, OSEffectiveToPhysical);
	insertBranch(FSReadFile + 0x30, INSTALL_ADDR + 12, OSEffectiveToPhysical);
	insertBranch(FSCloseFile + 0x2C, INSTALL_ADDR + 16, OSEffectiveToPhysical);
	insertBranch(FSSetPosFile + 0x24, INSTALL_ADDR + 20, OSEffectiveToPhysical);
	insertBranch(FSGetStatFile + 0x24, INSTALL_ADDR + 24, OSEffectiveToPhysical);*/

	//Disable OSSetExceptionCallback because DKC: TF will
	//overwrite our own exception handlers otherwise
	//Problem: Splatoon uses OSSetExceptionCallbackEx
	Import(coreinit, OSSetExceptionCallback)
	u32 loc = (u32)OSEffectiveToPhysical((void*)OSSetExceptionCallback);
	RunCodeAsKernel(&KernWritePhys, loc, 0x4E800020);

	RunCodeAsKernel(&KernWritePhys, KERN_ADDRESS_TBL + 0x48, 0x80000000);
	RunCodeAsKernel(&KernWritePhys, KERN_ADDRESS_TBL + 0x4C, 0x28305800);

	//Doesn't actually need to be relaunch_on_load, but hey
	return EXIT_RELAUNCH_ON_LOAD;
}
