/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_HUFFMAN_DECODER_H
#define ARCHIVEFS_HUFFMAN_DECODER_H

#include "archivefs_api.h"

#define archivefs_HuffmanDecode(symbol,nodes,bitReader) \
	do { \
		(symbol)=(nodes)[0]; \
		while ((int16_t)symbol>=0) \
		{ \
			bitReader(symbol); \
			(symbol)=*(uint16_t*)((uint8_t*)(nodes)+symbol); \
		} \
		(symbol)=~(symbol); \
	} while (0)

uint16_t archivefs_HuffmanInsert(uint16_t *nodes,uint16_t nodeCount,uint16_t length,uint32_t code,uint16_t value);

int archivefs_HuffmanCreateOrderlyTable(uint16_t *nodes,const uint8_t *bitLengths,uint32_t bitTableLength);

void archivefs_HuffmanCreateEmptyTable(uint16_t *nodes,uint16_t value);

void archivefs_HuffmanReset(uint16_t *nodes);

#endif
