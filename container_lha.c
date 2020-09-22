/* Copyright (C) Teemu Suutari */

#include "container_private.h"
#include "container_common.h"

#define HDR_SIZE (22)
#define HDR_OFFSET_SIZE (0)
#define HDR_OFFSET_CHECKSUM (1)
#define HDR_OFFSET_METHOD (2)
#define HDR_OFFSET_PACKEDSIZE (7)
#define HDR_OFFSET_RAWSIZE (11)
#define HDR_OFFSET_MTIME (15)
#define HDR_OFFSET_ATTRIBUTES (19)
#define HDR_OFFSET_LEVEL (20)
#define HDR_OFFSET_NAMELENGTH (21)

#define HDR_MID_SIZE (3)
#define HDR_MID_OFFSET_CRC (0)
#define HDR_MID_OFFSET_OS (2)

#define HDR_EXTRA_PATH_ID (2)

/*
	Reads level1 headers
	Parses filenote out of filename (if amiga archive)
	Expected types of lh0, lh5 and lhd
	Reads only path from extraheaders
	Does not do CRC
	Does not process symlinks
	Will return error on files not conforming to these rules
*/
static int container_lha_parse_entry(struct container_cached_file_entry **dest,struct container_state *container,uint32_t offset)
{
	int ret;
	uint8_t hdr[HDR_SIZE];
	uint32_t hdrSize,packedLength,rawLength,mtime,attributes,nameLength;
	uint32_t extraLength,pathOffset,pathLength,nameOffset,dataType,i;
	int isAmiga,isDir;
	struct container_cached_file_entry *entry;
	uint8_t *stringSpace;

	if ((ret=container_simpleRead(hdr,HDR_SIZE,offset,container))<0) return ret;

	if (hdr[HDR_OFFSET_METHOD]!='-'||hdr[HDR_OFFSET_METHOD+1]!='l'||hdr[HDR_OFFSET_METHOD+2]!='h'||hdr[HDR_OFFSET_METHOD+4]!='-')
		return CONTAINER_ERROR_INVALID_FORMAT;

	isDir=0;
	if (hdr[HDR_OFFSET_METHOD+3]=='d') isDir=1;
		else if (hdr[HDR_OFFSET_METHOD+3]=='5') dataType=1;
			else if (hdr[HDR_OFFSET_METHOD+3]=='0') dataType=0;
				else return CONTAINER_ERROR_UNSUPPORTED_FORMAT;

	/* Basic stuff */
	hdrSize=hdr[HDR_OFFSET_SIZE];
	if (hdrSize<HDR_SIZE+HDR_MID_SIZE)
		return CONTAINER_ERROR_INVALID_FORMAT;

	packedLength=GET_LE32(hdr+HDR_OFFSET_PACKEDSIZE)+2;
	rawLength=GET_LE32(hdr+HDR_OFFSET_RAWSIZE);
	mtime=GET_LE32(hdr+HDR_OFFSET_MTIME);
	attributes=hdr[HDR_OFFSET_ATTRIBUTES];
	if (hdr[HDR_OFFSET_LEVEL]!=1)
		return CONTAINER_ERROR_UNSUPPORTED_FORMAT;

	nameLength=hdr[HDR_OFFSET_NAMELENGTH];
	if (hdrSize<HDR_SIZE+HDR_MID_SIZE+nameLength)
		return CONTAINER_ERROR_INVALID_FORMAT;

	nameOffset=offset+HDR_SIZE;
	if ((ret=container_simpleRead(hdr,1,offset+HDR_SIZE+nameLength+HDR_MID_OFFSET_OS,container))<0) return ret;
	isAmiga=hdr[0]=='A';
#ifndef CONTAINER_ALLOW_NON_AMIGA_ARCHIVES
	if (!isAmiga)
		return CONTAINER_ERROR_NON_AMIGA_ARCHIVE;
#endif
	offset+=hdrSize;

	/* Extra headers i.e. lets find the path and the start of the packed data */
	pathOffset=0;
	pathLength=0;
	for (;;)
	{
		/* if the last item does not have a path and if we are missing the file EOF marker,
		   we can only read 2 bytes, which are null, so skip */
		if (offset+3>container->fileLength)
		{
			offset+=2;
			if (packedLength<2)
				return CONTAINER_ERROR_INVALID_FORMAT;
			packedLength-=2;
			break;
		}
		if ((ret=container_simpleRead(hdr,3,offset,container))<0) return ret;
		offset+=2;
		if (packedLength<2)
			return CONTAINER_ERROR_INVALID_FORMAT;
		packedLength-=2;
		extraLength=GET_LE16(hdr);
		if (!extraLength) break;
		offset++;
		if (!packedLength)
			return CONTAINER_ERROR_INVALID_FORMAT;
		packedLength--;
		if (extraLength<3)
			return CONTAINER_ERROR_INVALID_FORMAT;
		extraLength-=3;
		if (hdr[2]==HDR_EXTRA_PATH_ID)
		{
			pathOffset=offset;
			pathLength=extraLength;
		}
		offset+=extraLength;
		if (packedLength<extraLength)
			return CONTAINER_ERROR_INVALID_FORMAT;
		packedLength-=extraLength;
	}

	if (isDir)
	{
		packedLength=0;
		rawLength=0;
	} else if (!dataType&&packedLength!=rawLength)
		return CONTAINER_ERROR_INVALID_FORMAT;

	/* allocate the cached entry + enough size for strings and their null terminators */

	entry=container_malloc(sizeof(struct container_cached_file_entry)+nameLength+pathLength+3);
	*dest=entry;
	if (!entry)
		return CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED;
	entry->prev=0;
	entry->next=0;
	stringSpace=(void*)(entry+1);


	/* now process strings */
	if (pathLength)
	{
		if ((ret=container_simpleRead(stringSpace,pathLength,pathOffset,container))<0) return ret;
		for (i=0;i<pathLength;i++)
			if ((uint8_t)(stringSpace[i])==0xffU) stringSpace[i]='/';

		if (stringSpace[pathLength-1]!='/')
		{
			stringSpace[pathLength]='/';
			pathLength++;
		}
	}

	if (nameLength) if ((ret=container_simpleRead(stringSpace+pathLength,nameLength,nameOffset,container))<0) return ret;
	stringSpace[nameLength+pathLength]=0;
	if (stringSpace[nameLength+pathLength-1]=='/')
		stringSpace[nameLength+pathLength-1]=0;
	entry->filename=(char*)stringSpace;
	entry->filenote=(char*)stringSpace+pathLength+nameLength;

	/* funky way to store filenote */
	if (isAmiga) for (i=pathLength;i<pathLength+nameLength;i++)
	{
		if (!stringSpace[i]&&stringSpace[i+1])
		{
			if (i&&stringSpace[i-1]=='/')
				stringSpace[i-1]=0;
			entry->filenote=(char*)stringSpace+i+1;
			break;
		}
	}

	/* non standard empty directory handling for amiga */
	if (!packedLength&&!rawLength&&!stringSpace[pathLength])
		isDir=1;

	/* whew */

	entry->dataOffset=isDir?0:offset;
	entry->dataLength=packedLength;
	entry->dataType=dataType;
	entry->length=rawLength;

	entry->fileType=isDir?CONTAINER_TYPE_DIR:CONTAINER_TYPE_FILE;
	entry->protection=attributes;
	entry->mtimeDays=container_dosTimeToAmiga(mtime,&entry->mtimeMinutes,&entry->mtimeTicks);
#if 0
	printf("dataOffset 0x%x\n",entry->dataOffset);
	printf("dataLength 0x%x\n",entry->dataLength);
	printf("dataType 0x%x\n",entry->dataType);
	printf("length 0x%x\n",entry->length);
	printf("fileType %d\n",entry->fileType);
	printf("protection 0x%x\n",entry->protection);
	printf("mtime_days 0x%x\n",entry->mtimeDays);
	printf("mtime_minutes 0x%x\n",entry->mtimeMinutes);
	printf("mtime_ticks 0x%x\n",entry->mtimeTicks);
	printf("filename '%s'\n",entry->filename);
	printf("filenote '%s'\n",entry->filenote);
#endif
	return offset+packedLength;
}

static const struct container_cached_file_entry *container_lha_fileOpen(struct container_file_state *file_state,const char *name)
{
	const struct container_cached_file_entry *entry;
	entry=container_common_findEntry(file_state->container,name);
	file_state->state.lha.entry=entry;
	return entry;
}

static int container_lha_fileRead(void *dest,struct container_file_state *file_state,uint32_t length,uint32_t offset)
{
	/* does not check file type */
	const struct container_cached_file_entry *entry;

	entry=file_state->state.lha.entry;
	if (entry->dataType)
		return CONTAINER_ERROR_DECOMPRESSION_ERROR;
	if (length+offset>entry->length)
		return CONTAINER_ERROR_INVALID_READ;
	return container_simpleRead(dest,length,entry->dataOffset+offset,file_state->container);
}

int container_lha_initialize(struct container_state *container)
{
	struct container_cached_file_entry *entry;
	int offset=0;

	/* there might (should) be a stop character at the end of the file */
	while (offset+1<container->fileLength)
	{
		offset=container_lha_parse_entry(&entry,container,offset);
		if (offset<0) return offset;
		container_sortAndInsert(container,entry);
	}
	container->fileOpen=container_lha_fileOpen;
	container->fileRead=container_lha_fileRead;
	return 0;
}
