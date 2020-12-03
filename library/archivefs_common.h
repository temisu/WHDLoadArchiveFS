/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_COMMON_H
#define ARCHIVEFS_COMMON_H

#include "archivefs_private.h"

#define GET_LE32(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8)|(((uint16_t)((x)[2]))<<16)|(((uint16_t)((x)[3]))<<24))
#define GET_LE16(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8))

#ifdef __M68K__
#define GET_BE16(x) (*(uint16_t*)(x))
#else
#define GET_BE16(x) (((uint16_t)((x)[1]))|(((uint16_t)((x)[0]))<<8))
#endif

#define archivefs_common_readNextByte(ch,archive) \
	do { \
		if ((archive)->blockPos==(archive)->blockLength) \
			if ((ret=archivefs_common_readBlockBuffer((archive)->blockIndex+1,(archive)))<0) return ret; \
		(ch)=archive->blockData[archive->blockPos++]; \
	} while (0)


#define archivefs_common_readNextWord(ch,archive) \
	do { \
		if ((archive)->blockPos==(archive)->blockLength) \
			if ((ret=archivefs_common_readBlockBuffer((archive)->blockIndex+1,(archive)))<0) return ret; \
		(ch)=GET_BE16(archive->blockData+archive->blockPos); \
		archive->blockPos+=2; \
	} while (0)

int archivefs_common_initBlockBuffer(uint32_t offset,struct archivefs_state *archive);
int archivefs_common_readBlockBuffer(uint32_t blockIndex,struct archivefs_state *archive);
int archivefs_common_read(void *dest,uint32_t length,uint32_t offset,struct archivefs_state *archive);

uint32_t archivefs_common_unixTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince);
uint32_t archivefs_common_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince);
void archivefs_common_insertFileEntry(struct archivefs_state *archive,struct archivefs_cached_file_entry *entry);

#endif
