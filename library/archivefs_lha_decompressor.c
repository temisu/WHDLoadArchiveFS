/* Copyright (C) Teemu Suutari */

#include "archivefs_lha_decompressor.h"
#include "archivefs_huffman_decoder.h"
#include "archivefs_integration.h"

/* decompresses LH4, LH5 and LH6 files */

#define archivefs_lhaReadBits(value,bits) \
	do { \
		uint8_t _bits=(bits); \
		while (_bits) \
		{ \
			if (bitsLeft<_bits) \
			{ \
				(value)=((value)<<bitsLeft)|(accumulator>>(16U-bitsLeft)); \
				archivefs_common_readNextWord(accumulator,archive); \
				_bits-=bitsLeft; \
				bitsLeft=16U; \
			} else { \
				(value)=((value)<<_bits)|(accumulator>>(16U-_bits)); \
				accumulator<<=_bits; \
				bitsLeft-=_bits; \
				break; \
			} \
		} \
	} while (0)

#define archivefs_lhaReadBit(incr) \
	do { \
		if (!bitsLeft) \
		{ \
			archivefs_common_readNextWord(accumulator,archive); \
			bitsLeft=15U; \
		} else bitsLeft--; \
		if ((int16_t)(accumulator)<0) incr+=2; \
		accumulator+=accumulator; \
	} while (0)

#define archivefs_lhaHuffmanDecode(symbol,nodes) archivefs_HuffmanDecode(symbol,nodes,archivefs_lhaReadBit)

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
	uint16_t accumulator=state->accumulator;
	uint8_t bitsLeft=state->bitsLeft;
	struct archivefs_state *archive;

	archive=state->archive;
	tableLength=0;
	archivefs_lhaReadBits(tableLength,bits);
	if (tableLength>length)
		return WVFS_ERROR_DECOMPRESSION_ERROR;
	if (!tableLength)
	{
		value=0;
		archivefs_lhaReadBits(value,bits);
		archivefs_HuffmanCreateEmptyTable(nodes,value);
		state->accumulator=accumulator;
		state->bitsLeft=bitsLeft;
		return 0;
	}
	for (i=0;i<tableLength;)
	{
		value=0;
		archivefs_lhaReadBits(value,3U);
		if (value==7)
		{
			uint16_t tmp;
			do
			{
				tmp=0;
				archivefs_lhaReadBit(tmp);
				if (tmp) value++;
			} while (tmp);
			if (value>32U)
				return WVFS_ERROR_DECOMPRESSION_ERROR;
		}
		bitLengths[i++]=value;
		if (i==3U && enableHole)
		{
			value=0;
			archivefs_lhaReadBits(value,2U);
			if (i+value>length)
				return WVFS_ERROR_DECOMPRESSION_ERROR;
			while (value--)
				bitLengths[i++]=0;
		}
	}
	state->accumulator=accumulator;
	state->bitsLeft=bitsLeft;
	return archivefs_HuffmanCreateOrderlyTable(nodes,bitLengths,tableLength);
}

