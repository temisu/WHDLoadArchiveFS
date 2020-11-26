/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_LHA_H
#define ARCHIVEFS_LHA_H

struct archivefs_lhaDecompressState;

/* hackish, but it allows us to not allocate decompressor dynamically */
struct archivefs_lha_state
{
	struct archivefs_lhaDecompressState		*decompressState;
};

struct archivefs_lha_file_state
{
	struct archivefs_lhaDecompressState		*decompressState;
	const struct archivefs_cached_file_entry	*entry;
};

struct archivefs_state;
extern int archivefs_lha_initialize(struct archivefs_state *archive);

#endif
