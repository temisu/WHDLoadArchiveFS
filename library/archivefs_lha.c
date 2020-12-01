/* Copyright (C) Teemu Suutari */

#include "archivefs_private.h"
#include "archivefs_common.h"
#include "archivefs_integration.h"
#include "archivefs_lha_decompressor.h"

#define HDR_L0_SIZE (22)
#define HDR_L0_OFFSET_SIZE (0)
#define HDR_L0_OFFSET_CHECKSUM (1)
#define HDR_L0_OFFSET_METHOD (2)
#define HDR_L0_OFFSET_PACKEDSIZE (7)
#define HDR_L0_OFFSET_RAWSIZE (11)
#define HDR_L0_OFFSET_MTIME (15)
#define HDR_L0_OFFSET_ATTRIBUTES (19)
#define HDR_L0_OFFSET_LEVEL (20)
#define HDR_L0_OFFSET_NAMELENGTH (21)

#define HDR_L2_SIZE (24)
#define HDR_L2_OFFSET_SIZE (0)
#define HDR_L2_OFFSET_METHOD (2)
#define HDR_L2_OFFSET_PACKEDSIZE (7)
#define HDR_L2_OFFSET_RAWSIZE (11)
#define HDR_L2_OFFSET_MTIME (15)
#define HDR_L2_OFFSET_RESERVED (19)
#define HDR_L2_OFFSET_LEVEL (20)
#define HDR_L2_OFFSET_CRC (21)
#define HDR_L2_OFFSET_OS (23)

#define HDR_L0_MID_SIZE (2)
#define HDR_L1_MID_SIZE (3)
#define HDR_L1_MID_OFFSET_CRC (0)
#define HDR_L1_MID_OFFSET_OS (2)

#define HDR_EXTRA_FILENAME_ID (0x01U)
#define HDR_EXTRA_PATH_ID (0x02U)
#define HDR_EXTRA_ATTRIBUTES_ID (0x40U)
#define HDR_EXTRA_FILENOTE_ID (0x71U)

