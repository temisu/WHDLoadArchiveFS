/* Copyright (C) Teemu Suutari */

#include "archivefs_zip_decompressor.h"
#include "archivefs_huffman_decoder.h"

/* decompresses Deflate */

#if 0
static int archivefs_zipReadNextBlock(struct archivefs_zipDecompressState *state)
{
	uint32_t length;
	int ret;
	
	length=state->fileLength-state->filePos;
	if (!length)
		return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
	if (length>ARCHIVEFS_INPUT_BUFFER_LENGTH)
		length=ARCHIVEFS_INPUT_BUFFER_LENGTH;
	if ((ret=archivefs_common_simpleRead(state->inputBuffer,length,state->fileOffset+state->filePos,state->archive))<0) return ret;
	state->filePos+=length;
	state->inputBufferLength=length;
	state->inputBufferPos=0;
	return 0;
}

/* 31 bits max */
static int32_t archivefs_zipReadBits(struct archivefs_zipDecompressState *state,uint8_t bits)
{
	uint32_t value=0;
	int ret;
	uint8_t length;

	while (bits)
	{
		if (!state->bitsLeft)
		{
			if (state->inputBufferPos==state->inputBufferLength)
				if ((ret=archivefs_zipReadNextBlock(state))<0) return ret;
			state->accumulator=state->inputBuffer[state->inputBufferPos++];
			state->bitsLeft=8U;
		}
		length=state->bitsLeft;
		if (length>bits) length=bits;
		value=(value<<length)|(state->accumulator>>(8U-length));
		state->accumulator<<=length;
		state->bitsLeft-=length;
		bits-=length;
	}
	return value;
}

static int archivefs_zipReadBit(struct archivefs_zipDecompressState *state)
{
	int ret;
	if (!state->bitsLeft)
	{
		if (state->inputBufferPos==state->inputBufferLength)
			if ((ret=archivefs_zipReadNextBlock(state))<0) return ret;
		state->accumulator=state->inputBuffer[state->inputBufferPos++];
		state->bitsLeft=7U;
	} else state->bitsLeft--;
	ret=(state->accumulator&0x80U)?1U:0;
	state->accumulator<<=1U;
	return ret;
}

static int archivefs_zipHuffmanDecode(struct archivefs_zipDecompressState *state,const uint16_t *nodes) archivefs_HuffmanDecode(nodes,archivefs_zipReadBit(state))
#endif

void archivefs_zipDecompressInitialize(struct archivefs_zipDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength)
{
	state->archive=archive;
	state->fileOffset=fileOffset;
	state->fileLength=fileLength;
	state->filePos=0;

	state->rawLength=rawLength;
	state->rawPos=0;

	state->accumulator=0;
	state->bitsLeft=0;

	state->blockRemaining=0;
	state->remainingRepeat=0;

	state->historyPos=0;
}

static void archivefs_zipDecompressReset(struct archivefs_zipDecompressState *state)
{
	state->filePos=0;

	state->rawPos=0;

	state->accumulator=0;
	state->bitsLeft=0;

	state->blockRemaining=0;
	state->remainingRepeat=0;

	state->historyPos=0;
}

static int archivefs_zipDecompressFull(struct archivefs_zipDecompressState *state,uint8_t *dest)
{
	return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
}

static int archivefs_zipDecompressSkip(struct archivefs_zipDecompressState *state,uint32_t targetLength)
{
	return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
}

static int archivefs_zipDecompressPartial(struct archivefs_zipDecompressState *state,uint8_t *dest,uint32_t targetLength)
{
	return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
}

int32_t archivefs_zipDecompress(struct archivefs_zipDecompressState *state,uint8_t *dest,uint32_t length,uint32_t offset)
{
	int ret;

	if (offset<state->rawPos)
		archivefs_zipDecompressReset(state);
	if (offset!=state->rawPos)
	{
		if ((ret=archivefs_zipDecompressSkip(state,offset))<0)
		{
			archivefs_zipDecompressReset(state);
			return ret;
		}
	}
	if (!offset && length==state->rawLength)
	{
		if ((ret=archivefs_zipDecompressFull(state,dest))<0)
		{
			archivefs_zipDecompressReset(state);
			return ret;
		}
	} else {
		if ((ret=archivefs_zipDecompressPartial(state,dest,offset+length))<0)
		{
			archivefs_zipDecompressReset(state);
			return ret;
		}
	}
	return length;
}
