/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_LHA_DECOMPRESSOR_H
#define ARCHIVEFS_LHA_DECOMPRESSOR_H

#include "archivefs_common.h"

struct archivefs_lhaDecompressState
{
	struct archivefs_state *archive;

	uint32_t	fileOffset;
	uint32_t	fileLength;
	uint32_t	filePos;

	uint32_t	rawLength;
	uint32_t	rawPos;

	uint16_t	inputBufferLength;
	uint16_t	inputBufferPos;
	uint8_t		accumulator;
	uint8_t		bitsLeft;

	/*
	   lh4: 4095 bytes of history
	   lh5: 8191 bytes of history
	   Both have:
	   * Optional Huffman Symbol decoder of max 511 entries
	   * Optional Huffman Distance decoder of max 14 entries
	*/
	uint32_t	method;		/* 4 = LH4, 5 = LH5 */

	uint32_t	blockRemaining;
	uint16_t	remainingRepeat;
	uint16_t	distance;

	uint16_t	historyPos;

	/* round upwards intentionally. Nodes are booked with pairs so rounding down would be misguided here */
	uint16_t	symbolTree[512*2];
	uint16_t	distanceTree[16*2];

	uint8_t		history[8192];
	uint8_t		inputBuffer[ARCHIVEFS_INPUT_BUFFER_LENGTH];
};

extern void archivefs_lhaDecompressInitialize(struct archivefs_lhaDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength,uint32_t method);
extern int32_t archivefs_lhaDecompress(struct archivefs_lhaDecompressState *state,uint8_t *dest,uint32_t length,uint32_t offset);

#endif