/* Copyright (C) Teemu Suutari */

/* Skip and DecodePartial are almost the same, still different enough */

	uint32_t pos=state->rawPos,spaceLength,blockLength=state->blockRemaining;
	uint16_t symbol=state->remainingRepeat,distance=state->distance;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
	uint32_t bufferPos=0;
#endif
	uint16_t historyPos=state->historyPos;
	uint8_t *history=state->history;
	int32_t ret;
	uint8_t ch;

	if (symbol)
	{
		if (pos<distance)
		{
			spaceLength=distance-pos;
			if (spaceLength>symbol) spaceLength=symbol;
			while (pos<targetLength && spaceLength--)
			{
				history[historyPos++]=0x20U;
				historyPos&=mask;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
				dest[bufferPos++]=0x20U;
#endif
				pos++;
				symbol--;
			}
		}
		while (pos<targetLength && symbol--)
		{
			ch=history[(historyPos-distance)&mask];
			history[historyPos++]=ch;
			historyPos&=mask;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
			dest[bufferPos++]=ch;
#endif
			pos++;
		}
	}

	for (;pos<targetLength;)
	{
		if (!blockLength)
		{
			blockLength=ret=archivefs_lhaInitializeBlock(state);
			if (ret<0) return ret;
		}
		blockLength--;

		archivefs_lhaHuffmanDecode(symbol,state,state->symbolTree);
		if (symbol<256U)
		{
			history[historyPos++]=symbol;
			historyPos&=mask;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
			dest[bufferPos++]=symbol;
#endif
			pos++;
		} else {
			distance=ret=archivefs_lhaDecodeDistance(state,mask);
			if (ret<0) return ret;

			symbol-=253U;
			if (pos<distance)
			{
				spaceLength=distance-pos;
				if (spaceLength>symbol) spaceLength=symbol;
				while (pos<targetLength && spaceLength--)
				{
					history[historyPos++]=0x20U;
					historyPos&=mask;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
					dest[bufferPos++]=0x20U;
#endif
					pos++;
					symbol--;
				}
			}
			while (pos<targetLength && symbol--)
			{
				ch=history[(historyPos-distance)&mask];
				history[historyPos++]=ch;
				historyPos&=mask;
#ifndef ARCHIVEFS_LHA_DECOMPRESS_SKIP
				dest[bufferPos++]=ch;
#endif
				pos++;
			}
		}
	}
	state->rawPos=pos;
	state->blockRemaining=blockLength;
	state->remainingRepeat=symbol;
	state->distance=distance;
	state->historyPos=historyPos;
	return 0;
