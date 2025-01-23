/* Copyright (C) Teemu Suutari */

#include "archivefs_integration.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

void *archivefs_malloc(uint32_t size)
{
	return malloc(size);
}

void archivefs_free(void *ptr)
{
	free(ptr);
}

void archivefs_integration_initialize()
{
	// nothing needed
}

void archivefs_integration_uninitialize()
{
	// nothing needed
}

int archivefs_integration_fileOpen(const char *filename,uint32_t *length,uint8_t *blockShift,void **file)
{
	int fd,l;

	fd=open(filename,O_RDONLY);
	if (fd<0)
		return WVFS_ERROR_FILE_NOT_FOUND;
	l=lseek(fd,0,SEEK_END);
	if (l<0)
		return WVFS_ERROR_INVALID_FORMAT;
	*length=l;
	*blockShift=9U;
	l=lseek(fd,0,SEEK_SET);
	if (l<0)
		return WVFS_ERROR_INVALID_FORMAT;
	*file=(void*)(size_t)fd;
	return 0;
}

int archivefs_integration_fileClose(void *file)
{
	close((size_t)file);
	return 0;
}

int archivefs_integration_fileSeek(uint32_t offset,void *file)
{
	int fd=(size_t)file;
	int ret=lseek(fd,offset,SEEK_SET);
	if (ret<0)
		return WVFS_ERROR_INVALID_FORMAT;
	return 0;
}

int32_t archivefs_integration_fileRead(void *dest,uint32_t length,void *file)
{
	int fd=(size_t)file;
 	int ret=read(fd,dest,length);
	if (ret<0)
		return WVFS_ERROR_INVALID_FORMAT;
 	return ret;
}

