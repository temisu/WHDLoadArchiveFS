#include <exec/types.h>
#include <proto/dos.h>

#include "archivefs_lha_decompressor.h"
#include "archivefs_integration.h"

//#define DEBUG

#ifdef __VBCC__
	#define REG(reg, arg) __reg(#reg) arg
	#define ASM_FUNC
#elif defined(__SASC)
	#define REG(reg, arg) register __ ## reg arg
	#define ASM_FUNC __asm
#else
	#define REG(reg, arg) arg __asm(#reg)
	#define ASM_FUNC
#endif

#define INVALID_WINDOW_POS (~0U)

#ifdef __SASC
/* SAS/C (C89) does not support C99 variadic macros */
static void LOG_noop(const char *fmt, ...) { (void)fmt; }
#define FORCE_LOG LOG_noop
#define LOG LOG_noop
#else
#define FORCE_LOG(...) do {\
		BPTR handle = Open((uint8_t*)"*", MODE_NEWFILE);\
		FPrintf(handle, (uint8_t*)__VA_ARGS__);\
		Close(handle);\
	} while (0)

#ifdef DEBUG
	#define LOG(...) FORCE_LOG(__VA_ARGS__)
#else
	#define LOG(...)
#endif
#endif

typedef struct UnlhaState UnlhaState;

#ifdef __SASC
/* SAS/C does not support __asm or __reg in function pointer typedefs */
typedef uint32_t (*ReadCallbackType)(void* context, uint8_t** buf);
typedef int (*WriteCallbackType)(void* context, uint8_t* buf, uint32_t size);
#else
typedef uint32_t (*ReadCallbackType)(REG(a0,void* context), REG(a1,uint8_t** buf));
typedef int (*WriteCallbackType)(REG(a0,void* context),REG(a1,uint8_t* buf),REG(d0,uint32_t size));
#endif

extern uint32_t UnlhaStateSize(void);
#ifdef __SASC
extern int32_t Unlha(UnlhaState*, void*, ReadCallbackType, WriteCallbackType, uint8_t*, uint32_t, uint16_t);
#pragma regcall(Unlha(a6,a0,a1,a2,a3,d0,d1))
extern int32_t UnlhaComplete(UnlhaState*, void*, ReadCallbackType, uint8_t*, uint32_t, uint16_t);
#pragma regcall(UnlhaComplete(a6,a0,a1,a2,d0,d1))
extern int32_t UnlhaResume(UnlhaState*);
#pragma regcall(UnlhaResume(a6))
#else
extern int32_t Unlha(REG(a6,UnlhaState* state),REG(a0,void* context),REG(a1,ReadCallbackType readCallback),REG(a2,WriteCallbackType writeCallback),REG(a3,uint8_t* window),REG(d0,uint32_t outSize),REG(d1,uint16_t windowBits));
extern int32_t UnlhaComplete(REG(a6,UnlhaState* state), REG(a0,void* context), REG(a1,ReadCallbackType ReadCallback), REG(a2,uint8_t* Dest), REG(d0,uint32_t OutSize), REG(d1,uint16_t WindowBits));
extern int32_t UnlhaResume(REG(a6,UnlhaState* state));
#endif

static uint32_t ASM_FUNC readCallback(REG(a0,void* context), REG(a1,uint8_t** buf))
{
	struct archivefs_lhaDecompressState* state=context;
	const uint32_t rem=state->fileLength-state->filePos;
	int32_t size=archivefs_common_readNextBytes(buf,rem,state->archive);
	state->filePos+=size;
	LOG("readCallback rem=%lx size=%lx ptr=%lx\n",rem,size,(uint32_t)*buf);
	return (uint32_t)size;
}

static int serviceFromWindow(struct archivefs_lhaDecompressState *state)
{
	uint32_t windowEnd;
	uint32_t windowOfs;
	uint32_t windowMax;
	uint32_t len;
	windowEnd=state->windowStart+state->windowSize;
	if (state->reqOffset>=windowEnd)
		return 1;
	windowOfs=state->reqOffset-state->windowStart;
	windowMax=state->windowSize-windowOfs;
	len=state->reqLength;
	if (len>windowMax)
		len=windowMax;
	LOG("Window [%lx;%lx] (Max %lx) Req [%lx;%lx]\n",state->windowStart,windowEnd,windowMax,state->reqOffset,state->reqLength);
	LOG("serviceFromWindow ofs=%lx len=%lx\n",windowOfs,len);
	archivefs_common_memcpy(state->reqBuf,state->window+windowOfs,len);
	state->reqBuf+=len;
	state->reqOffset+=len;
	state->reqLength-=len;
	return state->reqLength!=0;
}

