#pragma once

#include "common.h"

#define OSDynLoad_Acquire ((int (*)(const char *name, int *handle))0x0102A3B4)
#define OSDynLoad_FindExport ((int (*)(int handle, bool isData, const char *exportName, void *ptr))0x0102B828)

#define OS_MESSAGE_NOBLOCK 0
#define OS_MESSAGE_BLOCK 1

#define OS_EXCEPTION_DSI 2
#define OS_EXCEPTION_ISI 3
#define OS_EXCEPTION_PROGRAM 6
#define OS_EXCEPTION_MODE_THREAD 1
#define OS_EXCEPTION_MODE_GLOBAL_ALL_CORES 4

struct OSThread;

struct OSThreadLink {
    OSThread *next;
    OSThread *prev;
};

struct OSThreadQueue {
	OSThread *head;
	OSThread *tail;
	void *parentStruct;
	u32 reserved;
};

struct OSMessage {
    u32 message;
    u32 data0;
    u32 data1;
    u32 data2;
};

struct OSMessageQueue {
    u32 tag;
    char *name;
    u32 reserved;

    OSThreadQueue sendQueue;
    OSThreadQueue recvQueue;
    OSMessage *messages;
    int msgCount;
    int firstIndex;
    int usedCount;
};

struct OSContext {
	char tag[8];

	u32 gpr[32];

	u32 cr;
	u32 lr;
	u32 ctr;
	u32 xer;

	u32 srr0;
	u32 srr1;

	u32 ex0;
	u32 ex1;

	u32 exception_type;
	u32 reserved;

	double fpscr;
	double fpr[32];

	u16 spinLockCount;
	u16 state;

	u32 gqr[8];
	u32 pir;
	double psf[32];

	u64 coretime[3];
	u64 starttime;

	u32 error;
	u32 attributes;

	u32 pmc1;
	u32 pmc2;
	u32 pmc3;
	u32 pmc4;
	u32 mmcr0;
	u32 mmcr1;
};

typedef int (*ThreadFunc)(int argc, void *argv);

struct OSThread {
    OSContext context;

    u32 txtTag;
    u8 state;
    u8 attr;

    short threadId;
    int suspend;
    int priority;

    char _[0x394 - 0x330];

	void *stackBase;
	void *stackEnd;

	ThreadFunc entryPoint;

	char _3A0[0x6A0 - 0x3A0];
};


typedef bool (*OSExceptionCallback)(OSContext *context);

typedef int (*OSDynLoad_GetModuleName_t)(int handle, char *name, int *size);

typedef void  (*OSFatal_t)(const char *msg);
typedef int   (*__os_snprintf_t)(char *buffer, int size, char *fmt, ... );
typedef void  (*_Exit_t)();

typedef void  (*DCFlushRange_t)(const void *addr, u32 num);
typedef void  (*ICInvalidateRange_t)(const void *addr, u32 num);

typedef void *(*MEMAllocFromDefaultHeap_t)(u32 size);
typedef void *(*MEMAllocFromDefaultHeapEx_t)(u32 size, int alignment);
typedef void  (*MEMFreeToDefaultHeap_t)(void *ptr);
typedef void *(*OSAllocFromSystem_t)(u32 size, int alignment);
typedef void  (*OSFreeToSystem_t)(void *ptr);
typedef void *(*OSEffectiveToPhysical_t)(void *addr);

typedef OSThread *(*OSGetCurrentThread_t)();
typedef bool  (*OSCreateThread_t)(OSThread *, ThreadFunc entryPoint, int argc, void *argv, void *stack, u32 stackSize, int priority, u16 attr);
typedef bool  (*OSGetActiveThreadLink_t)(OSThread *, OSThreadLink *);
typedef u16   (*OSGetThreadAffinity_t)(OSThread *);
typedef const char *(*OSGetThreadName_t)(OSThread *);
typedef int   (*OSGetThreadPriority_t)(OSThread *);
typedef void  (*OSSetThreadName_t)(OSThread *, const char *name);
typedef int   (*OSResumeThread_t)(OSThread *);
typedef void  (*OSSleepTicks_t)(u64 ticks);

typedef void  (*OSInitMessageQueue_t)(OSMessageQueue *, OSMessage *array, int count);
typedef bool  (*OSReceiveMessage_t)(OSMessageQueue *, OSMessage *, int flags);
typedef bool  (*OSSendMessage_t)(OSMessageQueue *, OSMessage *, int flags);

typedef void  (*OSSetExceptionCallback_t)(u8 type, OSExceptionCallback cb);
typedef void  (*OSSetExceptionCallbackEx_t)(u8 mode, u8 type, OSExceptionCallback cb);
typedef void  (*OSLoadContext_t)(OSContext *);

