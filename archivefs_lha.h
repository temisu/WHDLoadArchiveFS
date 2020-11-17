/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_LHA_H
#define ARCHIVEFS_LHA_H

struct archivefs_lha_file_state
{
	const struct archivefs_cached_file_entry	*entry;
};

struct archivefs_state;
extern int archivefs_lha_initialize(struct archivefs_state *archive);

#endif
