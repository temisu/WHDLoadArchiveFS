/* Copyright (C) Teemu Suutari */

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>

#include "archivefs_integration.h"

#ifdef ARCHIVEFS_STANDALONE
struct ExecBase *SysBase=0;
struct DosLibrary *DOSBase=0;
static int archivefs_integration_initCounter=0;
#endif

void *archivefs_malloc(uint32_t size)
{
	//Printf((unsigned char*)"malloc %ld\n", size);
	return AllocVec(size,MEMF_CLEAR);
}

void archivefs_free(void *ptr)
{
	//Printf((unsigned char*)"free %lx\n", (uint32_t)ptr);
	FreeVec(ptr);
}


void archivefs_integration_initialize()
{
#ifdef ARCHIVEFS_STANDALONE
	if (!archivefs_integration_initCounter++)
	{
		SysBase=(*((struct ExecBase **) 4U));
		DOSBase=(struct DosLibrary *)OpenLibrary((unsigned char *)"dos.library",37);
	}
#endif
}

void archivefs_integration_uninitialize()
{
#ifdef ARCHIVEFS_STANDALONE
	if (!--archivefs_integration_initCounter)
		CloseLibrary((void*)DOSBase);
#endif
}

int archivefs_integration_fileOpen(const char *filename,uint32_t *length,uint8_t *blockShift,void **file)
{
	BPTR fd,lock;
	struct FileInfoBlock fib;
	struct InfoData info;
	int bs;

	lock=Lock((unsigned char *) filename,ACCESS_READ);
	if (!lock)
		return WVFS_ERROR_FILE_NOT_FOUND;

	if (!Examine(lock,&fib) || fib.fib_DirEntryType>0 || !Info(lock,&info)) {
		UnLock(lock);
		return WVFS_ERROR_FILE_NOT_FOUND;
	}

	fd=OpenFromLock(lock);
	if (!fd) {
		UnLock(lock);
		return WVFS_ERROR_FILE_NOT_FOUND;
	}

	*file=(void*)fd;
	*length=fib.fib_Size;
	bs = info.id_BytesPerBlock;
	if (bs < 512) {
		*blockShift=9U;		// 512
	} else if (bs >= 8192) {
		*blockShift=13U;	// 8192
	} else {
		*blockShift = 0;
		while (bs > 1) {
			bs >>= 1;
			(*blockShift)++;
		}
		// *blockShift = ffs(bs) - 1;	// ffs(): unknown to vbcc & SAS/C
	}
	// Printf((unsigned char*)"archive: length=%ld blockshift=%ld blocksize=%ld\n", *length, *blockShift, 1<<*blockShift);
	return 0;
}

int archivefs_integration_fileClose(void *file)
{
	Close((BPTR)file);
	return 0;
}

int archivefs_integration_fileSeek(uint32_t offset,void *file)
{
	//Printf((unsigned char*)"seek %ld\n", offset);
	Seek((BPTR)file,offset,OFFSET_BEGINNING);
	return 0;
}

int32_t archivefs_integration_fileRead(void *dest,uint32_t length,void *file)
{
	int32_t ret;

	//Printf((unsigned char*)"read %ld %lx\n", length, (uint32_t)dest);
	ret=Read((BPTR)file,dest,length);
	if (ret<0)
		return WVFS_ERROR_INVALID_FORMAT;
	return ret;
}