typedef void  (*OSScreenClearBufferEx_t)(u8 screen, u32 color);
typedef void  (*OSScreenEnableEx_t)(u8 screen, bool enabled);
typedef void  (*OSScreenFlipBuffersEx_t)(u8 screen);
typedef u32   (*OSScreenGetBufferSizeEx_t)(u8 screen);
typedef void  (*OSScreenInit_t)();
typedef void  (*OSScreenPutFontEx_t)(u8 screen, u32 x, u32 y, const char *text);
typedef void  (*OSScreenSetBufferEx_t)(u8 screen, void *ptr);

typedef void (*OSSetDABR_t)(bool allCores, void *address, bool matchReads, bool matchWrites);
typedef void *(*OSGetSymbolName_t)(u32 addr, u8 *symbolName, u32 nameBufSize);
typedef void *(*DisasmGetSym)(u32 addr, u8 *symbolName, u32 nameBufSize);
typedef bool (*DisassemblePPCOpcode_t)(u32 *opcode, char *outputBuffer, u32 bufferSize,
                          DisasmGetSym disasmGetSym, u32 disasmOptions);
struct FSClient {
    u8 buffer[5888];
};

struct FSCmdBlock {
    u8 buffer[2688];
};

struct FSStat {
    u32 flags;
    u8 _4[0xC];
    u32 size;
    u8 _14[0x50];
};

struct FSDirEntry {
    FSStat stat;
    char name[256];
};

typedef void(*FSInit_t)();
typedef int (*FSAddClient_t)(FSClient *client, int errHandling);
typedef void(*FSInitCmdBlock_t)(FSCmdBlock *block);
typedef int (*FSGetStat_t)(FSClient *client, FSCmdBlock *block, const char *path, FSStat *stat, int errHandling);
typedef int (*FSOpenFile_t)(FSClient *client, FSCmdBlock *block, const char *path, const char *mode, int *handle, int errHandling);
typedef int (*FSReadFile_t)(FSClient *client, FSCmdBlock *block, void *buffer, u32 size, u32 count, int handle, u32 flags, int errHandling);
typedef int (*FSCloseFile_t)(FSClient *client, FSCmdBlock *block, int handle, int errHandling);
typedef int (*FSSetPosFile_t)(FSClient *client, FSCmdBlock *block, int handle, u32 pos, int errHandling);
typedef int (*FSGetStatFile_t)(FSClient *client, FSCmdBlock *block, int handle, FSStat *stat, int errHandling);
typedef int (*FSOpenDir_t)(FSClient *client, FSCmdBlock *block, const char *path, int *handle, int errHandling);
typedef int (*FSReadDir_t)(FSClient *client, FSCmdBlock *block, int handle, FSDirEntry *entry, int errHandling);
typedef int (*FSCloseDir_t)(FSClient *client, FSCmdBlock *block, int handle, int errHandling);


#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

#define SOL_SOCKET -1
#define SO_REUSEADDR 4

struct sockaddr {
	unsigned short sin_family;
	unsigned short sin_port;
	unsigned long sin_addr;
	char sin_zero[8];
};

typedef int (*SOInit_t)();
typedef int (*SOSocket_t)(int family, int type, int proto);
typedef int (*SOSetSockOpt_t)(int fd, int level, int optname, void *optval, int optlen);
typedef int (*SOConnect_t)(int fd, sockaddr *addr, int addrlen);
typedef int (*SOBind_t)(int fd, sockaddr *addr, int addrlen);
typedef int (*SOListen_t)(int fd, int backlog);
typedef int (*SOAccept_t)(int fd, sockaddr *addr, int *addrlen);
typedef int (*SORecv_t)(int fd, void *buffer, int len, int flags);
typedef int (*SOSend_t)(int fd, const void *buffer, int len, int flags);
typedef int (*SOClose_t)(int fd);
typedef int (*SOFinish_t)();
typedef int (*SOLastError_t)();

typedef int (* SYSCheckTitleExists_t)(u64 titleId);
typedef int (* SYSLaunchTitle_t)(u64 titleId);
typedef int (*_SYSLaunchTitleByPathFromLauncher_t)(const char* path, int len, int zero);

#define VPAD_BUTTON_HOME 0x2

struct VPADStatus {
	u32 hold;
	u32 trig;
	u32 release;

	Vec2 lStick;
	Vec2 rStick;

	char _14[0xA4 - 0x14];
};

typedef int (*VPADRead_t)(int chan, VPADStatus buffer[], u32 length, int* err);
