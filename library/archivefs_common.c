/* Copyright (C) Teemu Suutari */

#include "archivefs_private.h"
#include "archivefs_integration.h"

static int32_t archivefs_common_readPartial(void *dest,uint32_t length,uint32_t offset,struct archivefs_state *archive)
{
	int ret;

	if (offset!=archive->filePos)
	{
		if ((ret=archivefs_integration_fileSeek(offset,archive->file))<0) return ret;
		archive->filePos=offset;
	}

	ret=archivefs_integration_fileRead(dest,length,archive->file);
	if (ret<0) return ret;
	archive->filePos+=ret;
	if (ret!=length) return ARCHIVEFS_ERROR_INVALID_FORMAT;
	return length;
}

static int32_t archivefs_common_readBlockRaw(void *dest,uint32_t blockIndex,struct archivefs_state *archive)
{
	uint32_t offset=blockIndex<<archive->blockShift;
	uint32_t blockSize=1U<<archive->blockShift;
	uint32_t length=archive->fileLength-offset;

	if (length>blockSize) length=blockSize;
	return archivefs_common_readPartial(dest,length,offset,archive);
}

uint32_t archivefs_common_getTotalBlocks(uint32_t offset,uint32_t length,struct archivefs_state *archive)
{
	uint32_t blockMask=(1U<<archive->blockShift)-1U;
	uint32_t count=0;

	if (!length) return 0;

	offset&=blockMask;
	if (offset)
	{
		count++;
		if (length<(blockMask+1U-offset)) return count;
		length-=blockMask+1U-offset;
	}
	if (length&blockMask) count++;
	count+=length>>archive->blockShift;
	return count;
}

void archivefs_common_handleProgress(struct archivefs_state *archive)
{
	if (archive->progressEnabled)
	{
		uint32_t pos=++archive->currentProgress;
		if (archive->progressFunc)
		{
			if (pos>archive->totalBlocks) pos=archive->totalBlocks;
			archive->progressFunc(pos,archive->totalBlocks);
		}
	}
}

int archivefs_common_readBlockBuffer(uint32_t blockIndex,struct archivefs_state *archive)
{
	int32_t ret;

	archivefs_common_handleProgress(archive);
	if (blockIndex==archive->blockIndex && ((int32_t)(archive->blockLength)>0))
	{
		archive->blockPos=0;
		return 0;
	}
	ret=archivefs_common_readBlockRaw(archive->blockData,blockIndex,archive);
	if (ret>=0)
	{
		archive->blockIndex=blockIndex;
		if (((int32_t)archive->blockPos)<0)
		{
			archive->blockPos=~archive->blockPos;
		} else {
			archive->blockPos=0;
		}
		archive->blockLength=ret;
		if (archive->blockPos>archive->blockLength)
		{
			archive->blockPos=0;
			archive->blockLength=0;
			return ARCHIVEFS_ERROR_INVALID_FORMAT;
		}
		return 0;
	} else {
		archive->blockPos=0;
		archive->blockLength=0;
		return ret;
	}
}

int archivefs_common_initBlockBuffer(uint32_t offset,struct archivefs_state *archive)
{
	uint32_t blockIndex=offset>>archive->blockShift;

	if (offset>=archive->fileLength)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
	archive->blockPos=offset&((1U<<archive->blockShift)-1U);
	if (blockIndex!=archive->blockIndex && ((int32_t)(archive->blockLength)>0))
	{
		archive->blockPos=~archive->blockPos;
		archive->blockLength=archive->blockPos;
		archive->blockIndex=blockIndex-1;
	}
	return 0;
}

int32_t archivefs_common_readNextBytes(uint8_t **dest,uint32_t length,struct archivefs_state *archive)
{
	uint32_t max;
	int32_t ret;

	if (archive->blockPos==archive->blockLength)
		if ((ret=archivefs_common_readBlockBuffer(archive->blockIndex+1,archive))<0) return ret;
	max=archive->blockLength-archive->blockPos;
	if (length>max) length=max;
	*dest=archive->blockData+archive->blockPos;
	archive->blockPos+=length;
	return length;
}

