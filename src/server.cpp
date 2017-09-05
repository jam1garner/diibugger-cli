
#include "cafe.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

extern int (* const entryPoint)(int argc, char *argv[]);

#define main (*entryPoint)

#define Declare(function) \
	function ##_t function;

#define Acquire(module) \
	int module; \
	OSDynLoad_Acquire(#module ".rpl", &module);

#define Import(module, function) \
	OSDynLoad_FindExport(module, 0, #function, &a.function);

#define Import2(module, function, name) \
	OSDynLoad_FindExport(module, 0, #function, &a.name);

#define ImportPtr(module, function) \
    function ##_t *function ##_ptr; \
    OSDynLoad_FindExport(module, 1, #function, &function ##_ptr); \
    a.function = *function ##_ptr;

#define HANDLE_CRASH(type, handler) \
    globals *a = GLOBALS; \
    memcpy((char *)&a->crashContext, (const char *)context, sizeof(OSContext)); \
    a->crashType = type; \
    context->srr0 = (u32)handler; \
    return true;

#define CHECK_ERROR(result, funcname) \
    if (result < 0) { \
        char buffer[50] = {0}; \
        GLOBALS->__os_snprintf(buffer, 50, "%s failed: %i", funcname, result); \
        GLOBALS->OSFatal(buffer); \
    }

#define CHECK_SOCKET(result, funcname) \
    if (result == -1) { \
        char buffer[50] = {0}; \
        GLOBALS->__os_snprintf(buffer, 50, "%s failed: %i", funcname, GLOBALS->SOLastError()); \
        GLOBALS->OSFatal(buffer); \
    }

//Address 0x10000000 contains the word 1000 by default (coreinit.rpl)
#define INIT_FS_PATCH \
    globals *a = GLOBALS; \
    if (a == (globals *)1000) return -1; \
    if (client == &a->fileClient) return -1;

#define STACK_SIZE 0x8000
#define MESSAGE_COUNT 4
#define GLOBALS (*(globals **)0x10000000)
#define TRAP 0x7FE00008
#define STEP1 10
#define STEP2 11

#define SERVER_MESSAGE_DSI 0
#define SERVER_MESSAGE_ISI 1
#define SERVER_MESSAGE_PROGRAM 2
#define SERVER_MESSAGE_GET_STAT 3
#define SERVER_MESSAGE_OPEN_FILE 4
#define SERVER_MESSAGE_READ_FILE 5
#define SERVER_MESSAGE_CLOSE_FILE 6
#define SERVER_MESSAGE_SET_POS_FILE 7
#define SERVER_MESSAGE_GET_STAT_FILE 8

#define CLIENT_MESSAGE_CONTINUE 0
#define CLIENT_MESSAGE_STEP 1
#define CLIENT_MESSAGE_STEP_OVER 2
#define CLIENT_MESSAGE_STEP_MSC 3

#define STEP_STATE_RUNNING 0
#define STEP_STATE_CONTINUE 1
#define STEP_STATE_STEPPING 2

#define CRASH_STATE_NONE 0
#define CRASH_STATE_UNRECOVERABLE 1
#define CRASH_STATE_BREAKPOINT 2

struct breakpoint {
    u32 address;
    u32 instruction;
};

struct globals {
    OSThread thread;
    OSMessageQueue serverQueue;
    OSMessageQueue clientQueue;
    OSMessageQueue fileQueue;
    OSMessage serverMessages[MESSAGE_COUNT];
    OSMessage clientMessages[MESSAGE_COUNT];
    OSMessage fileMessage;
    OSContext crashContext;
    u32 crashType;

    u8 crashState;
    u8 stepState;
    u32 stepSource;

    //10 general breakpoints + 2 step breakpoints
    breakpoint breakpoints[12];
    u32 mscBreakPoints[10];
    bool mscStep;

    bool connection;

    FSCmdBlock fileBlock;
    FSClient fileClient;
    char *patchFiles;
    int fileHandle;

    char stack[STACK_SIZE];

    Declare(OSDynLoad_GetModuleName)

    Declare(OSCreateThread)
    Declare(OSResumeThread)
    Declare(OSGetActiveThreadLink)
    Declare(OSGetCurrentThread)
    Declare(OSGetThreadName)
    Declare(OSGetThreadPriority)
    Declare(OSGetThreadAffinity)
    Declare(OSSetThreadName)
    Declare(OSLoadContext)

    Declare(OSScreenInit)
    Declare(OSScreenGetBufferSizeEx)
    Declare(OSScreenSetBufferEx)
    Declare(OSScreenClearBufferEx)
    Declare(OSScreenEnableEx)
    Declare(OSScreenPutFontEx)
    Declare(OSScreenFlipBuffersEx)

    Declare(OSInitMessageQueue)
    Declare(OSSendMessage)
    Declare(OSReceiveMessage)
    Declare(OSSleepTicks)

    Declare(OSSetExceptionCallback)
    Declare(OSSetExceptionCallbackEx)
    Declare(__os_snprintf)
    Declare(OSFatal)

    Declare(DCFlushRange)
    Declare(ICInvalidateRange)

    Declare(MEMAllocFromDefaultHeap)
    Declare(MEMAllocFromDefaultHeapEx)
    Declare(MEMFreeToDefaultHeap)

    Declare(FSInit)
    Declare(FSInitCmdBlock)
    Declare(FSAddClient)
    Declare(FSOpenFile)
    Declare(FSReadFile)
    Declare(FSCloseFile)
    Declare(FSGetStatFile)
    Declare(FSOpenDir)
    Declare(FSReadDir)
    Declare(FSCloseDir)

    Declare(SOInit)
    Declare(SOSocket)
    Declare(SOSetSockOpt)
    Declare(SOBind)
    Declare(SOListen)
    Declare(SOAccept)
    Declare(SORecv)
    Declare(SOSend)
    Declare(SOClose)
    Declare(SOFinish)
    Declare(SOLastError)

    Declare(VPADRead)
};

void sendall(int fd, void *data, int length) {
	int sent = 0;
	while (sent < length) {
		int num = GLOBALS->SOSend(fd, data, length - sent, 0);
		CHECK_SOCKET(num, "SOSend")
		sent += num;
		data = (char *)data + num;
	}
}

void recvall(int fd, void *buffer, int length) {
    int bytes = 0;
    while (bytes < length) {
        int num = GLOBALS->SORecv(fd, buffer, length - bytes, 0);
        CHECK_SOCKET(num, "SORecv")
        bytes += num;
        buffer = (char *)buffer + num;
    }
}

u8 recvbyte(int fd) {
	u8 byte;
	recvall(fd, &byte, 1);
	return byte;
}

u32 recvword(int fd) {
	u32 word;
	recvall(fd, &word, 4);
	return word;
}

u32 strlen(const char *s) {
    u32 len = 0;
    while (*s) {
        len++;
        s++;
    }
    return len;
}

void memcpy(char *dst, const char *src, u32 length) {
    for (u32 i = 0; i < length; i++) {
        *dst++ = *src++;
    }
}

extern "C" void memset(char *dst, char val, u32 length) {
    for (u32 i = 0; i < length; i++) {
        *dst++ = val;
    }
}

void WriteCode(u32 address, u32 instr) {
    u32 *ptr = (u32 *)(address + 0xA0000000);
    *ptr = instr;
    GLOBALS->DCFlushRange(ptr, 4);
    GLOBALS->ICInvalidateRange(ptr, 4);
}

bool compare(const char *one, const char *two, int length) {
    for (int i = 0; i < length; i++) {
        if (one[i] != two[i]) return false;
    }
    return true;
}

bool IsServerFile(const char *path) {
    const char *fileList = GLOBALS->patchFiles;
    if (fileList) {
        u32 fileCount = *(u32 *)fileList; fileList += 4;
        for (u32 i = 0; i < fileCount; i++) {
            u16 pathLength = *(u16 *)fileList; fileList += 2;
            if (strlen(path) == pathLength) {
                if (compare(fileList, path, pathLength)) {
                    return true;
                }
            }
            fileList += pathLength;
        }
    }
    return false;
}

extern "C" {

    int Patch_FSGetStat(FSClient *client, FSCmdBlock *block, const char *path, FSStat_ *stat, int errHandling) {
        INIT_FS_PATCH
        if (!IsServerFile(path)) return -1;

        OSMessage message;
        message.message = SERVER_MESSAGE_GET_STAT;
        message.data0 = (u32)path;
        message.data1 = strlen(path);
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        memset((char *)stat, 0, sizeof(FSStat_));
        stat->size = message.data0;
        return 0;
    }

    int Patch_FSOpenFile(FSClient *client, FSCmdBlock *block, const char *path, const char *mode, int *handle, int errHandling) {
        INIT_FS_PATCH
        if (!IsServerFile(path)) return -1;
        if (a->fileHandle != 0) {
            a->OSFatal("The game tried to open two patched files at the same time.\n"
                       "However, the current debugger implementation only allows\n"
                       "one patched file to be opened at once.");
        }

        OSMessage message;
        message.message = SERVER_MESSAGE_OPEN_FILE;
        message.data0 = (u32)path;
        message.data1 = strlen(path);
        message.data2 = *(u32 *)mode; //A bit of a hack
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        *handle = message.data0;
        a->fileHandle = message.data0;
        return 0;
    }

    int Patch_FSReadFile(FSClient *client, FSCmdBlock *block, void *buffer, u32 size, u32 count, int handle, u32 flags, int errHandling) {
        INIT_FS_PATCH
        if (handle != a->fileHandle) return -1;

        u32 readArgs[] = {(u32)buffer, size, count, handle};
        OSMessage message;
        message.message = SERVER_MESSAGE_READ_FILE;
        message.data0 = (u32)readArgs;
        message.data1 = 0x10;
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        return message.data0;
    }

    int Patch_FSCloseFile(FSClient *client, FSCmdBlock *block, int handle, int errHandling) {
        INIT_FS_PATCH
        if (handle != a->fileHandle) return -1;

        OSMessage message;
        message.message = SERVER_MESSAGE_CLOSE_FILE;
        message.data0 = 0;
        message.data1 = 0;
        message.data2 = handle;
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        a->fileHandle = 0;
        return 0;
    }

    int Patch_FSSetPosFile(FSClient *client, FSCmdBlock *block, int handle, u32 pos, int errHandling) {
        INIT_FS_PATCH
        if (handle != a->fileHandle) return -1;

        u32 args[] = {handle, pos};
        OSMessage message;
        message.message = SERVER_MESSAGE_SET_POS_FILE;
        message.data0 = 8;
        message.data1 = (u32)args;
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        return 0;
    }

    int Patch_FSGetStatFile(FSClient *client, FSCmdBlock *block, int handle, FSStat_ *stat, int errHandling) {
        INIT_FS_PATCH
        if (handle != a->fileHandle) return -1;

        OSMessage message;
        message.message = SERVER_MESSAGE_GET_STAT_FILE;
        message.data0 = 0;
        message.data1 = 0;
        message.data2 = handle;
        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
        a->OSReceiveMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
        stat->size = message.data0;
        return 0;
    }

}

void FatalCrashHandler() {
    globals *a = GLOBALS;

    char buffer[0x400];
    a->__os_snprintf(buffer, 0x400,
        "An exception of type %i occurred:\n\n"
        "r0: %08X r1: %08X r2: %08X r3: %08X r4: %08X\n"
        "r5: %08X r6: %08X r7: %08X r8: %08X r9: %08X\n"
        "r10:%08X r11:%08X r12:%08X r13:%08X r14:%08X\n"
        "r15:%08X r16:%08X r17:%08X r18:%08X r19:%08X\n"
        "r20:%08X r21:%08X r22:%08X r23:%08X r24:%08X\n"
        "r25:%08X r26:%08X r27:%08X r28:%08X r29:%08X\n"
        "r30:%08X r31:%08X\n\n"
        "CR: %08X LR: %08X CTR:%08X XER:%08X\n"
        "EX0:%08X EX1:%08X SRR0:%08X SRR1:%08X\n",
        a->crashType,
        a->crashContext.gpr[0],
        a->crashContext.gpr[1],
        a->crashContext.gpr[2],
        a->crashContext.gpr[3],
        a->crashContext.gpr[4],
        a->crashContext.gpr[5],
        a->crashContext.gpr[6],
        a->crashContext.gpr[7],
        a->crashContext.gpr[8],
        a->crashContext.gpr[9],
        a->crashContext.gpr[10],
        a->crashContext.gpr[11],
        a->crashContext.gpr[12],
        a->crashContext.gpr[13],
        a->crashContext.gpr[14],
        a->crashContext.gpr[15],
        a->crashContext.gpr[16],
        a->crashContext.gpr[17],
        a->crashContext.gpr[18],
        a->crashContext.gpr[19],
        a->crashContext.gpr[20],
        a->crashContext.gpr[21],
        a->crashContext.gpr[22],
        a->crashContext.gpr[23],
        a->crashContext.gpr[24],
        a->crashContext.gpr[25],
        a->crashContext.gpr[26],
        a->crashContext.gpr[27],
        a->crashContext.gpr[28],
        a->crashContext.gpr[29],
        a->crashContext.gpr[30],
        a->crashContext.gpr[31],
        a->crashContext.cr,
        a->crashContext.lr,
        a->crashContext.ctr,
        a->crashContext.xer,
        a->crashContext.ex0,
        a->crashContext.ex1,
        a->crashContext.srr0,
        a->crashContext.srr1
    );

    a->OSFatal(buffer);
}

bool DSIHandler_Fatal(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_DSI, FatalCrashHandler) }
bool ISIHandler_Fatal(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_ISI, FatalCrashHandler) }
bool ProgramHandler_Fatal(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_PROGRAM, FatalCrashHandler) }

void ReportCrash(u32 msg) {
    globals *a = GLOBALS;
    a->crashState = CRASH_STATE_UNRECOVERABLE;

    OSMessage message;
    message.message = msg;
    message.data0 = (u32)&a->crashContext;
    message.data1 = sizeof(a->crashContext);
    a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);
    while (true) {
        a->OSSleepTicks(1000000);
    }
}

