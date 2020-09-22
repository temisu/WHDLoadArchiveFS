/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_COMMON_H
#define CONTAINER_COMMON_H

#include "container_private.h"

#define GET_LE32(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8)|(((uint16_t)((x)[2]))<<16)|(((uint16_t)((x)[3]))<<24))
#define GET_LE16(x) (((uint16_t)((x)[0]))|(((uint16_t)((x)[1]))<<8))

extern int container_simpleRead(void *dest,uint32_t length,uint32_t offset,struct container_state *container);
extern uint32_t container_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince);
extern void container_sortAndInsert(struct container_state *container,struct container_cached_file_entry *entry);

extern int container_common_getFileSize(struct container_state *container,const char *name);
extern int container_common_isCompressed(struct container_state *container,const char *name);
extern const struct container_cached_file_entry *container_common_findEntry(struct container_state *container,const char *name);
extern int container_common_examine(struct FIB *dest_fib,struct container_examine_state *examine_state,const char *name);
extern int container_common_exnext(struct FIB *dest_fib,struct container_examine_state *examine_state);

#endif