static int ASM_FUNC writeCallback(REG(a0,void* context),REG(a1,uint8_t* buf),REG(d0,uint32_t size))
{
	struct archivefs_lhaDecompressState* state=context;
	//LOG("writeCallback size=%lu\n",size);
	state->windowStart+=state->windowSize;
	return serviceFromWindow(state);
}

struct archivefs_lhaDecompressState *archivefs_lhaAllocateDecompressState(int hasLH1,int hasLH45,int hasLH6)
{
	struct archivefs_lhaDecompressState* state;
	uint32_t size=sizeof(*state);
	const uint32_t stateSize=UnlhaStateSize();
	const uint32_t windowSize=hasLH6?ARCHIVEFS_LHA_LH6_HISTORY_SIZE:ARCHIVEFS_LHA_LH5_HISTORY_SIZE;
	size+=stateSize;
	size+=windowSize;
	state=archivefs_malloc(size);
	if (state)
	{
		state->window=(uint8_t*)state+sizeof(*state)+stateSize;
		state->maxWindowSize=windowSize;
	}
	return state;
}

void archivefs_lhaDecompressInitialize(struct archivefs_lhaDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength,uint32_t method)
{
	LOG("\n");
	state->archive=archive;
	state->fileOffset=fileOffset;
	state->fileLength=fileLength;
	state->filePos=0;
	state->rawLength=rawLength;
	state->rawPos=0;
	state->method=method;
	state->windowStart=INVALID_WINDOW_POS;
	state->reqOffset=0;
	state->reqLength=0;
}

static int32_t initBlockBuffer(struct archivefs_lhaDecompressState *state)
{
	return archivefs_common_initBlockBuffer(state->fileOffset+state->filePos,state->archive);
}

int32_t archivefs_lhaDecompress(struct archivefs_lhaDecompressState *state,uint8_t *dest,int32_t length,uint32_t offset)
{
	int32_t ret;
	uint32_t windowBits;
	UnlhaState* unlhaState = (UnlhaState*)((uint8_t*)state+sizeof(*state));
	LOG("length=%lx offset=%lx fileOffset=%lx fileLength=%lx rawLength=%lx method=%lu\n", length, offset, state->fileOffset, state->fileLength, state->rawLength, state->method);
	switch (state->method)
	{
		//case 1: // Not supported yet
		case 4:
		windowBits=12;
		break;

		case 5:
		windowBits=13;
		break;

		case 6:
		windowBits=15;
		break;

		default:
		return WVFS_ERROR_INVALID_FORMAT;
	}

	state->reqBuf=dest;
	state->reqOffset=offset;
	state->reqLength=length;
	state->windowSize=1<<windowBits;

	if (state->windowStart==INVALID_WINDOW_POS||offset<state->windowStart)
	{
		LOG("Restarting\n");
		state->windowStart=0U-state->windowSize;
		state->filePos=0;
		if ((ret=initBlockBuffer(state))<0)
			return ret;
		if (length==state->rawLength)
			ret=UnlhaComplete(unlhaState,state,(ReadCallbackType)&readCallback,dest,state->rawLength,windowBits);
		else
			ret=Unlha(unlhaState,state,(ReadCallbackType)&readCallback,(WriteCallbackType)&writeCallback,state->window,state->rawLength,windowBits);
	}
	else
	{
		if (!serviceFromWindow(state))
		{
			LOG("Serviced from window\n");
			ret=length;
		}
		else
		{
			if ((ret=initBlockBuffer(state))<0)
				return ret;
			LOG("Resume\n");
			ret=UnlhaResume(unlhaState);
		}
	}

	LOG("ret=%lx length=%lx\n",ret,length);
#ifdef DEBUG
	while (ret==-1) {
		for (uint16_t c=0;;++c)
			*(volatile uint16_t*)0xdff180=c;
	}
#endif

	return ret==-1?WVFS_ERROR_DECOMPRESSION_ERROR:length;
}

// vim: set softtabstop=0 noexpandtab:
