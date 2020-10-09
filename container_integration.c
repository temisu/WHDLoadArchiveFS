/* Copyright (C) Teemu Suutari */

#include "container_integration.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

/* very unix-ish at the moment */

int container_integration_fileOpen(const char *filename,uint32_t *length,void **file)
{
	int fd=open(filename,O_RDONLY);
	if (fd<0)
	{
		printf("unable to open archive %s\n",filename);
		return -1;
	}
	struct stat st;
	int ret=fstat(fd,&st);
	if (ret<0)
	{
		printf("unable to stat archive %s\n",filename);
		close(fd);
		return -1;
	}
	*file=(void*)(size_t)fd;
	*length=st.st_size;
	return 0;
}

int container_integration_fileClose(void *file)
{
	close((int)file);
	return 0;
}

int container_integration_fileRead(void *dest,uint32_t length,uint32_t offset,void *file)
{
	int fd=(int)file;
	int ret=lseek(fd,offset,SEEK_SET);
	if (ret==-1)
	{
		return -1;
	}
 	ret=read(fd,dest,length);
 	return ret;
}

void *container_malloc(uint32_t size)
{
	return malloc(size);
}

void container_free(void *ptr)
{
	free(ptr);
}
