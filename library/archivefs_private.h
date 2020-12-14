/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_PRIVATE_H
#define ARCHIVEFS_PRIVATE_H

#include "archivefs_api.h"
#include "archivefs_lha.h"
#include "archivefs_zip.h"

#define ARCHIVEFS_TYPE_FILE (-3)
#define ARCHIVEFS_TYPE_DIR (2)

/* radically smaller than fib (depending on the length of the filename / filenote though) */
struct archivefs_cached_file_entry
{
	uint32_t				dataOffset;
	uint32_t				dataLength;
	/* 0 unpacked, 1+ packed */
	uint32_t				dataType;
	uint32_t				length;

	int32_t					fileType;
	uint32_t				protection;
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;

	char					*path;
	char					*pathAndName;
	char					*filename;
	char					*filenote;

	struct archivefs_cached_file_entry	*prev;
	struct archivefs_cached_file_entry	*next;
};


struct archivefs_state;

struct archivefs_file_state
{
	struct archivefs_state			*archive;

	union
	{
		struct archivefs_lha_file_state	lha;
		struct archivefs_zip_file_state	zip;
	} state;
};

struct archivefs_state
{
	/* reading the file */
	void					*file;
	uint32_t				filePos;
	uint32_t				fileLength;

	/* internal buffering */
	uint8_t					blockShift;
	uint32_t				blockIndex;
	uint32_t				blockPos;
	uint32_t				blockLength;

	/* contents of the archive */
	struct archivefs_cached_file_entry	*firstEntry;
	struct archivefs_cached_file_entry	*lastEntry;
	
	/* pointers to the implementation */
	int (*fileOpen)(struct archivefs_file_state*,struct archivefs_cached_file_entry*);
	int32_t (*fileRead)(void *dest,struct archivefs_file_state *,uint32_t,uint32_t);
	int (*uninitialize)(struct archivefs_state*);

	/* Progress indicator */
	archivefs_progressIndicator		progressFunc;
	uint32_t				currentProgress;
	uint32_t				totalBlocks;
	int					progressEnabled;

	union
	{
		struct archivefs_lha_state	lha;
		struct archivefs_zip_state	zip;
	} state;

	/* must be align 2 */
	uint8_t					blockData[1];
};

struct archivefs_combined_state
{
	struct archivefs_cached_file_entry	*currentFile;

	struct archivefs_file_state		fileState;
	struct archivefs_state			archive;
};

struct FIB
{
	/* unknown */
	uint32_t				diskKey;
	/* >0 dir, <0 file
	   -3 file
	   -4 hard link to file
	   -5 pipe
	   1 root
	   2 dir
	   3 softlink
	   4 hard link to dir
	*/
	uint32_t				dirEntryType;
	uint8_t					filename[108];
	uint32_t				protection;
	/* obsolete?!? */
	uint32_t				entryType;
	uint32_t				size;
	uint32_t				blocks;
	/* mtime */
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;
	uint8_t					comment[80];
	/* not really used */
	uint16_t				uid;
	uint16_t				gid;
	uint8_t					reserved[32];
};

#endif
