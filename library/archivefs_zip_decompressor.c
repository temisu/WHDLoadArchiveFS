/* Copyright (C) Teemu Suutari */

#include "archivefs_zip_decompressor.h"
#include "archivefs_huffman_decoder.h"

/* decompresses Deflate */

#define archivefs_zipReadBits(value,bits) \
	do { \
		uint8_t _bits=(bits),bitPos=0; \
		value=0; \
		while (_bits) \
		{ \
			if (bitsLeft<_bits) \
			{ \
				(value)|=((uint16_t)(accumulator&((1U<<bitsLeft)-1U)))<<bitPos; \
				archivefs_common_readNextByte(accumulator,archive); \
				bitPos+=bitsLeft; \
				_bits-=bitsLeft; \
				bitsLeft=8U; \
			} else { \
				(value)|=((uint16_t)(accumulator&((1U<<_bits)-1U)))<<bitPos; \
				accumulator>>=_bits; \
				bitsLeft-=_bits; \
				break; \
			} \
		} \
	} while (0)

#define archivefs_zipReadBit(incr) \
	do { \
		if (!bitsLeft) \
		{ \
			archivefs_common_readNextByte(accumulator,archive); \
			bitsLeft=7U; \
		} else bitsLeft--; \
		if (accumulator&1U) incr+=2; \
		accumulator>>=1U; \
	} while (0)

#define archivefs_zipHuffmanDecode(symbol,nodes) archivefs_HuffmanDecode(symbol,nodes,archivefs_zipReadBit)

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

	state->mode=0;
	state->final=0;

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

	state->mode=0;
	state->final=0;

	state->blockRemaining=0;
	state->remainingRepeat=0;

	state->historyPos=0;
}

static const uint8_t archivefs_zipLengthTableOrder[19]={
	16,17,18, 0, 8, 7, 9, 6,
	10, 5,11, 4,12, 3,13, 2,
	14, 1,15};

