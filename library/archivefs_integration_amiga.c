/* Copyright (C) Teemu Suutari */

#include "archivefs_integration.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>

#ifdef ARCHIVEFS_STANDALONE
struct ExecBase *SysBase=0;
struct DosLibrary *DOSBase=0;
static int archivefs_integration_initCounter=0;
#endif

void *archivefs_malloc(uint32_t size)
{
	return AllocVec(size,MEMF_CLEAR);
}

void archivefs_free(void *ptr)
{
	FreeVec(ptr);
}


void archivefs_integration_initialize()
{
#ifdef ARCHIVEFS_STANDALONE
	if (!archivefs_integration_initCounter++)
	{
		SysBase=(*((struct ExecBase **) 4U));
		DOSBase=(void*)OpenLibrary("dos.library",37);
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
	BOOL success;

	lock=Lock(filename,ACCESS_READ);
	if (!lock)
		return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
	success=Examine(lock,&fib);
	UnLock(lock);
	if (!success)
		return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
	if (fib.fib_DirEntryType>0)
		return ARCHIVEFS_ERROR_INVALID_FILE_TYPE;

	fd=Open(filename,MODE_OLDFILE);
	if (!fd)
		return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
	*file=(void*)fd;
	*length=fib.fib_Size;
	/* TODO: real block size */
	*blockShift=9U;
	return 0;
}

int archivefs_integration_fileClose(void *file)
{
	Close((BPTR)file);
	return 0;
}

int archivefs_integration_fileSeek(uint32_t offset,void *file)
{
	Seek((BPTR)file,offset,OFFSET_BEGINNING);
	return 0;
}

int32_t archivefs_integration_fileRead(void *dest,uint32_t length,void *file)
{
	int32_t ret;

	ret=Read((BPTR)file,dest,length);
	if (ret<0)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
	return ret;
}

