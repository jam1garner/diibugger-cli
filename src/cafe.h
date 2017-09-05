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
typedef struct OSThread OSThread;

struct OSThreadLink {
    OSThread *next;
    OSThread *prev;
};
typedef struct OSThreadLink OSThreadLink;

struct OSThreadQueue {
	OSThread *head;
	OSThread *tail;
	void *parentStruct;
	un32 reserved;
};
typedef struct OSThreadQueue OSThreadQueue;

struct OSMessage {
    un32 message;
    un32 data0;
    un32 data1;
    un32 data2;
};
typedef struct OSMessage OSMessage;

struct OSMessageQueue {
    un32 tag;
    char *name;
    un32 reserved;

    OSThreadQueue sendQueue;
    OSThreadQueue recvQueue;
    OSMessage *messages;
    int msgCount;
    int firstIndex;
    int usedCount;
};
typedef struct OSMessageQueue OSMessageQueue;

struct OSContext {
	char tag[8];

	un32 gpr[32];

	un32 cr;
	un32 lr;
	un32 ctr;
	un32 xer;

	un32 srr0;
	un32 srr1;

	un32 ex0;
	un32 ex1;

	un32 exception_type;
	un32 reserved;

	double fpscr;
	double fpr[32];

	un16 spinLockCount;
	un16 state;

	un32 gqr[8];
	un32 pir;
	double psf[32];

	un64 coretime[3];
	un64 starttime;

	un32 error;
	un32 attributes;

	un32 pmc1;
	un32 pmc2;
	un32 pmc3;
	un32 pmc4;
	un32 mmcr0;
	un32 mmcr1;
};
typedef struct OSContext OSContext;

typedef int (*ThreadFunc)(int argc, void *argv);

struct OSThread {
    OSContext context;