static int32_t archivefs_zipInitializeBlock(struct archivefs_zipDecompressState *state)
{
	uint16_t i,nodeCount,mode;
	uint8_t bitsLeft=state->bitsLeft,accumulator=state->accumulator;
	int ret;
	struct archivefs_state *archive;

	archive=state->archive;
	archivefs_zipReadBits(mode,2U);
	state->mode=mode;
	switch (mode)
	{
		case 0:
		{
			uint16_t len,nlen,tmp;

			archivefs_common_readNextByte(tmp,archive);
			len=tmp;
			archivefs_common_readNextByte(tmp,archive);
			len|=((uint16_t)tmp)<<8U;
			archivefs_common_readNextByte(tmp,archive);
			nlen=tmp;
			archivefs_common_readNextByte(tmp,archive);
			nlen|=((uint16_t)tmp)<<8U;
			nlen=~nlen;
			if (len!=nlen)
				return WVFS_ERROR_DECOMPRESSION_ERROR;
			state->bitsLeft=0;
			return len;
		}

		case 1:
		archivefs_HuffmanReset(state->symbolTree);
		nodeCount=1;
		/* Yup! There are 288 codes total even though the last 2 ones are not defined in any spec */
		for (i=0;i<0x18U;i++) nodeCount=archivefs_HuffmanInsert(state->symbolTree,nodeCount,7U,i,i+0x100U);
		for (i=0x30U;i<0xc0U;i++) nodeCount=archivefs_HuffmanInsert(state->symbolTree,nodeCount,8U,i,i-0x30U);
		for (i=0xc0U;i<0xc6U;i++) nodeCount=archivefs_HuffmanInsert(state->symbolTree,nodeCount,8U,i,i+0x58U);
		for (i=0xc6U;i<0xc8U;i++) nodeCount=archivefs_HuffmanInsert(state->symbolTree,nodeCount,8U,i,0x11dU);
		for (i=0x190U;i<0x200U;i++) nodeCount=archivefs_HuffmanInsert(state->symbolTree,nodeCount,9U,i,i-0x100U);

		archivefs_HuffmanReset(state->distanceTree);
		nodeCount=1;
		/* Ditto: there are 32 codes, even though 30 is used */
		for (i=0;i<0x1eU;i++) nodeCount=archivefs_HuffmanInsert(state->distanceTree,nodeCount,5U,i,i);
		for (i=0x1e;i<0x20U;i++) nodeCount=archivefs_HuffmanInsert(state->distanceTree,nodeCount,5U,i,29);
		state->bitsLeft=bitsLeft;
		state->accumulator=accumulator;
		return 1;

		case 2:
		{
			uint16_t symbolCount,tableLength,distanceCount;
			uint8_t lengthTable[286+32];

			archivefs_zipReadBits(symbolCount,5U);
			symbolCount+=257U;
			if (symbolCount>=287U) return WVFS_ERROR_DECOMPRESSION_ERROR;
			archivefs_zipReadBits(distanceCount,5U);
			distanceCount++;
			archivefs_zipReadBits(tableLength,4U);
			tableLength+=4U;

			for (i=0;i<19U;i++) lengthTable[i]=0;
			for (i=0;i<tableLength;i++)
			{
				uint8_t tmp;

				archivefs_zipReadBits(tmp,3U);
				lengthTable[archivefs_zipLengthTableOrder[i]]=tmp;
			}
			if ((ret=archivefs_HuffmanCreateOrderlyTable(state->symbolTree,lengthTable,19U))<0) return ret;

			for (i=0;i<symbolCount+distanceCount;)
			{
				uint16_t tmp;

				archivefs_zipHuffmanDecode(tmp,state->symbolTree);
				switch (tmp)
				{
					case 16:
					if (!i) return WVFS_ERROR_DECOMPRESSION_ERROR;
					archivefs_zipReadBits(tmp,2U);
					tmp+=i+3U;
					if (tmp>symbolCount+distanceCount)
						return WVFS_ERROR_DECOMPRESSION_ERROR;
					for (;i<tmp;i++)
						lengthTable[i]=lengthTable[i-1];
					break;

					case 17:
					archivefs_zipReadBits(tmp,3U);
					tmp+=i+3U;
					if (tmp>symbolCount+distanceCount)
						return WVFS_ERROR_DECOMPRESSION_ERROR;
					for (;i<tmp;i++)
						lengthTable[i]=0;
					break;

					case 18:
					archivefs_zipReadBits(tmp,7U);
					tmp+=i+11U;
					if (tmp>symbolCount+distanceCount)
						return WVFS_ERROR_DECOMPRESSION_ERROR;
					for (;i<tmp;i++)
						lengthTable[i]=0;
					break;

					default:
					lengthTable[i++]=tmp;
					break;
				}
			}
			if ((ret=archivefs_HuffmanCreateOrderlyTable(state->symbolTree,lengthTable,symbolCount))<0) return ret;
			if ((ret=archivefs_HuffmanCreateOrderlyTable(state->distanceTree,lengthTable+symbolCount,distanceCount))<0) return ret;
		}
		state->bitsLeft=bitsLeft;
		state->accumulator=accumulator;
		return 1;

		default:
		return WVFS_ERROR_DECOMPRESSION_ERROR;
	}
}

static const uint16_t archivefs_zipLengthAdditions[29]={
	  3,  4,  5,  6,  7,  8,  9, 10,
	 11, 13, 15, 17, 19, 23, 27, 31,
	 35, 43, 51, 59, 67, 83, 99,115,
	131,163,195,227,258};

static const uint8_t archivefs_zip_LengthBits[29]={
	0,0,0,0,0,0,0,0,
	1,1,1,1,2,2,2,2,
	3,3,3,3,4,4,4,4,
	5,5,5,5,0};

