/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_ZIP_H
#define ARCHIVEFS_ZIP_H

struct archivefs_zipDecompressState;

/* hackish, but it allows us to not allocate decompressor dynamically */
struct archivefs_zip_state
{
	struct archivefs_zipDecompressState		*decompressState;
};

struct archivefs_zip_file_state
{
	struct archivefs_zipDecompressState		*decompressState;
	struct archivefs_cached_file_entry	*entry;
};

struct archivefs_state;
extern int archivefs_zip_initialize(struct archivefs_state *archive);

#endif
