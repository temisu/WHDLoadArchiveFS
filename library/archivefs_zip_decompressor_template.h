/* Copyright (C) Teemu Suutari */

/* Skip and DecodePartial are almost the same, still different enough */

	uint32_t pos=state->rawPos,blockLength=state->blockRemaining;
	uint16_t count=state->remainingRepeat,distance=state->distance;
	uint32_t length=state->rawLength;
	uint16_t final=state->final;
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
	uint32_t bufferPos=0;
#endif
	uint16_t historyPos=state->historyPos;
	uint8_t *history=state->history;
	uint8_t bitsLeft=state->bitsLeft,accumulator=state->accumulator;
	int32_t ret;
	struct archivefs_state *archive;

	archive=state->archive;
	while (pos<targetLength && count)
	{
		uint8_t ch;

		ch=history[(historyPos-distance)&32767U];
		history[historyPos++]=ch;
		historyPos&=32767U;
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
		dest[bufferPos++]=ch;
#endif
		pos++;
		count--;
	}

	for (;pos<targetLength;)
	{
		if (!blockLength)
		{
			if (final)
				return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;
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
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
			uint8_t *buffer;
#endif

			if (pos+blockLength>length)
				return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;

			if (pos+bufLength>targetLength) bufLength=targetLength-pos;
			blockLength-=bufLength;
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
			while (bufLength)
			{
				if ((ret=archivefs_common_readNextBytes(&buffer,bufLength,archive))<0) return ret;
				archivefs_common_memcpy(dest+pos,buffer,ret);
				pos+=ret;
				bufLength-=ret;
			}
#else
			if ((ret=archivefs_common_skipNextBytes(bufLength,archive))<0) return ret;
#endif
		} else {
			uint16_t symbol;

			while (pos<targetLength)
			{
				archivefs_zipHuffmanDecode(symbol,state->symbolTree);
				if (symbol<256U) {
					history[historyPos++]=symbol;
					historyPos&=32767U;
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
					dest[bufferPos++]=symbol;
#endif
					pos++;
				} else if (symbol==256U) {
					blockLength=0;
					break;
				} else {
					uint16_t code;

					symbol-=257U;
					archivefs_zipReadBits(count,archivefs_zip_LengthBits[symbol]);
					count+=archivefs_zipLengthAdditions[symbol];
					if (pos+count>length)
						return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;

					archivefs_zipHuffmanDecode(code,state->distanceTree);
					archivefs_zipReadBits(distance,archivefs_zipDistanceBits[code]);
					distance+=archivefs_zipDistanceAdditions[code];
					if (distance>pos)
						return ARCHIVEFS_ERROR_DECOMPRESSION_ERROR;

					while (pos<targetLength && count)
					{
						uint8_t ch;

						ch=history[(historyPos-distance)&32767U];
						history[historyPos++]=ch;
						historyPos&=32767U;
#ifndef ARCHIVEFS_ZIP_DECOMPRESS_SKIP
						dest[bufferPos++]=ch;
#endif
						pos++;
						count--;
					}
				}
			}
		}
	}

	state->filePos=(state->archive->blockIndex<<state->archive->blockShift)+state->archive->blockPos-state->fileOffset;
	state->rawPos=pos;
	state->blockRemaining=blockLength;
	state->remainingRepeat=count;
	state->final=final;
	state->distance=distance;
	state->historyPos=historyPos;
	state->bitsLeft=bitsLeft;
	state->accumulator=accumulator;
	return 0;
