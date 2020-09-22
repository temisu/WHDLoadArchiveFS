/* Copyright (C) Teemu Suutari */

#include "container_private.h"
#include "container_common.h"

int container_initialize(void **_container,void *context,container_read_func read_func,uint32_t length)
{
	int ret;
	uint32_t fileCount;
	uint8_t hdr[3];
	struct container_combined_state *combined_state;
	struct container_cached_file_entry *entry;
	
	combined_state=container_malloc(sizeof(struct container_combined_state));
	if (!combined_state)
		return CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED;

	combined_state->container.readContext=context;
	combined_state->container.readFunc=read_func;
	combined_state->container.fileLength=length;
	combined_state->container.firstEntry=0;
	combined_state->container.lastEntry=0;
	combined_state->container.fileList=0;
	combined_state->current_file=0;
	ret=read_func(hdr,3,2,context);
	if (ret<0) return ret;
	if (ret!=3) return CONTAINER_ERROR_INVALID_FORMAT;

	combined_state->examine_state.container=&combined_state->container;
	combined_state->examine_state.current=0;
	combined_state->file_state.container=&combined_state->container;

	/* although not good enough for the generic case, this works for amiga lha/zip archives */
	if (hdr[0]=='-' && hdr[1]=='l' && hdr[2]=='h') ret=container_lha_initialize(&combined_state->container);
		else ret=container_zip_initialize(&combined_state->container);

	if (!ret)
	{
		fileCount=0;
		entry=combined_state->container.firstEntry;
		while (entry)
		{
			if (entry->fileType==CONTAINER_TYPE_FILE)
				fileCount++;
			entry=entry->next;
		}
		combined_state->container.fileList=container_malloc((fileCount+1)*sizeof(char*));
		if (!combined_state->container.fileList)
			ret=CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED;
		
	}
	if (!ret)
	{
		fileCount=0;
		entry=combined_state->container.firstEntry;
		while (entry)
		{
			if (entry->fileType==CONTAINER_TYPE_FILE)
				combined_state->container.fileList[fileCount++]=entry->filename;
			entry=entry->next;
		}
		combined_state->container.fileList[fileCount]=0;
	}
	if (ret<0) container_uninitialize(combined_state);
	*_container=ret?0:combined_state;
	return ret;
}

int container_uninitialize(void *_container)
{
	struct container_cached_file_entry *tmp,*prev;
	struct container_state *container=&((struct container_combined_state*)_container)->container;

	if (container->fileList) container_free(container->fileList);
	container->fileList=0;
	tmp=container->lastEntry;
	while (tmp)
	{
		prev=tmp->prev;
		container_free(tmp);
		tmp=prev;
	}
	container->firstEntry=0;
	container->lastEntry=0;
	container_free(_container);
	return 0;
}

const char **container_getFileList(void *_container)
{
	struct container_state *container=&((struct container_combined_state*)_container)->container;
	return (const char**)container->fileList;
}

int container_getFileSize(void *_container,const char *name)
{
	struct container_state *container=&((struct container_combined_state*)_container)->container;
	return container_common_getFileSize(container,name);
}

int container_isCompressed(void *_container,const char *name)
{
	struct container_state *container=&((struct container_combined_state*)_container)->container;
	return container_common_isCompressed(container,name);
}

extern int container_examine(void *dest_fib,void *_container,const char *name)
{
	struct container_examine_state *examine_state=&((struct container_combined_state*)_container)->examine_state;
	return container_common_examine(dest_fib,examine_state,name);
}

extern int container_exnext(void *dest_fib,void *_container)
{
	struct container_examine_state *examine_state=&((struct container_combined_state*)_container)->examine_state;
	return container_common_exnext(dest_fib,examine_state);
}

extern int container_fileRead(void *dest,void *_container,const char *name,uint32_t length,uint32_t offset)
{
	struct container_combined_state *combined=(struct container_combined_state*)_container;
	struct container_state *container=&combined->container;
	const struct container_cached_file_entry *entry;
	const char *name1,*name2;
	int fileLoaded;

	fileLoaded=0;
	if (combined->current_file)
	{
		name1=name;
		name2=combined->current_file;
		while(*name1&&*name1==*name2)
		{
			name1++;
			name2++;
		}
		fileLoaded=!*name1&&!*name2;
	}
	if (!fileLoaded)
	{
		entry=container->fileOpen(&combined->file_state,name);
		if (!entry)
			return CONTAINER_ERROR_FILE_NOT_FOUND;
		if (entry->fileType!=CONTAINER_TYPE_FILE)
			return CONTAINER_ERROR_INVALID_FILE_TYPE;
		combined->current_file=entry->filename;
	}
	return container->fileRead(dest,&combined->file_state,length,offset);
}
