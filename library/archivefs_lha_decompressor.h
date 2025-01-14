/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_LHA_DECOMPRESSOR_H
#define ARCHIVEFS_LHA_DECOMPRESSOR_H

#include "archivefs_common.h"

#define ARCHIVEFS_LHA_LH5_HISTORY_SIZE (8192)
#define ARCHIVEFS_LHA_LH6_HISTORY_SIZE (32768)

struct archivefs_lhaDecompressState
{
	struct archivefs_state *archive;

	uint32_t	fileOffset;
	uint32_t	fileLength;
	uint32_t	filePos;

	uint32_t	rawLength;
	uint32_t	rawPos;

	uint16_t	accumulator;
	uint8_t		bitsLeft;

	/*
	   lh4: 4095 bytes of history
	   lh5: 8191 bytes of history
	   lh4/lh5 have:
	   * Optional Huffman Symbol decoder of max 511 entries
	   * Optional Huffman Distance decoder of max 14 entries
	   lh6 has:
	   * Optional Huffman Distance decoder of max 16 entries
	*/
	uint32_t	method;		/* 4 = LH4, 5 = LH5, 6 = LH6 */

	uint16_t	blockRemaining;
	uint16_t	remainingRepeat;
	uint16_t	distance;

	uint16_t	historyPos;

	/* worst case */
	uint16_t	symbolTree[511*2+1];
	uint16_t	distanceTree[16*2+1];

	uint8_t		history[ARCHIVEFS_LHA_LH5_HISTORY_SIZE];
};

struct archivefs_lhaDecompressState *archivefs_lhaAllocateDecompressState(int hasLH1,int hasLH45,int hasLH6);
void archivefs_lhaDecompressInitialize(struct archivefs_lhaDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength,uint32_t method);
int32_t archivefs_lhaDecompress(struct archivefs_lhaDecompressState *state,uint8_t *dest,int32_t length,uint32_t offset);

#endif