int archivefs_common_skipNextBytes(uint32_t length,struct archivefs_state *archive)
{
	uint32_t max;
	uint32_t blockSize=1U<<archive->blockShift;

	max=archive->blockLength-archive->blockPos;
	/* TODO: error handling */
	if (length>max)
	{
		length-=max;
		archive->blockIndex+=length>>archive->blockShift;
		archive->blockPos=~(length&(blockSize-1U));
		archive->blockLength=archive->blockPos;
		return 0;
	} else {
		archive->blockPos+=length;
		return 0;
	}
}

/* TODO: make it proper, or use library */
void archivefs_common_memcpy(void *dest,const void *src,uint32_t length)
{
	uint8_t *_dest=dest;
	const uint8_t *_src=src;
	while (length--) *(_dest++)=*(_src++);
}

static int32_t archivefs_common_copyBlock(uint8_t *dest,uint32_t startBlock,uint32_t maxBlock,uint32_t currentBufferBlock,uint32_t length,uint32_t offset,struct archivefs_state *archive)
{
	uint32_t currentBlockLength,copyOffset;
	uint8_t blockShift=archive->blockShift;
	uint32_t blockSize=1U<<blockShift;

	/* we are interested about the tail length of the last buffer (if applicable) */
	if (currentBufferBlock==startBlock)
	{
		currentBlockLength=(offset&(blockSize-1U))+length;
		if (currentBlockLength>blockSize) currentBlockLength=blockSize;
		copyOffset=offset&(blockSize-1U);
	} else if (currentBufferBlock==maxBlock-1U) {
		currentBlockLength=(offset+length)&(blockSize-1U);
		if (!currentBlockLength) currentBlockLength=blockSize;
		copyOffset=0;
	} else {
		currentBlockLength=blockSize;
		copyOffset=0;
	}
	if (currentBlockLength>archive->blockLength)
		return ARCHIVEFS_ERROR_INVALID_FORMAT;
	archivefs_common_memcpy(dest,archive->blockData+copyOffset,currentBlockLength-copyOffset);
	return currentBlockLength-copyOffset;
}

int archivefs_common_read(void *dest,uint32_t length,uint32_t offset,struct archivefs_state *archive)
{
	int ret;
	uint32_t i,blockSize,startBlock,maxBlock,currentBufferBlock,newBufferBlock,currentBlockLength;
	uint32_t destOffset,destCopied;
	uint8_t blockShift;

	if (!length) return 0;

	blockShift=archive->blockShift;
	blockSize=1U<<blockShift;
	startBlock=offset>>blockShift;
	maxBlock=(offset+length+blockSize-1U)>>blockShift;
	currentBufferBlock=archive->blockIndex;
	/* We could make buffering logic better here for the choice of the buffered block... */
	newBufferBlock=maxBlock-1;

	if (startBlock<=currentBufferBlock && currentBufferBlock<maxBlock)
	{
		if (currentBufferBlock==startBlock) destOffset=0;
			else destOffset=((blockSize-offset)&(blockSize-1U))+((currentBufferBlock-startBlock-1U)<<blockShift);
		destCopied=ret=archivefs_common_copyBlock((uint8_t*)dest+destOffset,startBlock,maxBlock,currentBufferBlock,length,offset,archive);
		if (ret<0) return ret;
	} else destCopied=0;

	for (i=startBlock;i<maxBlock;i++)
	{
		if (i==currentBufferBlock)
		{
			archivefs_common_handleProgress(archive);
			dest=(uint8_t*)dest+destCopied;
			continue;
		}
		if (i==newBufferBlock)
		{
			if ((ret=archivefs_common_readBlockBuffer(i,archive))<0) return ret;
			ret=archivefs_common_copyBlock(dest,startBlock,maxBlock,i,length,offset,archive);
			if (ret<0) return ret;
			dest=(uint8_t*)dest+ret;
		} else {
			archivefs_common_handleProgress(archive);
			if (i==startBlock)
			{
				/* possible partial read */
				currentBlockLength=(offset&(blockSize-1U))+length;
				if (currentBlockLength>blockSize) currentBlockLength=blockSize;
				currentBlockLength-=offset&(blockSize-1U);
				if ((ret=archivefs_common_readPartial(dest,currentBlockLength,offset,archive))<0) return ret;
				dest=(uint8_t*)dest+currentBlockLength;
			} else if (i==maxBlock-1U) {
				/* another possible partial read */
				currentBlockLength=(offset+length)&(blockSize-1U);
				if (!currentBlockLength) currentBlockLength=blockSize;
				if ((ret=archivefs_common_readPartial(dest,currentBlockLength,((blockSize-offset)&(blockSize-1U))+((i-startBlock-1U)<<blockShift),archive))<0) return ret;
			} else {
				/* the most common case. In middle of the file, throw away block directly to dest buffer */
				if ((ret=archivefs_common_readBlockRaw(dest,i,archive))<0) return ret;
				dest=(uint8_t*)dest+blockSize;
			}
		}
	}
	return 0;
}

