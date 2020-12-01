/* Copyright (C) Teemu Suutari */

#include "archivefs_lha_decompressor.h"
#include "archivefs_huffman_decoder.h"

/* decompresses LH4 and LH5 files */

/* 31 bits max */
static int32_t archivefs_lhaReadBits(struct archivefs_lhaDecompressState *state,uint8_t bits)
{
	uint32_t value=0;
	int ret;
	uint8_t length;
	struct archivefs_state *archive;

	while (bits)
	{
		if (!state->bitsLeft)
		{
			archive=state->archive;
			if (state->filePos==state->fileLength) return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
			archivefs_common_readNextByte(state->accumulator,archive);
			state->filePos++;
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

static int archivefs_lhaReadBit(struct archivefs_lhaDecompressState *state)
{
	int ret;
	struct archivefs_state *archive;

	if (!state->bitsLeft)
	{
		archive=state->archive;
		if (state->filePos==state->fileLength) return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
		archivefs_common_readNextByte(state->accumulator,archive);
		state->filePos++;
		state->bitsLeft=7U;
	} else state->bitsLeft--;
	ret=(state->accumulator&0x80U)?1U:0;
	state->accumulator<<=1U;
	return ret;
}

static int archivefs_lhaHuffmanDecode(struct archivefs_lhaDecompressState *state,const uint16_t *nodes) archivefs_HuffmanDecode(nodes,archivefs_lhaReadBit(state))

void archivefs_lhaDecompressInitialize(struct archivefs_lhaDecompressState *state,struct archivefs_state *archive,uint32_t fileOffset,uint32_t fileLength,uint32_t rawLength,uint32_t method)
{
	state->archive=archive;
	state->fileOffset=fileOffset;
	state->fileLength=fileLength;
	state->filePos=0;

	state->rawLength=rawLength;
	state->rawPos=0;

	state->accumulator=0;
	state->bitsLeft=0;

	state->method=method;

	state->blockRemaining=0;
	state->remainingRepeat=0;

	state->historyPos=0;
}

static void archivefs_lhaDecompressReset(struct archivefs_lhaDecompressState *state)
{
	state->filePos=0;

	state->rawPos=0;

	state->accumulator=0;
	state->bitsLeft=0;

	state->blockRemaining=0;
	state->remainingRepeat=0;

	state->historyPos=0;
}

static int archivefs_lhaCreateSimpleTable(struct archivefs_lhaDecompressState *state,uint16_t *nodes,uint16_t length,uint8_t bits,int enableHole)
{
	int ret;
	uint8_t bitLengths[19];
	uint8_t i,tableLength;
	uint8_t value;

	tableLength=ret=archivefs_lhaReadBits(state,bits);
	if (ret<0) return ret;
	if (tableLength>length)
		return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
	if (!tableLength)
	{
		value=ret=archivefs_lhaReadBits(state,bits);
		if (ret<0) return ret;
		archivefs_HuffmanCreateEmptyTable(nodes,value);
		return 0;
	}
	for (i=0;i<tableLength;)
	{
		value=ret=archivefs_lhaReadBits(state,3U);
		if (ret<0) return ret;
		if (value==7)
		{
			do
			{
				ret=archivefs_lhaReadBit(state);
				if (ret>0) value++;
			} while (ret>0);
			if (ret<0) return ret;
			if (value>31U)
				return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
		}
		bitLengths[i++]=value;
		if (i==3U && enableHole)
		{
			value=ret=archivefs_lhaReadBits(state,2U);
			if (ret<0) return ret;
			if (i+value>length)
				return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
			while (value--)
				bitLengths[i++]=0;
		}
	}
	return archivefs_HuffmanCreateOrderlyTable(nodes,bitLengths,tableLength);
}

static int32_t archivefs_lhaInitializeBlock(struct archivefs_lhaDecompressState *state)
{
	uint32_t blockRemaining;
	uint16_t i,tableLength,rep;
	int32_t ret;
	uint8_t value;

	/* rather inconvenient, but we have no better place than stack to put this table */
	uint8_t symbols[511];

	blockRemaining=ret=archivefs_lhaReadBits(state,16U);
	if (!blockRemaining) blockRemaining=0x10000U;

	if ((ret=archivefs_lhaCreateSimpleTable(state,state->symbolTree,19U,5U,1))<0) return ret;

	tableLength=ret=archivefs_lhaReadBits(state,9U);
	if (ret<0) return ret;
	if (!tableLength)
	{
		value=ret=archivefs_lhaReadBits(state,9U);
		if (ret<0) return ret;
		archivefs_HuffmanCreateEmptyTable(state->symbolTree,value);
		return 0;
	}
	for (i=0;i<tableLength;)
	{
		value=ret=archivefs_lhaHuffmanDecode(state,state->symbolTree);
		if (ret<0) return ret;
		switch (value)
		{
			case 0:
			rep=1U;
			break;

			case 1U:
			value=0;
			rep=ret=archivefs_lhaReadBits(state,4U)+3U;
			if (ret<0) return ret;
			break;

			case 2U:
			value=0;
			rep=ret=archivefs_lhaReadBits(state,9U)+20U;
			if (ret<0) return ret;
			break;

			default:
			value-=2U;
			rep=1U;
			break;
		}
		if (i+(uint16_t)rep>tableLength)
			return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
		while (rep--)
			symbols[i++]=value;
	}
	if ((ret=archivefs_HuffmanCreateOrderlyTable(state->symbolTree,symbols,tableLength))<0) return ret;
	if ((ret=archivefs_lhaCreateSimpleTable(state,state->distanceTree,14U,4U,0))<0) return ret;
	return blockRemaining;
}

static int32_t archivefs_lhaDecodeDistance(struct archivefs_lhaDecompressState *state,uint16_t mask)
{
	uint16_t distance;
	int32_t ret;
	uint8_t distanceBits;

	distanceBits=ret=archivefs_lhaHuffmanDecode(state,state->distanceTree);
	if (ret<0) return ret;
	if (distanceBits)
	{
		distance=ret=archivefs_lhaReadBits(state,distanceBits-1);
		if (ret<0) return ret;
		distance|=(1U<<(distanceBits-1U));
		distance&=mask;
		distance++;
	} else distance=1;
	return distance;
}

static int archivefs_lhaDecompressFull(struct archivefs_lhaDecompressState *state,uint8_t *dest,uint16_t mask)
{
	uint32_t length=state->rawLength;
	uint32_t pos,spaceLength,blockLength=0;
	uint32_t symbol,distance;
	int32_t ret;

	for (pos=0;pos<length;)
	{
		if (!blockLength)
		{
			blockLength=ret=archivefs_lhaInitializeBlock(state);
			if (ret<0) return ret;
		}
		blockLength--;

		symbol=ret=archivefs_lhaHuffmanDecode(state,state->symbolTree);
		if (ret<0) return ret;
		if (symbol<256U)
		{
			dest[pos++]=symbol;
		} else {
			distance=ret=archivefs_lhaDecodeDistance(state,mask);
			if (ret<0) return ret;

			symbol-=253U;
			if (pos+symbol>length) symbol=length-pos;
			if (pos<distance)
			{
				spaceLength=distance-pos;
				if (spaceLength>symbol) spaceLength=symbol;
				symbol-=spaceLength;
				while (spaceLength--)
					dest[pos++]=0x20U;
			}
			while (symbol--)
			{
				dest[pos]=dest[pos-distance];
				pos++;
			}
		}
	}
	archivefs_lhaDecompressReset(state);
	return 0;
}

static int archivefs_lhaDecompressSkip(struct archivefs_lhaDecompressState *state,uint32_t targetLength,uint16_t mask)
{
#define ARCHIVEFS_LHA_DECOMPRESS_SKIP 1
#include "archivefs_lha_decompressor_template.h"
}

static int archivefs_lhaDecompressPartial(struct archivefs_lhaDecompressState *state,uint8_t *dest,uint32_t targetLength,uint16_t mask)
{
#undef ARCHIVEFS_LHA_DECOMPRESS_SKIP
#include "archivefs_lha_decompressor_template.h"
}

int32_t archivefs_lhaDecompress(struct archivefs_lhaDecompressState *state,uint8_t *dest,uint32_t length,uint32_t offset)
{
	uint16_t mask=(state->method==4)?0xfffU:0x1fffU;
	int ret;

	if (offset<state->rawPos)
		archivefs_lhaDecompressReset(state);
	archivefs_common_initBlockBuffer(state->fileOffset+state->filePos,state->archive);
	if (offset!=state->rawPos)
	{
		if ((ret=archivefs_lhaDecompressSkip(state,offset,mask))<0)
		{
			archivefs_lhaDecompressReset(state);
			return ret;
		}
	}
	if (!offset && length==state->rawLength)
	{
		if ((ret=archivefs_lhaDecompressFull(state,dest,mask))<0)
		{
			archivefs_lhaDecompressReset(state);
			return ret;
		}
	} else {
		if ((ret=archivefs_lhaDecompressPartial(state,dest,offset+length,mask))<0)
		{
			archivefs_lhaDecompressReset(state);
			return ret;
		}
	}
	return length;
}
