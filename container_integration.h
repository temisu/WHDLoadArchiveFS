/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_INTEGRATION_H
#define CONTAINER_INTEGRATION_H

#include "container_api.h"


/* here are the functions required for system integration */

/*
   These are the memory allocation functions that need to be available
   in order to container_api to link.
   Semantics should be the same as in malloc/free (memory from heap)
*/

extern void *container_malloc(uint32_t size);
extern void container_free(void *ptr);


/*
   container_integration_fileOpen will open file for reading after querying its length
   Parameters:
	* filename - filename to open
	* length - return parameter for the file length
	* file - return parameter for the returned file, anything else except null
   Returns:
	* 0 if succesfull
	* negative values in case of errors - these negative values are passed through on container_* functions are their return values
*/
extern int container_integration_fileOpen(const char *filename,uint32_t *length,void **file);

/*
   container_integration_fileClose will close the file opened by container_integration_fileOpen
   Parameters:
	* file - file to close
   Returns:
	* 0 if succesfull
	* negative values in case of errors - these negative values are passed through on container_* functions are their return values
*/
extern int container_integration_fileClose(void *file);

/*
   container_integration_fileRead will be read (portion of) a file
   Parameters:
	* dest - pointer to destination buffer to be filled
	* length - length of data to be read
	* offset - file offset from where to start reading
	* file - opaque pointer for the file
   Returns:
	* bytes read if succesfull - it is expected that length bytes is returned i.e. no partial reads
	* negative values in case of errors - these negative values are passed through on container_* functions are their return values
*/
extern int container_integration_fileRead(void *dest,uint32_t length,uint32_t offset,void *file);

#endif