/*
   Reads level0, level1 and level2 headers
   Parses filenote out of filename (if amiga archive) for level0, level1
   Expected methods: lh0, lhd
   Does not process any fancy stuff f.e. symlinks
   Will return error on files are not conforming to these rules

   Rather complex due to intertwined logic of different levels which still share quite a lot of common, but with quirks...
   In theory should read non-Amiga archives with good success, but this is not a priority
*/
static int archivefs_lha_parse_entry(struct archivefs_cached_file_entry **dest,struct archivefs_state *archive,uint32_t offset)
{
	int ret;
	uint8_t hdr[HDR_L2_SIZE];
	uint32_t hdrSize,packedLength,rawLength,mtime,attributes,level;
	uint32_t extraLength,maxExtraOffset,pathOffset,pathLength,nameOffset,nameLength,noteOffset,noteLength,dataType,i;
	int isAmiga,isDir;
	struct archivefs_cached_file_entry *entry;
	uint8_t *stringSpace;

	*dest=0;
	if ((ret=archivefs_common_read(hdr,HDR_L0_SIZE,offset,archive))<0) return ret;

	/* Basic stuff */
	level=hdr[HDR_L0_OFFSET_LEVEL];
	if (level>2)
		return ARCHIVEFS_ERROR_UNSUPPORTED_FORMAT;
	if (level==2)
		if ((ret=archivefs_common_read(hdr+HDR_L0_SIZE,HDR_L2_SIZE-HDR_L0_SIZE,offset+HDR_L0_SIZE,archive))<0) return ret;

	if (hdr[HDR_L0_OFFSET_METHOD]!='-'||hdr[HDR_L0_OFFSET_METHOD+1]!='l'||hdr[HDR_L0_OFFSET_METHOD+2]!='h'||hdr[HDR_L0_OFFSET_METHOD+4]!='-')
		return ARCHIVEFS_ERROR_INVALID_FORMAT;

	/* Amiga does not use -lhd- method for empty directories at any level, but we can still support it */
	isDir=0;
	switch (hdr[HDR_L0_OFFSET_METHOD+3])
	{
		case 'd':
		dataType=0;
		isDir=1;
		break;

		case '0':
		dataType=0;
		break;

		case '4':
		dataType=4;
		break;

		case '5':
		dataType=5;
		break;

		default:
		return ARCHIVEFS_ERROR_UNSUPPORTED_FORMAT;
	}

	/* common for all header levels */
	packedLength=GET_LE32(hdr+HDR_L0_OFFSET_PACKEDSIZE)+2;
	rawLength=GET_LE32(hdr+HDR_L0_OFFSET_RAWSIZE);
	mtime=GET_LE32(hdr+HDR_L0_OFFSET_MTIME);

	if (level<2)
	{
		hdrSize=hdr[HDR_L0_OFFSET_SIZE];
		attributes=hdr[HDR_L0_OFFSET_ATTRIBUTES];
		nameLength=hdr[HDR_L0_OFFSET_NAMELENGTH];
		nameOffset=offset+HDR_L0_SIZE;
	}

	pathOffset=0;
	pathLength=0;
	noteOffset=0;
	noteLength=0;
	if (level) {
		if (level==2)
		{
			hdrSize=GET_LE16(hdr+HDR_L2_OFFSET_SIZE);
			if (hdrSize<HDR_L2_SIZE)
				return ARCHIVEFS_ERROR_INVALID_FORMAT;
			attributes=0;
			nameLength=0;
			nameOffset=0;
			isAmiga=hdr[HDR_L2_OFFSET_OS]=='A';
			maxExtraOffset=offset+hdrSize;
			offset+=HDR_L2_SIZE;
		} else {
			if (hdrSize<HDR_L0_SIZE+HDR_L1_MID_SIZE+nameLength)
				return ARCHIVEFS_ERROR_INVALID_FORMAT;
			if ((ret=archivefs_common_read(hdr,1,offset+HDR_L0_SIZE+nameLength+HDR_L1_MID_OFFSET_OS,archive))<0) return ret;
			isAmiga=hdr[0]=='A';
			if (offset+hdrSize>archive->fileLength)
				return ARCHIVEFS_ERROR_INVALID_FORMAT;
			maxExtraOffset=archive->fileLength;
			offset+=hdrSize;
		}

		/* Extra headers i.e. lets find the path and the start of the packed data */
		i=0;
		for (;;)
		{
			/* if the last item does not have a path and if we are missing the file EOF marker,
			   we can only read 2 bytes, which are null, so skip */
			if (offset+3>maxExtraOffset)
			{
				offset+=2;
				i+=2;
				break;
			}
			if ((ret=archivefs_common_read(hdr,3,offset,archive))<0) return ret;
			offset+=2;
			i+=2;
			extraLength=GET_LE16(hdr);
			if (!extraLength) break;
			offset++;
			i++;
			if (extraLength<3)
				return ARCHIVEFS_ERROR_INVALID_FORMAT;
			extraLength-=3;
			switch (hdr[2])
			{
				case HDR_EXTRA_FILENAME_ID:
				nameOffset=offset;
				nameLength=extraLength;
				break;

				case HDR_EXTRA_PATH_ID:
				pathOffset=offset;
				pathLength=extraLength;
				break;

				case HDR_EXTRA_ATTRIBUTES_ID:
				if (extraLength<1)
					return ARCHIVEFS_ERROR_INVALID_FORMAT;
				if ((ret=archivefs_common_read(hdr,1,offset,archive))<0) return ret;
				attributes=hdr[0];
				break;

				case HDR_EXTRA_FILENOTE_ID:
				noteOffset=offset;
				noteLength=extraLength;
				break;

				default:
				break;
			}
			offset+=extraLength;
			i+=extraLength;
		}
		/* Technically the first extra header field is part of the main header in level2... */
		if (level==2) i=2;
		if (packedLength<i)
			return ARCHIVEFS_ERROR_INVALID_FORMAT;
		packedLength-=i;
	} else {
		/* no way to encode os - lets play pretend */
		isAmiga=1;

		if (hdrSize<HDR_L0_SIZE+nameLength)
			return ARCHIVEFS_ERROR_INVALID_FORMAT;
		if (offset+hdrSize+HDR_L0_MID_SIZE>archive->fileLength)
			return ARCHIVEFS_ERROR_INVALID_FORMAT;
		offset+=hdrSize+HDR_L0_MID_SIZE;
		/* extra stuff (CRC) are not part of header */
		if (packedLength<HDR_L0_MID_SIZE)
			return ARCHIVEFS_ERROR_INVALID_FORMAT;
		packedLength-=HDR_L0_MID_SIZE;
	}

#ifndef ARCHIVEFS_ALLOW_NON_AMIGA_ARCHIVES
	if (!isAmiga)
		return ARCHIVEFS_ERROR_NON_AMIGA_ARCHIVE;
#endif

	if (isDir)
	{
		packedLength=0;
		rawLength=0;
	} else if (!dataType&&packedLength!=rawLength)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;

	/* Allocate the cached entry + enough size for strings and their null terminators
	   This is little bit wasteful for level 0 headers. Those files should be rare though!
	   */
	if (level==2)
	{
		i=nameLength+noteLength+pathLength*2+4;
	} else if (level) {
		i=nameLength+pathLength*2+3;
	} else {
		i=nameLength*2+3;
	}
	entry=archivefs_malloc(sizeof(struct archivefs_cached_file_entry)+i);
	*dest=entry;
	if (!entry)
		return ARCHIVEFS_ERROR_MEMORY_ALLOCATION_FAILED;
	entry->prev=0;
	entry->next=0;
	stringSpace=(void*)(entry+1);


	/* now process strings */
	if (pathLength)
	{
		if ((ret=archivefs_common_read(stringSpace,pathLength,pathOffset,archive))<0) return ret;
		for (i=0;i<pathLength;i++)
			if ((uint8_t)(stringSpace[i])==0xffU) stringSpace[i]='/';

		if (stringSpace[pathLength-1]!='/')
		{
			stringSpace[pathLength]='/';
			pathLength++;
		}
	}

	if (nameLength)
		if ((ret=archivefs_common_read(stringSpace+pathLength,nameLength,nameOffset,archive))<0) return ret;
	if (noteLength)
	{
		/* lets make level2 look like level0-1 filenote. it is easier this way, although it requires string search later
		   level1 is still the most common case and level2 is just corner case
		 */
		stringSpace[pathLength+nameLength]=0;
		if ((ret=archivefs_common_read(stringSpace+pathLength+nameLength+1,noteLength,noteOffset,archive))<0) return ret;
		nameLength+=noteLength+1;
	}
	if (!level)
	{
		/* Level0 headers encode whole name + path into a single string stored in the filename
		   Instead of 0xff, level 0 headers use backslash for directory separator
		   Empty directories are marked with trailing slash

		   Process the info so that we can get similar functionality as in other levels
		*/
		pathLength=0;
		if (nameLength) for (i=0;i<nameLength;i++)
		{
			if (!stringSpace[i]) break;
			if (stringSpace[i]=='\\')
			{
				stringSpace[i]='/';
				pathLength=i+1;
			}
		}
		nameLength-=pathLength;
	}

	entry->pathAndName=(char*)stringSpace;
	stringSpace[pathLength+nameLength]=0;
	entry->filenote=(char*)stringSpace+pathLength*2+nameLength+1;
	*entry->filenote=0;
	entry->path=(char*)stringSpace+pathLength+nameLength+1;

	/* funky way to store filenote */
	if (isAmiga) for (i=pathLength;i<pathLength+nameLength;i++)
	{
		if (!stringSpace[i])
		{
			entry->filenote=(char*)stringSpace+i+1;
			nameLength=i-pathLength;
			break;
		}
	}

	if (!nameLength&&pathLength)
	{
		if (stringSpace[pathLength-1]=='/')
		stringSpace[--pathLength]=0;
	}

	/* non standard empty directory handling for amiga */
	if (!packedLength&&!rawLength&&!stringSpace[pathLength])
		isDir=1;

	if (isDir&&!nameLength)
	{
		while (pathLength)
		{
			if (stringSpace[pathLength-1]=='/') break;
			pathLength--;
			nameLength++;
		}
	}

	/* more processing for filenames */
	entry->filename=(char*)stringSpace+pathLength;
	if (pathLength)
	{
		for (i=0;i<pathLength-1;i++)
			entry->path[i]=entry->pathAndName[i];
		entry->path[pathLength-1]=0;
	} else {
		*entry->path=0;
	}


	/* whew */

	entry->dataOffset=isDir?0:offset;
	entry->dataLength=packedLength;
	entry->dataType=dataType;
	entry->length=rawLength;

	entry->fileType=isDir?ARCHIVEFS_TYPE_DIR:ARCHIVEFS_TYPE_FILE;
	entry->protection=attributes;
	if (level==2) entry->mtimeDays=archivefs_common_unixTimeToAmiga(mtime,&entry->mtimeMinutes,&entry->mtimeTicks);
		else entry->mtimeDays=archivefs_common_dosTimeToAmiga(mtime,&entry->mtimeMinutes,&entry->mtimeTicks);
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
	printf("path '%s'\n",entry->path);
	printf("pathAndName '%s'\n",entry->pathAndName);
	printf("filename '%s'\n",entry->filename);
	printf("filenote '%s'\n\n",entry->filenote);
#endif
	return offset+packedLength;
}