void HandleDSI() {
    ReportCrash(SERVER_MESSAGE_DSI);
}

void HandleISI() {
    ReportCrash(SERVER_MESSAGE_ISI);
}

breakpoint *GetBreakPoint(u32 addr, int end) {
    breakpoint *bplist = GLOBALS->breakpoints;
    for (int i = 0; i < end; i++) {
        if (bplist[i].address == addr) {
            return &bplist[i];
        }
    }
    return 0;
}

int GetMscBreakPointIndex(u32 addr, int end){
    u32 *mscBpList = GLOBALS->mscBreakPoints;
    for (int i = 0; i < end; i++)
        if(mscBpList[i] == addr)
            return i;
    return -1;
}

void RestoreStepInstructions() {
    globals *a = GLOBALS;

    //Write back the instructions that were replaced for the step
    WriteCode(a->breakpoints[STEP1].address, a->breakpoints[STEP1].instruction);
    a->breakpoints[STEP1].address = 0;
    a->breakpoints[STEP1].instruction = 0;
    if (a->breakpoints[STEP2].address) {
        WriteCode(a->breakpoints[STEP2].address, a->breakpoints[STEP2].instruction);
        a->breakpoints[STEP2].address = 0;
        a->breakpoints[STEP2].instruction = 0;
    }

    breakpoint *bp = GetBreakPoint(a->stepSource, 10);
    if (bp) {
        WriteCode(bp->address, TRAP);
    }
}

