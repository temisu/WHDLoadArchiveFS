/* Copyright (C) Teemu Suutari */

#include "archivefs_huffman_decoder.h"

static uint16_t archivefs_HuffmanInsert(uint16_t *nodes,uint16_t nodeCount,uint16_t length,uint32_t code,uint16_t value)
{
	int currentBit;
	uint16_t i=0;

	for (currentBit=length-1U;currentBit>=0;currentBit--)
	{
		if (!nodes[i])
		{
			nodes[i]=nodeCount;
			i=nodeCount;
			nodeCount+=2;
			nodes[i]=0;
			nodes[i+1]=0;
		} else {
			i=nodes[i];
		}
		i+=(code>>currentBit)&1U;
		if (nodes[i]&0x8000U)
			return 0;
	}
	nodes[i]=value|0x8000U;
	return nodeCount;
}

int archivefs_HuffmanCreateOrderlyTable(uint16_t *nodes,const uint8_t *bitLengths,uint32_t bitTableLength)
{
	uint16_t i,nodeCount=1;
	uint16_t minDepth=33,maxDepth=0,depth;
	uint32_t code=0;

	nodes[0]=0;

	for (i=0;i<bitTableLength;i++)
	{
		uint8_t length=bitLengths[i];
		if (length>32)
			return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
		if (length)
		{
			if (length<minDepth) minDepth=length;
			if (length>maxDepth) maxDepth=length;
		}
	}
	if (!maxDepth) return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;

	for (depth=minDepth;depth<=maxDepth;depth++)
	{
		for (i=0;i<bitTableLength;i++)
		{
			if (bitLengths[i]==depth)
			{
				if (!(nodeCount=archivefs_HuffmanInsert(nodes,nodeCount,depth,code>>(maxDepth-depth),i)))
					return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
				code+=1U<<(maxDepth-depth);
			}
		}
	}
	return 0;
}

void archivefs_HuffmanCreateEmptyTable(uint16_t *nodes,uint16_t value)
{
	nodes[0]=value|0x8000U;
}