    un32 txtTag;
    un8 state;
    un8 attr;

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

typedef void  (*DCFlushRange_t)(const void *addr, un32 num);
typedef void  (*ICInvalidateRange_t)(const void *addr, un32 num);

typedef void *(*MEMAllocFromDefaultHeap_t)(un32 size);
typedef void *(*MEMAllocFromDefaultHeapEx_t)(un32 size, int alignment);
typedef void  (*MEMFreeToDefaultHeap_t)(void *ptr);
typedef void *(*OSAllocFromSystem_t)(un32 size, int alignment);
typedef void  (*OSFreeToSystem_t)(void *ptr);
typedef void *(*OSEffectiveToPhysical_t)(void *addr);

typedef OSThread *(*OSGetCurrentThread_t)();
typedef bool  (*OSCreateThread_t)(OSThread *, ThreadFunc entryPoint, int argc, void *argv, void *stack, un32 stackSize, int priority, un16 attr);
typedef bool  (*OSGetActiveThreadLink_t)(OSThread *, OSThreadLink *);
typedef un16   (*OSGetThreadAffinity_t)(OSThread *);
typedef const char *(*OSGetThreadName_t)(OSThread *);
typedef int   (*OSGetThreadPriority_t)(OSThread *);
typedef void  (*OSSetThreadName_t)(OSThread *, const char *name);
typedef int   (*OSResumeThread_t)(OSThread *);
typedef void  (*OSSleepTicks_t)(un64 ticks);

typedef void  (*OSInitMessageQueue_t)(OSMessageQueue *, OSMessage *array, int count);
typedef bool  (*OSReceiveMessage_t)(OSMessageQueue *, OSMessage *, int flags);
typedef bool  (*OSSendMessage_t)(OSMessageQueue *, OSMessage *, int flags);

typedef void  (*OSSetExceptionCallback_t)(un8 type, OSExceptionCallback cb);
typedef void  (*OSSetExceptionCallbackEx_t)(un8 mode, un8 type, OSExceptionCallback cb);
typedef void  (*OSLoadContext_t)(OSContext *);

typedef void  (*OSScreenClearBufferEx_t)(un8 screen, un32 color);
typedef void  (*OSScreenEnableEx_t)(un8 screen, bool enabled);
typedef void  (*OSScreenFlipBuffersEx_t)(un8 screen);
typedef un32   (*OSScreenGetBufferSizeEx_t)(un8 screen);
typedef void  (*OSScreenInit_t)();
typedef void  (*OSScreenPutFontEx_t)(un8 screen, un32 x, un32 y, const char *text);
typedef void  (*OSScreenSetBufferEx_t)(un8 screen, void *ptr);

typedef void (*OSSetDABR_t)(bool allCores, void *address, bool matchReads, bool matchWrites);
typedef void *(*OSGetSymbolName_t)(un32 addr, un8 *symbolName, un32 nameBufSize);
typedef void *(*DisasmGetSym)(un32 addr, un8 *symbolName, un32 nameBufSize);
typedef bool (*DisassemblePPCOpcode_t)(un32 *opcode, char *outputBuffer, un32 bufferSize,
                          DisasmGetSym disasmGetSym, un32 disasmOptions);
struct FSClient {
    un8 buffer[5888];
};
typedef struct FSClient FSClient;

struct FSCmdBlock {
    un8 buffer[2688];
};
typedef struct FSCmdBlock FSCmdBlock;

struct FSStat_ {
    un32 flags;
    un8 _4[0xC];
    un32 size;
    un8 _14[0x50];
};
typedef struct FSStat_ FSStat_;

struct FSDirEntry_ {
    FSStat_ stat;
    char name[256];
};
typedef struct FSDirEntry_ FSDirEntry_;

typedef void(*FSInit_t)();
typedef int (*FSAddClient_t)(FSClient *client, int errHandling);
typedef void(*FSInitCmdBlock_t)(FSCmdBlock *block);
typedef int (*FSGetStat_t)(FSClient *client, FSCmdBlock *block, const char *path, FSStat_ *stat, int errHandling);
typedef int (*FSOpenFile_t)(FSClient *client, FSCmdBlock *block, const char *path, const char *mode, int *handle, int errHandling);
typedef int (*FSReadFile_t)(FSClient *client, FSCmdBlock *block, void *buffer, un32 size, un32 count, int handle, un32 flags, int errHandling);
typedef int (*FSCloseFile_t)(FSClient *client, FSCmdBlock *block, int handle, int errHandling);
typedef int (*FSSetPosFile_t)(FSClient *client, FSCmdBlock *block, int handle, un32 pos, int errHandling);
typedef int (*FSGetStatFile_t)(FSClient *client, FSCmdBlock *block, int handle, FSStat_ *stat, int errHandling);
typedef int (*FSOpenDir_t)(FSClient *client, FSCmdBlock *block, const char *path, int *handle, int errHandling);
typedef int (*FSReadDir_t)(FSClient *client, FSCmdBlock *block, int handle, FSDirEntry_ *entry, int errHandling);
typedef int (*FSCloseDir_t)(FSClient *client, FSCmdBlock *block, int handle, int errHandling);


#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

#define SOL_SOCKET -1
#define SO_REUSEADDR 4

struct sockaddr_ {
	unsigned short sin_family;
	unsigned short sin_port;
	unsigned long sin_addr;
	char sin_zero[8];
};
typedef struct sockaddr_ sockaddr_;

typedef int (*SOInit_t)();
typedef int (*SOSocket_t)(int family, int type, int proto);
typedef int (*SOSetSockOpt_t)(int fd, int level, int optname, void *optval, int optlen);
typedef int (*SOConnect_t)(int fd, sockaddr_ *addr, int addrlen);
typedef int (*SOBind_t)(int fd, sockaddr_ *addr, int addrlen);
typedef int (*SOListen_t)(int fd, int backlog);
typedef int (*SOAccept_t)(int fd, sockaddr_ *addr, int *addrlen);
typedef int (*SORecv_t)(int fd, void *buffer, int len, int flags);
typedef int (*SOSend_t)(int fd, const void *buffer, int len, int flags);
typedef int (*SOClose_t)(int fd);
typedef int (*SOFinish_t)();
typedef int (*SOLastError_t)();

typedef int (* SYSCheckTitleExists_t)(un64 titleId);
typedef int (* SYSLaunchTitle_t)(un64 titleId);
typedef int (*_SYSLaunchTitleByPathFromLauncher_t)(const char* path, int len, int zero);

#define VPAD_BUTTON_HOME 0x2

struct VPADStatus {
	un32 hold;
	un32 trig;
	un32 release;

	Vec2 lStick;
	Vec2 rStick;

	char _14[0xA4 - 0x14];
};
typedef struct VPADStatus VPADStatus;

typedef int (*VPADRead_t)(int chan, VPADStatus buffer[], un32 length, int* err);