u32 GetInstruction(u32 address) {
    breakpoint *bp = GetBreakPoint(address, 12);
    if (bp) {
        return bp->instruction;
    }
    return *(u32 *)address;
}

void PredictStepAddresses(bool stepOver) {
    globals *a = GLOBALS;

    u32 currentAddr = a->crashContext.srr0;
    u32 instruction = GetInstruction(currentAddr);

    breakpoint *step1 = &a->breakpoints[STEP1];
    breakpoint *step2 = &a->breakpoints[STEP2];
    step1->address = currentAddr + 4;
    step2->address = 0;

    u8 opcode = instruction >> 26;
    if (opcode == 19) {
        u16 XO = (instruction >> 1) & 0x3FF;
        bool LK = instruction & 1;
        if (!LK || !stepOver) {
            if (XO ==  16) step2->address = a->crashContext.lr; //bclr
            if (XO == 528) step2->address = a->crashContext.ctr; //bcctr
        }
    }

    else if (opcode == 18) { //b
        bool AA = instruction & 2;
        bool LK = instruction & 1;
        u32 LI = instruction & 0x3FFFFFC;
        if (!LK || !stepOver) {
            if (AA) step1->address = LI;
            else {
                if (LI & 0x2000000) LI -= 0x4000000;
                step1->address = currentAddr + LI;
            }
        }
    }

    else if (opcode == 16) { //bc
        bool AA = instruction & 2;
        bool LK = instruction & 1;
        u32 BD = instruction & 0xFFFC;
        if (!LK || !stepOver) {
            if (AA) step2->address = BD;
            else {
                if (BD & 0x8000) BD -= 0x10000;
                step2->address = currentAddr + BD;
            }
        }
    }
}

