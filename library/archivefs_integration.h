/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_INTEGRATION_H
#define ARCHIVEFS_INTEGRATION_H

#include "archivefs_api.h"


/* here are the functions required for system integration */

/*
   These are the memory allocation functions that need to be available
   in order to archivefs_api to link.
   Semantics should be the same as in malloc/free (memory from heap)
*/

extern void *archivefs_malloc(uint32_t size);
extern void archivefs_free(void *ptr);


/*
   Initialize and uninitialize platform
*/
extern void archivefs_integration_initialize();
extern void archivefs_integration_uninitialize();


/*
   archivefs_integration_fileOpen will open file for reading after querying its length
   Parameters:
	* filename - filename to open
	* length - return parameter for the file length
	* blockShift - return parameter for the recommended block size 1<<blockShift
	* file - return parameter for the returned file, anything else except null
   Returns:
	* 0 if succesfull
	* negative values in case of errors - these negative values are passed through on archivefs_* functions are their return values
*/
extern int archivefs_integration_fileOpen(const char *filename,uint32_t *length,uint8_t *blockShift,void **file);

/*
   archivefs_integration_fileClose will close the file opened by archivefs_integration_fileOpen
   Parameters:
	* file - file to close
   Returns:
	* 0 if succesfull
	* negative values in case of errors - these negative values are passed through on archivefs_* functions are their return values
*/
extern int archivefs_integration_fileClose(void *file);

/*
   archivefs_integration_fileSeel will be seek inside a file
   Parameters:
	* offset - file offset from where to start reading
	* file - opaque pointer for the file
   Returns:
	* 0 if successfull
	* negative values in case of errors - these negative values are passed through on archivefs_* functions are their return values
*/
extern int archivefs_integration_fileSeek(uint32_t offset,void *file);

/*
   archivefs_integration_fileRead will be read (portion of) a file
   Parameters:
	* dest - pointer to destination buffer to be filled
	* length - length of data to be read
	* offset - file offset from where to start reading
	* file - opaque pointer for the file
   Returns:
	* bytes read if succesfull - it is expected that length bytes is returned i.e. no partial reads
	* negative values in case of errors - these negative values are passed through on archivefs_* functions are their return values
*/
extern int32_t archivefs_integration_fileRead(void *dest,uint32_t length,void *file);

#endif
