/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_LHA_H
#define CONTAINER_LHA_H

struct container_lha_file_state
{
	const struct container_cached_file_entry	*entry;
};

struct container_state;
extern int container_lha_initialize(struct container_state *container);

#endif