void HandleProgram() {
    globals *a = GLOBALS;

    //Check if the exception was caused by a breakpoint
    if (!(a->crashContext.srr1 & 0x20000)) {
        ReportCrash(SERVER_MESSAGE_PROGRAM);
    }


    //A breakpoint is done by replacing an instruction by a "trap" instruction
    //When execution is continued this instruction still has to be executed
    //So we have to put back the original instruction, execute it, and insert
    //the breakpoint again

    //We can't simply use the BE and SE bits in the MSR without kernel patches
    //However, since they're optional, they might not be implemented on the Wii U
    //Patching the kernel is not really worth the effort in this case, so I'm
    //simply placing a trap at the next instruction

    //Special case, the twu instruction at the start
    if (a->crashContext.srr0 == (u32)entryPoint + 0x48) {
        WriteCode(a->crashContext.srr0, 0x60000000); //nop
    }

    if (a->stepState == STEP_STATE_RUNNING || a->stepState == STEP_STATE_STEPPING) {
        a->crashState = CRASH_STATE_BREAKPOINT;
		OSMessage message;

		if(a->crashContext.srr0 == 0xD37B564){
            if(a->mscStep){
                a->mscStep = false;
		        message.message = SERVER_MESSAGE_PROGRAM;
		        message.data0 = (u32)&a->crashContext;
		        message.data1 = sizeof(a->crashContext);
		        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);

		        a->OSReceiveMessage(&a->clientQueue, &message, OS_MESSAGE_BLOCK);
            }
            else{
                u32 register0 = a->crashContext.gpr[0];
                for(int i = 0; i < 10; i++){
                    if(a->mscBreakPoints[i] == 0){
    					message.message = CLIENT_MESSAGE_CONTINUE;
                        break;
    				}
                    if(a->mscBreakPoints[i] == register0){
                        message.message = SERVER_MESSAGE_PROGRAM;
        		        message.data0 = (u32)&a->crashContext;
        		        message.data1 = sizeof(a->crashContext);
        		        a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);

        		        a->OSReceiveMessage(&a->clientQueue, &message, OS_MESSAGE_BLOCK);
    					break;
    				}
    			}
            }
        }
        else{
            message.message = SERVER_MESSAGE_PROGRAM;
            message.data0 = (u32)&a->crashContext;
            message.data1 = sizeof(a->crashContext);
            a->OSSendMessage(&a->serverQueue, &message, OS_MESSAGE_BLOCK);

            a->OSReceiveMessage(&a->clientQueue, &message, OS_MESSAGE_BLOCK);
        }

		if((u32)message.message == CLIENT_MESSAGE_STEP_MSC){
			a->mscStep = true;
			message.message = CLIENT_MESSAGE_CONTINUE;
		}

		if (a->stepState == STEP_STATE_STEPPING) {
			RestoreStepInstructions();
		}

		breakpoint *bp = GetBreakPoint(a->crashContext.srr0, 10);
		if (bp) {
			WriteCode(bp->address, bp->instruction);
		}

        //A conditional branch can end up at two places, depending on
        //wheter it's taken or not. To work around this, I'm using a
        //second, optional address. This is less work than writing code
        //that checks the condition registers.
        if ((u32)message.message == CLIENT_MESSAGE_STEP_OVER) {
            PredictStepAddresses(true);
        }
        else PredictStepAddresses(false);

        a->breakpoints[STEP1].instruction = *(u32 *)(a->breakpoints[STEP1].address);
        WriteCode(a->breakpoints[STEP1].address, TRAP);
        if (a->breakpoints[STEP2].address) {
            a->breakpoints[STEP2].instruction = *(u32 *)(a->breakpoints[STEP2].address);
            WriteCode(a->breakpoints[STEP2].address, TRAP);
        }

        a->stepSource = a->crashContext.srr0;

        if ((u32)message.message == CLIENT_MESSAGE_CONTINUE) a->stepState = STEP_STATE_CONTINUE;
        else a->stepState = STEP_STATE_STEPPING;
    }
    else if (a->stepState == STEP_STATE_CONTINUE) {
        RestoreStepInstructions();
        a->stepState = STEP_STATE_RUNNING;
        a->crashState = CRASH_STATE_NONE;
    }
    a->OSLoadContext(&a->crashContext); //Resume execution
}