static int archivefs_isLeapYear(uint32_t year)
{
	return (!(year&3U)&&(year%100)?1:!(year%400));
}

static const uint16_t archivefs_daysPerMonthAccum[12]={0,31,59,90,120,151,181,212,243,273,304,334};

uint32_t archivefs_common_unixTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince)
{
	uint32_t daysSinceEpoch;
	/* epoch switch from 1970.1.1 to 1978.1.1 */
	timestamp-=252460800U;

	daysSinceEpoch=timestamp/86400U;
	timestamp%=86400U;
	*minutesSince=timestamp/60U;
	timestamp%=60U;
	*ticksSince=timestamp*50U;
	return daysSinceEpoch;
}

uint32_t archivefs_common_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince)
{
	uint32_t seconds,minutes,hours,day,month,year;
	uint32_t daysSinceEpoch;

	seconds=(timestamp&31)<<1;
	minutes=(timestamp>>5)&63;
	hours=(timestamp>>11)&31;
	day=(timestamp>>16)&31;
	month=(timestamp>>21)&15;
	year=((timestamp>>25)&0x7f)+1980;

	if (seconds>=60U||minutes>=60U||hours>=60U||!day||day>31||!month||month>12)
	{
		*minutesSince=0;
		*ticksSince=0;
		return 0;
	}
#if 0
	printf("time %u.%u.%u %u:%u:%u\n",year,month,day,hours,minutes,seconds);
#endif
	/* days since 1978.1.1 */
	daysSinceEpoch=day-1+archivefs_daysPerMonthAccum[month-1];
	if (month>2&&archivefs_isLeapYear(year)) daysSinceEpoch++;
	/* this is slow. could be optimized, but fortunately does not really matter since the max is 127+2+1980... */
	daysSinceEpoch+=(year-1978)*365;
	year=(year+3)&~3U;
	while (year>1980)
	{
		year-=4;
		if (archivefs_isLeapYear(year))
			daysSinceEpoch++;
	}
	*minutesSince=hours*60+minutes;
	*ticksSince=seconds*50;

	return daysSinceEpoch;
}

void archivefs_common_insertFileEntry(struct archivefs_state *archive,struct archivefs_cached_file_entry *entry)
{
	if (entry->fileType==ARCHIVEFS_TYPE_FILE)
		archive->totalBlocks+=archivefs_common_getTotalBlocks(entry->dataOffset,entry->dataLength,archive);

	if (!archive->lastEntry)
	{
		archive->firstEntry=entry;
		archive->lastEntry=entry;
	} else {
		archive->lastEntry->next=entry;
		entry->prev=archive->lastEntry;
		archive->lastEntry=entry;
	}
}