static int32_t archivefs_lhaInitializeBlock(struct archivefs_lhaDecompressState *state)
{
	int32_t blockRemaining;
	uint16_t i,tableLength,rep;
	int32_t ret;
	uint16_t value;
	uint16_t accumulator=state->accumulator;
	uint8_t bitsLeft=state->bitsLeft;
	struct archivefs_state *archive;

	/* rather inconvenient, but we have no better place than stack to put this table */
	uint8_t symbols[511];

	archive=state->archive;
	blockRemaining=0;
	archivefs_lhaReadBits(blockRemaining,16U);
	if (!blockRemaining) blockRemaining=0x10000U;

	state->accumulator=accumulator;
	state->bitsLeft=bitsLeft;
	if ((ret=archivefs_lhaCreateSimpleTable(state,state->symbolTree,19U,5U,1))<0) return ret;
	accumulator=state->accumulator;
	bitsLeft=state->bitsLeft;

	tableLength=0;
	archivefs_lhaReadBits(tableLength,9U);
	if (!tableLength)
	{
		value=0;
		archivefs_lhaReadBits(value,9U);
		archivefs_HuffmanCreateEmptyTable(state->symbolTree,value);
		state->accumulator=accumulator;
		state->bitsLeft=bitsLeft;
		return 0;
	}
	for (i=0;i<tableLength;)
	{
		archivefs_lhaHuffmanDecode(value,state->symbolTree);
		switch (value)
		{
			case 0:
			rep=1U;
			break;

			case 1U:
			value=0;
			rep=0;
			archivefs_lhaReadBits(rep,4U);
			rep+=3U;
			break;

			case 2U:
			value=0;
			rep=0;
			archivefs_lhaReadBits(rep,9U);
			rep+=20U;
			break;

			default:
			value-=2U;
			rep=1U;
			break;
		}
		if (i+rep>tableLength)
			return WVFS_ERROR_DECOMPRESSION_ERROR;
		while (rep--)
			symbols[i++]=value;
	}
	if ((ret=archivefs_HuffmanCreateOrderlyTable(state->symbolTree,symbols,tableLength))<0) return ret;
	state->accumulator=accumulator;
	state->bitsLeft=bitsLeft;
	if ((ret=archivefs_lhaCreateSimpleTable(state,state->distanceTree,state->method==6U?16U:14U,state->method==6U?5U:4U,0))<0) return ret;
	return blockRemaining;
}

#define archivefs_lhaDecodeDistance(distance,state) \
	do { \
		uint16_t distanceBits; \
		archivefs_lhaHuffmanDecode(distanceBits,state->distanceTree); \
		if (distanceBits) \
		{ \
			distance=1U; \
			archivefs_lhaReadBits(distance,distanceBits-1); \
			distance++; \
		} else distance=1; \
	} while (0)

static int archivefs_lhaDecompressFull(struct archivefs_lhaDecompressState *state,uint8_t *dest)
{
	uint32_t length=state->rawLength;
	uint32_t pos,blockLength=0;
	uint16_t symbol,distance;
	uint16_t accumulator=state->accumulator;
	uint8_t bitsLeft=state->bitsLeft;
	int32_t ret;
	struct archivefs_state *archive;

	archive=state->archive;
	for (pos=0;pos<length;)
	{
		if (!blockLength)
		{
			state->accumulator=accumulator;
			state->bitsLeft=bitsLeft;
			blockLength=ret=archivefs_lhaInitializeBlock(state);
			if (ret<0) return ret;
			accumulator=state->accumulator;
			bitsLeft=state->bitsLeft;
		}
		blockLength--;

		archivefs_lhaHuffmanDecode(symbol,state->symbolTree);
		if (symbol<256U)
		{
			dest[pos++]=symbol;
		} else {
			archivefs_lhaDecodeDistance(distance,state);

			symbol-=253U;
			if (pos+symbol>length) symbol=length-pos;
			if (pos<distance)
			{
				uint32_t spaceLength;

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

int32_t archivefs_lhaDecompress(struct archivefs_lhaDecompressState *state,uint8_t *dest,int32_t length,uint32_t offset)
{
	uint16_t mask=state->method==6U?ARCHIVEFS_LHA_LH6_HISTORY_SIZE-1:(ARCHIVEFS_LHA_LH5_HISTORY_SIZE-1);
	int ret;

	if (offset<state->rawPos)
		archivefs_lhaDecompressReset(state);
	archivefs_common_initBlockBuffer(state->fileOffset+state->filePos,state->archive);
	if (!state->filePos && (state->fileOffset&1U))
	{
		struct archivefs_state *archive;

		archive=state->archive;
		archivefs_common_readNextByte(state->accumulator,archive);
		state->bitsLeft=8U;
		state->accumulator<<=8U;
	}
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
		if ((ret=archivefs_lhaDecompressFull(state,dest))<0)
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

struct archivefs_lhaDecompressState *archivefs_lhaAllocateDecompressState(int hasLH1,int hasLH45,int hasLH6)
{
	uint32_t length;

	/* no LH1 support as of yet */
	length=sizeof(struct archivefs_lhaDecompressState);
	if (hasLH6)
		length+=ARCHIVEFS_LHA_LH6_HISTORY_SIZE-ARCHIVEFS_LHA_LH5_HISTORY_SIZE;
	return archivefs_malloc(length);
}