bool DSIHandler_Debug(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_DSI, HandleDSI) }
bool ISIHandler_Debug(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_ISI, HandleISI) }
bool ProgramHandler_Debug(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_PROGRAM, HandleProgram) }

bool isValidStackPtr(u32 sp) {
    return sp >= 0x10000000 && sp < 0x20000000;
}

u32 PushThread(char *buffer, u32 offset, OSThread *thread) {
    globals *a = GLOBALS;
    *(u32 *)(buffer + offset) = a->OSGetThreadAffinity(thread);
    *(u32 *)(buffer + offset + 4) = a->OSGetThreadPriority(thread);
    *(u32 *)(buffer + offset + 8) = (u32)thread->stackBase;
    *(u32 *)(buffer + offset + 12) = (u32)thread->stackEnd;
    *(u32 *)(buffer + offset + 16) = (u32)thread->entryPoint;

    const char *threadName = a->OSGetThreadName(thread);
    if (threadName) {
        u32 namelen = strlen(threadName);
        *(u32 *)(buffer + offset + 20) = namelen;
        memcpy(buffer + offset + 24, threadName, namelen);
        return 24 + namelen;
    }

    *(u32 *)(buffer + offset + 20) = 0;
    return 24;
}

breakpoint *GetFreeBreakPoint() {
    breakpoint *bplist = GLOBALS->breakpoints;
    for (int i = 0; i < 10; i++) {
        if (bplist[i].address == 0) {
            return &bplist[i];
        }
    }
    return 0;
}

int GetFreeMSCBreakPoint(){
    u32 *mscBpList = GLOBALS->mscBreakPoints;
    for (int i = 0; i < 10; i++)
        if(mscBpList[i] == 0)
            return i;
    return -1;
}

breakpoint *GetBreakPointRange(u32 addr, u32 num, breakpoint *prev) {
    breakpoint *bplist = GLOBALS->breakpoints;

    int start = 0;
    if (prev) {
        start = (prev - bplist) + 1;
    }

    for (int i = start; i < 12; i++) {
        if (bplist[i].address >= addr && bplist[i].address < addr + num) {
            return &bplist[i];
        }
    }
    return 0;
}

