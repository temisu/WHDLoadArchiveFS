/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_HUFFMAN_DECODER_H
#define ARCHIVEFS_HUFFMAN_DECODER_H

#include "archivefs_api.h"

#define archivefs_HuffmanDecode(nodes,bitReader) \
{ \
	int bit; \
	uint16_t i=(nodes)[0]; \
	while (!(i&0x8000U)) \
	{ \
		if ((bit=(bitReader))<0) return bit; \
		i=(nodes)[i+bit]; \
	} \
	return i&0x7fffU; \
}

extern int archivefs_HuffmanCreateOrderlyTable(uint16_t *nodes,const uint8_t *bitLengths,uint32_t bitTableLength);

extern void archivefs_HuffmanCreateEmptyTable(uint16_t *nodes,uint16_t value);

#endif
