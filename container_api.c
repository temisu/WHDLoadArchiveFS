/* Copyright (C) Teemu Suutari */

#include "container_private.h"
#include "container_integration.h"
#include "container_common.h"

static int container_fileReadRaw(struct container_combined_state *combined,void *dest,const struct container_cached_file_entry *entry,uint32_t length,uint32_t offset)
{
	int ret;
	struct container_state *container=&combined->container;

	if (combined->currentFile!=entry)
	{
		ret=container->fileOpen(&combined->fileState,entry);
		if (ret<0) return ret;
		combined->currentFile=entry;
	}
	return container->fileRead(dest,&combined->fileState,length,offset);
}

/*
   createFIB function must be run on a Big Endian machine for the results to be really meaningful
   Otherwise useful only for testing
*/

static void container_createFIB(struct FIB *dest,const struct container_cached_file_entry *entry)
{
	uint32_t i;

	dest->diskKey=0;
	dest->dirEntryType=entry->fileType;
	for (i=0;i<107&&entry->filename[i];i++)
		dest->filename[i]=entry->filename[i];
	dest->filename[i]=0;
	dest->protection=entry->protection;
	dest->entryType=entry->fileType;
	dest->size=entry->length;
	dest->blocks=(entry->length+511)&~511U;
	dest->mtimeDays=entry->mtimeDays;
	dest->mtimeMinutes=entry->mtimeMinutes;
	dest->mtimeTicks=entry->mtimeTicks;
	for (i=0;i<79&&entry->filenote[i];i++)
		dest->comment[i]=entry->filenote[i];
	dest->comment[i]=0;
	dest->uid=0;
	dest->gid=0;
}

static const struct container_cached_file_entry *container_findEntry(struct container_state *container,const char *name)
{
	struct container_cached_file_entry *entry;
	const char *name1,*name2;

	entry=container->firstEntry;
	while (entry)
	{
		name1=entry->pathAndName;
		name2=name;
		while(*name1 && *name1==*name2)
		{
			name1++;
			name2++;
		}
		if (!*name1 && !*name2)
			return entry;
		entry=entry->next;
	}
	return 0;
}

/* --- */

int container_initialize(void **_container,const char *filename)
{
	int ret;
	uint8_t hdr[3];
	struct container_combined_state *combinedState;

	combinedState=container_malloc(sizeof(struct container_combined_state));
	if (!combinedState)
		return CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED;
	combinedState->fileState.container=&combinedState->container;

	ret=container_integration_fileOpen(filename,&combinedState->container.fileLength,&combinedState->container.file);
	if (!ret)
	{
		combinedState->container.firstEntry=0;
		combinedState->container.lastEntry=0;
		combinedState->currentFile=0;
		ret=container_integration_fileRead(hdr,3,2,combinedState->container.file);
		if (ret>=0 && ret!=3) ret=CONTAINER_ERROR_INVALID_FORMAT;
	} else {
		combinedState->container.file=0;
	}

	/* although not good enough for the generic case, this works for amiga lha/zip archives */
	if (hdr[0]=='-' && hdr[1]=='l' && hdr[2]=='h') ret=container_lha_initialize(&combinedState->container);
		else ret=container_zip_initialize(&combinedState->container);

	if (ret<0) container_uninitialize(combinedState);
	*_container=ret?0:combinedState;
	return ret;
}

int container_uninitialize(void *_container)
{
	struct container_cached_file_entry *tmp,*prev;
	struct container_state *container=&((struct container_combined_state*)_container)->container;

	tmp=container->lastEntry;
	while (tmp)
	{
		prev=tmp->prev;
		container_free(tmp);
		tmp=prev;
	}
	container->firstEntry=0;
	container->lastEntry=0;
	if (container->file)
		container_integration_fileClose(container->file);
	container_free(_container);
	return 0;
}

int container_getFileSize(void *_container,const char *name)
{
	const struct container_cached_file_entry *entry;
	struct container_state *container=&((struct container_combined_state*)_container)->container;

	entry=container_findEntry(container,name);
	if (!entry)
		return CONTAINER_ERROR_FILE_NOT_FOUND;
	if (entry->fileType!=CONTAINER_TYPE_FILE)
		return CONTAINER_ERROR_INVALID_FILE_TYPE;
	return entry->length;
}

int container_fileCache(void *_container,container_allocFile fileFunc)
{
	void *ptr;
	int ret;
	struct container_cached_file_entry *tmp;
	struct container_combined_state *combined=(struct container_combined_state*)_container;
	struct container_state *container=&combined->container;

	tmp=container->firstEntry;
	while (tmp)
	{
		if (tmp->fileType==CONTAINER_TYPE_FILE)
		{
			ptr=fileFunc(tmp->pathAndName,tmp->length);
			if (!ptr) break;
			if (((int)ptr)!=-1)
			{
				ret=container_fileReadRaw(combined,ptr,tmp,tmp->length,0);
				if (ret<0) return ret;
			}
		}
		tmp=tmp->next;
	}
	return 0;
}

int container_examine(void *_container,container_registerEntry registerFunc)
{
	struct container_cached_file_entry *tmp;
	uint32_t i,ret;
	/* making FIB static would save some stack, but then this func is not re-entrant */
	struct FIB fib;
	struct container_state *container=&((struct container_combined_state*)_container)->container;

	for (i=0;i<sizeof(fib)/4;i++) ((uint32_t*)&fib)[i]=0;
	tmp=container->firstEntry;
	while (tmp)
	{
		container_createFIB(&fib,tmp);
		ret=registerFunc(tmp->path,&fib);
		if (!ret) break;
		tmp=tmp->next;
	}
	return 0;
}

int container_fileRead(void *_container,void *dest,const char *name,uint32_t length,uint32_t offset)
{
	struct container_combined_state *combined=(struct container_combined_state*)_container;
	struct container_state *container=&combined->container;
	const struct container_cached_file_entry *entry;
	const char *name1,*name2;
	int fileLoaded;

	fileLoaded=0;
	if (combined->currentFile)
	{
		name1=name;
		name2=combined->currentFile->pathAndName;
		while(*name1&&*name1==*name2)
		{
			name1++;
			name2++;
		}
		fileLoaded=!*name1&&!*name2;
	}
	if (!fileLoaded)
	{
		entry=container_findEntry(container,name);
		if (!entry)
			return CONTAINER_ERROR_FILE_NOT_FOUND;
		if (entry->fileType!=CONTAINER_TYPE_FILE)
			return CONTAINER_ERROR_INVALID_FILE_TYPE;
	} else {
		entry=combined->currentFile;
	}
	return container_fileReadRaw(combined,dest,entry,length,offset);
}