int RPCServer(int intArg, void *ptrArg) {
    globals *a = (globals *)ptrArg;

    a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_THREAD, OS_EXCEPTION_DSI, DSIHandler_Fatal);
    a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_THREAD, OS_EXCEPTION_ISI, ISIHandler_Fatal);
    a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_THREAD, OS_EXCEPTION_PROGRAM, ProgramHandler_Fatal);

	int result = a->SOInit();
	CHECK_SOCKET(result, "SOInit")

	while (true) {

        int socket = a->SOSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        CHECK_SOCKET(socket, "SOSocket")

        u32 reuseaddr = 1;
        result = a->SOSetSockOpt(socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, 4);
        CHECK_SOCKET(result, "SOSetSockOpt")

        sockaddr_ serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = 1559;
        serverAddr.sin_addr = 0;
        result = a->SOBind(socket, &serverAddr, 16);
        CHECK_SOCKET(result, "SOBind")

        result = a->SOListen(socket, 1);
        CHECK_SOCKET(result, "SOListen")

        int length = 16;
        int client = a->SOAccept(socket, &serverAddr, &length);
        CHECK_SOCKET(client, "SOAccept")

        a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_DSI, DSIHandler_Debug);
	    a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_ISI, ISIHandler_Debug);
	    a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_PROGRAM, ProgramHandler_Debug);
	    a->connection = true;

        while (true) {
            u8 cmd = recvbyte(client);
            if (cmd == 1) { //Close
                //Remove all breakpoints
                for (int i = 0; i < 10; i++) {
                    if (a->breakpoints[i].address) {
                        WriteCode(a->breakpoints[i].address, a->breakpoints[i].instruction);
                        a->breakpoints[i].address = 0;
                        a->breakpoints[i].instruction = 0;
                    }
                }

                //Remove file patches
                if (a->patchFiles) {
                    a->MEMFreeToDefaultHeap(a->patchFiles);
                    a->patchFiles = 0;
                }

                //Make sure we're not stuck in an exception when the
                //debugger disconnects without handling it
                if (a->crashState == CRASH_STATE_BREAKPOINT) {
                    OSMessage message;
                    message.message = CLIENT_MESSAGE_CONTINUE;
                    a->OSSendMessage(&a->clientQueue, &message, OS_MESSAGE_BLOCK);
                    //Wait until execution is resumed before installing the OSFatal crash handler
                    while (a->crashState != CRASH_STATE_NONE) {
                        a->OSSleepTicks(100000);
                    }
                }

                a->connection = false;
            	a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_DSI, DSIHandler_Fatal);
	            a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_ISI, ISIHandler_Fatal);
	            a->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_PROGRAM, ProgramHandler_Fatal);
                break;
            }

            else if (cmd == 2) { //Read
                u32 addr = recvword(client);
                u32 num = recvword(client);

                //Restore instructions, we don't want to see "trap" in the disassembly
                breakpoint *bp = GetBreakPointRange(addr, num, 0);
                while (bp) {
                    WriteCode(bp->address, bp->instruction);
                    bp = GetBreakPointRange(addr, num, bp);
                }

                sendall(client, (void *)addr, num);

                //Now put back the breakpoints
                bp = GetBreakPointRange(addr, num, 0);
                while (bp) {
                    WriteCode(bp->address, TRAP);
                    bp = GetBreakPointRange(addr, num, bp);
                }
            }

            else if (cmd == 3) { //Write
                u32 addr = recvword(client);
                u32 num = recvword(client);
                recvall(client, (void *)addr, num);
            }

            else if (cmd == 4) { //Write code
                u32 addr = recvword(client);
                u32 instr = recvword(client);

                //Make sure we don't overwrite breakpoint traps
                breakpoint *bp = GetBreakPoint(addr, 12);
                if (bp) bp->instruction = instr;
                else WriteCode(addr, instr);
            }

            else if (cmd == 5) { //Get thread list
                //Might need OSDisableInterrupts here?
                char buffer[0x1000]; //This should be enough
                u32 offset = 0;

                OSThread *currentThread = a->OSGetCurrentThread();
                OSThread *iterThread = currentThread;
                OSThreadLink threadLink;
                do { //Loop previous threads
                    offset += PushThread(buffer, offset, iterThread);
                    a->OSGetActiveThreadLink(iterThread, &threadLink);
                    iterThread = threadLink.prev;
                } while (iterThread);

                a->OSGetActiveThreadLink(currentThread, &threadLink);
                iterThread = threadLink.next;
                while (iterThread) { //Loop next threads
                    offset += PushThread(buffer, offset, iterThread);
                    a->OSGetActiveThreadLink(iterThread, &threadLink);
                    iterThread = threadLink.next;
                }

                sendall(client, &offset, 4);
                sendall(client, buffer, offset);
            }

            else if (cmd == 6) { //Push message
                OSMessage message;
                recvall(client, &message, sizeof(OSMessage));
                a->OSSendMessage(&a->clientQueue, &message, OS_MESSAGE_BLOCK);
            }

            else if (cmd == 7) { //Get messages
                OSMessage messages[10];
                u32 count = 0;

                OSMessage message;
                while (a->OSReceiveMessage(&a->serverQueue, &message, OS_MESSAGE_NOBLOCK)) {
                    messages[count] = message;
                    count++;
                }

                sendall(client, &count, 4);
                for (int i = 0; i < count; i++) {
                    sendall(client, &messages[i], sizeof(OSMessage));
                    if (messages[i].data0) {
                        sendall(client, (void *)messages[i].data0, messages[i].data1);
                    }
                }
            }

            else if (cmd == 8) { //Get stack trace
                u32 sp = a->crashContext.gpr[1];
                u32 index = 0;
                u32 stackTrace[30];
                while (isValidStackPtr(sp)) {
                    sp = *(u32 *)sp;
                    if (!isValidStackPtr(sp)) break;

                    stackTrace[index] = *(u32 *)(sp + 4);
                    index++;
                }

                sendall(client, &index, 4);
                sendall(client, stackTrace, index * 4);
            }

            else if (cmd == 9) { //Poke registers
                recvall(client, &a->crashContext.gpr, 4 * 32);
                recvall(client, &a->crashContext.fpr, 8 * 32);
            }

            else if (cmd == 10) { //Toggle breakpoint
                u32 address = recvword(client);
                breakpoint *bp = GetBreakPoint(address, 10);
                if (bp) {
                    WriteCode(address, bp->instruction);
                    bp->address = 0;
                    bp->instruction = 0;
                }
                else {
                    bp = GetFreeBreakPoint();
                    bp->address = address;
                    bp->instruction = *(u32 *)address;
                    WriteCode(address, TRAP);
                }
            }

            else if (cmd == 11) { //Read directory
                char path[640] = {0}; //512 + 128
                u32 pathlen = recvword(client);
                if (pathlen < 640) {
                    recvall(client, &path, pathlen);

                    int error;
                    int handle;
                    FSDirEntry_ entry;

                    error = a->FSOpenDir(&a->fileClient, &a->fileBlock, path, &handle, -1);
                    CHECK_ERROR(error, "FSOpenDir")

                    while (a->FSReadDir(&a->fileClient, &a->fileBlock, handle, &entry, -1) == 0) {
                        int namelen = strlen(entry.name);
                        sendall(client, &namelen, 4);
                        sendall(client, &entry.stat.flags, 4);
                        if (!(entry.stat.flags & 0x80000000)) {
                            sendall(client, &entry.stat.size, 4);
                        }
                        sendall(client, &entry.name, namelen);
                    }

                    error = a->FSCloseDir(&a->fileClient, &a->fileBlock, handle, -1);
                    CHECK_ERROR(error, "FSCloseDir")
                }
                int terminator = 0;
                sendall(client, &terminator, 4);
            }

            else if (cmd == 12) { //Dump file
                char path[640] = {0};
                u32 pathlen = recvword(client);
                if (pathlen < 640) {
                    recvall(client, &path, pathlen);

                    int error, handle;
                    error = a->FSOpenFile(&a->fileClient, &a->fileBlock, path, "r", &handle, -1);
                    CHECK_ERROR(error, "FSOpenFile")

                    FSStat_ stat;
                    error = a->FSGetStatFile(&a->fileClient, &a->fileBlock, handle, &stat, -1);
                    CHECK_ERROR(error, "FSGetStatFile")

                    u32 size = stat.size;
                    sendall(client, &stat.size, 4);

                    char *buffer = (char *)a->MEMAllocFromDefaultHeapEx(0x20000, 0x40);

                    u32 read = 0;
                    while (read < size) {
                        int num = a->FSReadFile(&a->fileClient, &a->fileBlock, buffer, 1, 0x20000, handle, 0, -1);
                        CHECK_ERROR(num, "FSReadFile")
                        read += num;

                        sendall(client, buffer, num);
                    }

                    error = a->FSCloseFile(&a->fileClient, &a->fileBlock, handle, -1);
                    CHECK_ERROR(error, "FSCloseFile")

                    a->MEMFreeToDefaultHeap(buffer);
                }
                else {
                    a->OSFatal("pathlen >= 640");
                }
            }

            else if (cmd == 13) { //Get module name
                char name[100] = {0};
                int length = 100;
                a->OSDynLoad_GetModuleName(-1, name, &length);

                length = strlen(name);
                sendall(client, &length, 4);
                sendall(client, name, length);
            }

            else if (cmd == 14) { //Set patch files
                if (a->patchFiles) {
                    a->MEMFreeToDefaultHeap(a->patchFiles);
                }

                u32 length = recvword(client);
                a->patchFiles = (char *)a->MEMAllocFromDefaultHeapEx(length, 4);
                recvall(client, a->patchFiles, length);
            }

            else if (cmd == 15) { //Send file message
                OSMessage message;
                recvall(client, &message, sizeof(OSMessage));
                a->OSSendMessage(&a->fileQueue, &message, OS_MESSAGE_BLOCK);
            }

            else if (cmd == 16) { //Clear patch files
                if (a->patchFiles) {
                    a->MEMFreeToDefaultHeap(a->patchFiles);
                    a->patchFiles = 0;
                }
            }

            else if(cmd == 0xFE){
                a->mscStep = true;
            }

            else if(cmd == 0xFF){ //Toggle MSC Breakpoint
                int address = recvword(client);
                int bp = GetMscBreakPointIndex(address, 10);
                if (bp != -1) {
                    a->mscBreakPoints[bp] = 0;
                } else {
                    bp = GetFreeMSCBreakPoint();
                    if(bp != -1)
                        a->mscBreakPoints[bp] = address;
                }
            }
        }

        CHECK_SOCKET(a->SOClose(client), "SOClose")
        CHECK_SOCKET(a->SOClose(socket), "SOClose")
    }

    //a->SOFinish();
    //return 0;
}

