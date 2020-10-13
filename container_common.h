/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_COMMON_H
#define CONTAINER_COMMON_H

#include "container_private.h"

#define GET_LE32(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8)|(((uint16_t)((x)[2]))<<16)|(((uint16_t)((x)[3]))<<24))
#define GET_LE16(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8))

extern int container_common_simpleRead(void *dest,uint32_t length,uint32_t offset,struct container_state *container);
extern uint32_t container_common_unixTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince);
extern uint32_t container_common_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince);
extern void container_common_insertFileEntry(struct container_state *container,struct container_cached_file_entry *entry);

#endif