static int archivefs_lha_fileOpen(struct archivefs_file_state *file_state,struct archivefs_cached_file_entry *entry)
{
	file_state->state.lha.entry=entry;

	if (entry->dataType)
	{
		/* again a bit hackish. Have the decompression state as part of the main state... */
		file_state->state.lha.decompressState=file_state->archive->state.lha.decompressState;
		archivefs_lhaDecompressInitialize(file_state->state.lha.decompressState,file_state->archive,entry->dataOffset,entry->dataLength,entry->length,entry->dataType);
	}
	return 0;
}

static int32_t archivefs_lha_fileRead(void *dest,struct archivefs_file_state *file_state,uint32_t length,uint32_t offset)
{
	/* does not check file type */
	const struct archivefs_cached_file_entry *entry;
	int ret;

	entry=file_state->state.lha.entry;
	if (length+offset>entry->length)
		return ARCHIVEFS_ERROR_INVALID_READ;
	if (entry->dataType)
	{
		return archivefs_lhaDecompress(file_state->state.lha.decompressState,dest,length,offset);
	} else {
		if ((ret=archivefs_common_read(dest,length,entry->dataOffset+offset,file_state->archive))<0) return ret;
		return length;
	}
}

static int archivefs_lha_uninitialize(struct archivefs_state *archive)
{
	if (archive->state.lha.decompressState)
		archivefs_free(archive->state.lha.decompressState);
	return 0;
}

int archivefs_lha_initialize(struct archivefs_state *archive)
{
	struct archivefs_cached_file_entry *entry;
	int usesCompression=0;
	int32_t offset=0;

	archive->state.lha.decompressState=0;
	archive->fileOpen=archivefs_lha_fileOpen;
	archive->fileRead=archivefs_lha_fileRead;
	archive->uninitialize=archivefs_lha_uninitialize;

	/* there might (should) be a stop character at the end of the file */
	while (offset+1<archive->fileLength)
	{
		offset=archivefs_lha_parse_entry(&entry,archive,offset);
		if (offset<0)
		{
			if (entry) archivefs_free(entry);
			return offset;
		}
		if (entry->dataType) usesCompression=1;
		archivefs_common_insertFileEntry(archive,entry);
	}

	if (usesCompression)
	{
		archive->state.lha.decompressState=archivefs_malloc(sizeof(struct archivefs_lhaDecompressState));
		if (!archive->state.lha.decompressState)
			return ARCHIVEFS_ERROR_MEMORY_ALLOCATION_FAILED;
	}
	return 0;
}