void WriteScreen(const char *msg) {
    globals *a = GLOBALS;
    for (int buffer = 0; buffer < 2; buffer++) {
        a->OSScreenClearBufferEx(buffer, 0);
        a->OSScreenPutFontEx(buffer, 0, 0, msg);
        a->OSScreenFlipBuffersEx(buffer);
    }
}

bool WaitForConnection() {
    globals *a = GLOBALS;

    a->OSScreenInit();
    a->OSScreenSetBufferEx(0, (void *)0xF4000000);
    a->OSScreenSetBufferEx(1, (void *)(0xF4000000 + a->OSScreenGetBufferSizeEx(0)));
    a->OSScreenEnableEx(0, 1);
    a->OSScreenEnableEx(1, 1);
    WriteScreen("Waiting for debugger connection.\n"
                "Press the home button to continue without debugger.\n"
                "You can still connect while the game is running.");

    int error;
    VPADStatus status;
    a->VPADRead(0, &status, 1, &error);
    while (!(status.hold & VPAD_BUTTON_HOME)) {
        if (a->connection) {
            //Another WriteScreen to update the displayed message here
            //somehow causes problems when the game tries to use GX2
            return true;
        }
        a->VPADRead(0, &status, 1, &error);
    }
    return false;
}

void InitializeDebugger() {
    //if (!WaitForConnection()) {
        //If the game was started without debugger, use the OSFatal exception handler
        GLOBALS->OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_PROGRAM, ProgramHandler_Fatal);
        //Advance the program counter to the next instruction, because it's still on the breakpoint instruction.
        GLOBALS->crashContext.srr0 += 4;
    //}
    GLOBALS->OSLoadContext(&GLOBALS->crashContext);
}

