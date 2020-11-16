/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_ZIP_H
#define ARCHIVEFS_ZIP_H

struct archivefs_zip_file_state
{
	struct archivefs_cached_file_entry	*entry;
};

struct archivefs_state;
extern int archivefs_zip_initialize(struct archivefs_state *container);

#endif