static const uint16_t archivefs_zipDistanceAdditions[30]={
	0x001,0x002,0x003,0x004,0x005,0x007,0x009,0x00d,
	0x011,0x019,0x021,0x031,0x041,0x061,0x081,0x0c1,
	0x101,0x181,0x201,0x301,0x401,0x601,0x801,0xc01,
	0x1001,0x1801,0x2001,0x3001,0x4001,0x6001};

static const uint8_t archivefs_zipDistanceBits[30]={
	 0, 0, 0, 0, 1, 1, 2, 2,
	 3, 3, 4, 4, 5, 5, 6, 6,
	 7, 7, 8, 8, 9, 9,10,10,
	11,11,12,12,13,13};

static int archivefs_zipDecompressFull(struct archivefs_zipDecompressState *state,uint8_t *dest)
{
	uint32_t length=state->rawLength;
	uint32_t pos,blockLength=0;
	uint16_t count,distance,final=0;
	int32_t ret;
	uint8_t bitsLeft=state->bitsLeft,accumulator=state->accumulator;
	struct archivefs_state *archive;

	archive=state->archive;
	for (pos=0;pos<length;)
	{
		if (!blockLength)
		{
			if (final)
				return WVFS_ERROR_DECOMPRESSION_ERROR;
			archivefs_zipReadBits(final,1U);
			state->bitsLeft=bitsLeft;
			state->accumulator=accumulator;
			blockLength=ret=archivefs_zipInitializeBlock(state);
			if (ret<0) return ret;
			bitsLeft=state->bitsLeft;
			accumulator=state->accumulator;
		}
		if (!state->mode)
		{
			uint32_t bufLength=blockLength;
			uint8_t *buffer;

			if (pos+blockLength>length)
				return WVFS_ERROR_DECOMPRESSION_ERROR;

			while (bufLength)
			{
				if ((ret=archivefs_common_readNextBytes(&buffer,bufLength,archive))<0) return ret;
				archivefs_common_memcpy(dest+pos,buffer,ret);
				pos+=ret;
				bufLength-=ret;
			}
			blockLength=0;
		} else {
			uint16_t symbol;

			while (pos<length)
			{
				archivefs_zipHuffmanDecode(symbol,state->symbolTree);
				if (symbol<256U)
					dest[pos++]=symbol;
				else if (symbol==256U) {
					blockLength=0;
					break;
				} else {
					uint16_t code;

					symbol-=257U;
					archivefs_zipReadBits(count,archivefs_zip_LengthBits[symbol]);
					count+=archivefs_zipLengthAdditions[symbol];
					if (pos+count>length)
						return WVFS_ERROR_DECOMPRESSION_ERROR;

					archivefs_zipHuffmanDecode(code,state->distanceTree);
					archivefs_zipReadBits(distance,archivefs_zipDistanceBits[code]);
					distance+=archivefs_zipDistanceAdditions[code];
					if (distance>pos)
						return WVFS_ERROR_DECOMPRESSION_ERROR;

					while (count--)
					{
						dest[pos]=dest[pos-distance];
						pos++;
					}
				}
			}
		}
	}
	archivefs_zipDecompressReset(state);
	return 0;
}

static int archivefs_zipDecompressSkip(struct archivefs_zipDecompressState *state,uint32_t targetLength)
{
#define ARCHIVEFS_ZIP_DECOMPRESS_SKIP 1
#include "archivefs_zip_decompressor_template.h"
}

static int archivefs_zipDecompressPartial(struct archivefs_zipDecompressState *state,uint8_t *dest,uint32_t targetLength)
{
#undef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
#include "archivefs_zip_decompressor_template.h"
}

int32_t archivefs_zipDecompress(struct archivefs_zipDecompressState *state,uint8_t *dest,int32_t length,uint32_t offset)
{
	int ret;

	if (offset<state->rawPos)
		archivefs_zipDecompressReset(state);
	archivefs_common_initBlockBuffer(state->fileOffset+state->filePos,state->archive);
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