bool ProgramHandler_Initialize(OSContext *context) { HANDLE_CRASH(OS_EXCEPTION_PROGRAM, InitializeDebugger) }

extern "C" int _main(int argc, char *argv[]) {
    globals a __attribute__((aligned(8)));
    memset((char *)&a, 0, sizeof(globals));

	Acquire(coreinit)
	Acquire(nsysnet)
	Acquire(vpad)

	Import(coreinit, OSDynLoad_GetModuleName)

	Import(coreinit, OSCreateThread)
	Import(coreinit, OSResumeThread)
	Import(coreinit, OSGetActiveThreadLink)
	Import(coreinit, OSGetCurrentThread)
	Import(coreinit, OSGetThreadName)
	Import(coreinit, OSGetThreadPriority)
	Import(coreinit, OSGetThreadAffinity)
	Import(coreinit, OSSetThreadName)
	Import(coreinit, OSLoadContext)

	Import(coreinit, OSScreenInit)
	Import(coreinit, OSScreenGetBufferSizeEx)
	Import(coreinit, OSScreenSetBufferEx)
	Import(coreinit, OSScreenClearBufferEx)
	Import(coreinit, OSScreenEnableEx)
	Import(coreinit, OSScreenPutFontEx)
	Import(coreinit, OSScreenFlipBuffersEx)

	Import(coreinit, OSInitMessageQueue)
	Import(coreinit, OSSendMessage)
	Import(coreinit, OSReceiveMessage)
	Import(coreinit, OSSleepTicks)

	Import(coreinit, OSSetExceptionCallbackEx)
	Import(coreinit, __os_snprintf)
	Import(coreinit, OSFatal)

	Import(coreinit, DCFlushRange)
	Import(coreinit, ICInvalidateRange)

    ImportPtr(coreinit, MEMAllocFromDefaultHeap)
    ImportPtr(coreinit, MEMAllocFromDefaultHeapEx)
    ImportPtr(coreinit, MEMFreeToDefaultHeap)

    Import(coreinit, FSInit)
    Import(coreinit, FSInitCmdBlock)
    Import(coreinit, FSAddClient)
    Import(coreinit, FSOpenFile)
    Import(coreinit, FSReadFile)
    Import(coreinit, FSCloseFile)
    Import(coreinit, FSGetStatFile)
    Import(coreinit, FSOpenDir)
    Import(coreinit, FSReadDir)
    Import(coreinit, FSCloseDir)

	Import2(nsysnet, socket_lib_init, SOInit)
	Import2(nsysnet, socket, SOSocket)
	Import2(nsysnet, setsockopt, SOSetSockOpt)
	Import2(nsysnet, bind, SOBind)
	Import2(nsysnet, listen, SOListen)
	Import2(nsysnet, accept, SOAccept)
	Import2(nsysnet, recv, SORecv)
	Import2(nsysnet, send, SOSend)
	Import2(nsysnet, socketclose, SOClose)
	Import2(nsysnet, socket_lib_finish, SOFinish)
	Import2(nsysnet, socketlasterr, SOLastError)

	Import(vpad, VPADRead)

    a.connection = false;
    a.stepState = STEP_STATE_RUNNING;
    GLOBALS = &a;

    a.FSInit();
    a.FSInitCmdBlock(&a.fileBlock);
    a.FSAddClient(&a.fileClient, -1);

	a.OSInitMessageQueue(&a.serverQueue, a.serverMessages, MESSAGE_COUNT);
	a.OSInitMessageQueue(&a.clientQueue, a.clientMessages, MESSAGE_COUNT);
	a.OSInitMessageQueue(&a.fileQueue, &a.fileMessage, 1);
	a.OSCreateThread(
		&a.thread,
		RPCServer,
		0,
		&a,
		a.stack + STACK_SIZE,
		STACK_SIZE,
		0,
		12
	);
	a.OSSetThreadName(&a.thread, "Debug Server");
	a.OSResumeThread(&a.thread);

	a.OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_DSI, DSIHandler_Fatal);
	a.OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_ISI, ISIHandler_Fatal);
	a.OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES, OS_EXCEPTION_PROGRAM, ProgramHandler_Initialize);

    //Patch the OSIsDebuggerInitialized call
    WriteCode((u32)entryPoint + 0x28, 0x38600001); //li r3, 1

	return main(argc, argv);
}
