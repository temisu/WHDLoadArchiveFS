/* Copyright (C) Teemu Suutari */

#include "archivefs_private.h"
#include "archivefs_integration.h"
#include "archivefs_common.h"

static int32_t archivefs_fileReadRaw(struct archivefs_combined_state *combined,void *dest,struct archivefs_cached_file_entry *entry,uint32_t length,uint32_t offset)
{
	int ret;
	struct archivefs_state *archive=&combined->archive;

	if (combined->currentFile!=entry)
	{
		ret=archive->fileOpen(&combined->fileState,entry);
		if (ret<0) return ret;
		combined->currentFile=entry;
	}
	return archive->fileRead(dest,&combined->fileState,length,offset);
}

/*
   createFIB function must be run on a Big Endian machine for the results to be really meaningful
   Otherwise useful only for testing
*/

static void archivefs_createFIB(struct FIB *dest,const struct archivefs_cached_file_entry *entry)
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

static struct archivefs_cached_file_entry *archivefs_findEntry(struct archivefs_state *archive,const char *name)
{
	struct archivefs_cached_file_entry *entry;
	const char *name1,*name2;

	entry=archive->firstEntry;
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

int archivefs_initialize(void **_archive,const char *filename)
{
	int ret;
	uint8_t hdr[3];
	struct archivefs_combined_state *combinedState;

	combinedState=archivefs_malloc(sizeof(struct archivefs_combined_state));
	if (!combinedState)
		return ARCHIVEFS_ERROR_MEMORY_ALLOCATION_FAILED;
	combinedState->archive.uninitialize=0;
	combinedState->fileState.archive=&combinedState->archive;

	ret=archivefs_integration_fileOpen(filename,&combinedState->archive.fileLength,&combinedState->archive.file);
	if (!ret)
	{
		combinedState->archive.firstEntry=0;
		combinedState->archive.lastEntry=0;
		combinedState->currentFile=0;
	} else {
		combinedState->archive.file=0;
	}

	if (!ret)
	{
		/* although not good enough for the generic case, this works for amiga lha/zip archives */
		ret=archivefs_integration_fileRead(hdr,3,2,combinedState->archive.file);
		if (ret>=0 && ret!=3) ret=ARCHIVEFS_ERROR_INVALID_FORMAT;
		else
		{
			if (hdr[0]=='-' && hdr[1]=='l' && hdr[2]=='h') ret=archivefs_lha_initialize(&combinedState->archive);
				else ret=archivefs_zip_initialize(&combinedState->archive);
		}
	}

	if (ret) archivefs_uninitialize(combinedState);
	*_archive=ret?0:combinedState;
	return ret;
}

int archivefs_uninitialize(void *_archive)
{
	struct archivefs_cached_file_entry *tmp,*prev;
	struct archivefs_state *archive=&((struct archivefs_combined_state*)_archive)->archive;

	if (archive->uninitialize)
		archive->uninitialize(archive);

	tmp=archive->lastEntry;
	while (tmp)
	{
		prev=tmp->prev;
		archivefs_free(tmp);
		tmp=prev;
	}
	archive->firstEntry=0;
	archive->lastEntry=0;
	if (archive->file)
		archivefs_integration_fileClose(archive->file);
	archivefs_free(_archive);
	return 0;
}

int32_t archivefs_getFileSize(void *_archive,const char *name)
{
	const struct archivefs_cached_file_entry *entry;
	struct archivefs_state *archive=&((struct archivefs_combined_state*)_archive)->archive;

	entry=archivefs_findEntry(archive,name);
	if (!entry)
		return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
	if (entry->fileType!=ARCHIVEFS_TYPE_FILE)
		return ARCHIVEFS_ERROR_INVALID_FILE_TYPE;
	return entry->length;
}

int archivefs_fileCache(void *_archive,archivefs_allocFile fileFunc)
{
	void *ptr;
	int32_t ret;
	struct archivefs_cached_file_entry *tmp;
	struct archivefs_combined_state *combined=(struct archivefs_combined_state*)_archive;
	struct archivefs_state *archive=&combined->archive;

	tmp=archive->firstEntry;
	while (tmp)
	{
		if (tmp->fileType==ARCHIVEFS_TYPE_FILE)
		{
			ptr=fileFunc(tmp->pathAndName,tmp->length);
			if (!ptr) break;
			if (((int)ptr)!=-1)
			{
				ret=archivefs_fileReadRaw(combined,ptr,tmp,tmp->length,0);
				if (ret<0) return ret;
			}
		}
		tmp=tmp->next;
	}
	return 0;
}

int archivefs_dirCache(void *_archive,archivefs_registerEntry registerFunc)
{
	struct archivefs_cached_file_entry *tmp;
	uint32_t i,ret;
	/* making FIB static would save some stack, but then this func is not re-entrant */
	struct FIB fib;
	struct archivefs_state *archive=&((struct archivefs_combined_state*)_archive)->archive;

	for (i=0;i<sizeof(fib)/4;i++) ((uint32_t*)&fib)[i]=0;
	tmp=archive->firstEntry;
	while (tmp)
	{
		archivefs_createFIB(&fib,tmp);
		ret=registerFunc(tmp->path,&fib);
		if (!ret) break;
		tmp=tmp->next;
	}
	return 0;
}

int32_t archivefs_fileRead(void *_archive,void *dest,const char *name,uint32_t length,uint32_t offset)
{
	struct archivefs_combined_state *combined=(struct archivefs_combined_state*)_archive;
	struct archivefs_state *archive=&combined->archive;
	struct archivefs_cached_file_entry *entry;
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
		entry=archivefs_findEntry(archive,name);
		if (!entry)
			return ARCHIVEFS_ERROR_FILE_NOT_FOUND;
		if (entry->fileType!=ARCHIVEFS_TYPE_FILE)
			return ARCHIVEFS_ERROR_INVALID_FILE_TYPE;
	} else {
		entry=combined->currentFile;
	}
	return archivefs_fileReadRaw(combined,dest,entry,length,offset);
}

static const char *archivefs_errors[]={
	"no error",
	"Invalid input file format",
	"Unsupported input file format",
	"Memory allocation failed",
	"File not found",
	"Unsupported input file format (non Amiga archive)",
	"Invalid file type",
	"Decompression failure",
	"Invalid read parameters"
	"Unknown error"};

const char *archivefs_getErrorString(int error_code)
{
	if (error_code<0) error_code=-error_code;
		else error_code=0;
	if (error_code>9) error_code=9;
	return archivefs_errors[error_code];
}
