/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_ZIP_DECOMPRESSOR_H
#define ARCHIVEFS_ZIP_DECOMPRESSOR_H

#include "archivefs_common.h"

struct archivefs_zipDecompressState
{
	struct archivefs_state *archive;

	uint32_t	fileOffset;
	uint32_t	fileLength;
	uint32_t	filePos;

	uint32_t	rawLength;
	uint32_t	rawPos;

	uint8_t		accumulator;
	uint8_t		bitsLeft;

	uint32_t	blockRemaining;
	uint16_t	remainingRepeat;
	uint16_t	distance;

	uint16_t	historyPos;

	/* round upwards intentionally. Nodes are booked with pairs so rounding down would be misguided here */
	uint16_t	symbolTree[286*2];
	uint16_t	distanceTree[32*2];

	uint8_t		history[32768];
};

void archivefs_zipDecompressInitialize(struct archivefs_zipDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength);
int32_t archivefs_zipDecompress(struct archivefs_zipDecompressState *state,uint8_t *dest,uint32_t length,uint32_t offset);

#endif
