/* Copyright (C) Teemu Suutari */

#ifndef ARCHIVEFS_API_H
#define ARCHIVEFS_API_H


/*
   in order not to depend on any external headers
   we need to define uint32_t, int32_t, uint16_t and uint8_t here
   to suit the architecture
*/

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif
#ifndef int32_t
typedef int int32_t;
#endif
#ifndef uint16_t
typedef unsigned short uint16_t;
#endif
#ifndef int16_t
typedef short int16_t;
#endif
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif
#ifndef int8_t
typedef signed char int8_t;
#endif


/*
   Define this and the code tries to open non-amiga archives as well
*/
/*
#define ARCHIVEFS_ALLOW_NON_AMIGA_ARCHIVES 1
*/

/*
   Error codes returned by the archivefs_* functions
*/
#define ARCHIVEFS_ERROR_INVALID_FORMAT (-1)
#define ARCHIVEFS_ERROR_UNSUPPORTED_FORMAT (-2)
#define ARCHIVEFS_ERROR_MEMORY_ALLOCATION_FAILED (-3)
#define ARCHIVEFS_ERROR_FILE_NOT_FOUND (-4)
#define ARCHIVEFS_ERROR_NON_AMIGA_ARCHIVE (-5)
#define ARCHIVEFS_ERROR_INVALID_FILE_TYPE (-6)
#define ARCHIVEFS_ERROR_DECOMPRESSION_ERROR (-7)
#define ARCHIVEFS_ERROR_INVALID_READ (-8)
#define ARCHIVEFS_ERROR_OPERATION_CANCELED (-9)

/* callback functions for the API */

/*
   archivefs_allocFile will be called by archivefs_fileCache for each file in the archive.
   Parameters:
	* name - name of the file, including path
	* length - length of the file
   Returns:
	* null - when no more files are wanted. i.e. terminate the archivefs_fileCache. fileCache will return with ARCHIVEFS_ERROR_OPERATION_CANCELED error
	* -1 casted as void* - skip this file
	* any other pointer - allocated buffer of at least file length where the file is requested to read into
*/
typedef void *(*archivefs_allocFile)(const char *name,uint32_t length);

/*
   archivefs_registerEntry will be calles by archivefs_dirCache for each entry in the archive
   Parameters:
	* fullpath - path and filename of the file
	* fib - pointer to the 232-byte fib structure for the entry
   Returns:
	* 0 - stop processing further entries. dirCache will return with ARCHIVEFS_ERROR_OPERATION_CANCELED error
	* -1 (or any nonzero value) - continue processing
*/
typedef int (*archivefs_registerEntry)(const char *fullpath,const void *fib);

typedef void (*archivefs_progressIndicator)(uint32_t current,uint32_t max);

/* API */

/*
   Initialize archive. Will allocate memory for the directory structure, file list and internal state.
   Parameters:
	* archive - return pointer to void* archive. Will be initialized if successfull
	* filename - filename for the archive
   Returns:
	* error code or 0 if success. In case error occured all allocated memory is freed
   Notable error codes:
	* Pass through errors from archivefs_integration
	* ARCHIVEFS_ERROR_INVALID_FORMAT - file could not be parsed
	* ARCHIVEFS_ERROR_UNSUPPORTED_FORMAT - file format not supported
	* ARCHIVEFS_ERROR_MEMORY_ALLOCATION_FAILED - Failed to allocate memory
	* ARCHIVEFS_ERROR_NON_AMIGA_ARCHIVE - Archive is not created in Amiga OS
*/
int archivefs_initialize(void **archive,const char *filename);

/*
   Uninitialize archive. frees all memory
   Parameters:
	* archive - pointer to archive
   Returns:
	* error code or 0 if success.
   Notable error codes:
	* none
*/
int archivefs_uninitialize(void *archive);

/*
   Get file size. Does not allocate memory
   Parameters:
	* archive - pointer to archive
	* name - file name w. path
   Returns:
	* error code or file size if success
   Notable error codes:
	* ARCHIVEFS_ERROR_FILE_NOT_FOUND - file does not exist
	* ARCHIVEFS_ERROR_INVALID_FILE_TYPE - filename points to non-file (f.e. directory)
*/
uint32_t archivefs_getFileSize(void *archive,const char *name);

/*
   Read (chosen) files into fileCache.
   Does not allocate memory
   archivefs_fileCache will call archivefs_allocFile for each file in the archive and if the function returns with a valid pointer
   file contents are read to it.
   Parameters:
	* archive - pointer to archive
	* fileFunc - function to call for each file
   Returns:
	* error code or 0 if success. If error is returned, the last file might not have been read to the buffer
   Notable error codes:
	* Pass through errors from archivefs_integration
	* ARCHIVEFS_ERROR_INVALID_FORMAT - file could not be parsed
	* ARCHIVEFS_ERROR_DECOMPRESSION_ERROR - failed to decompress the file
	* ARCHIVEFS_ERROR_OPERATION_CANCELED - callback returned stop condition
*/
int archivefs_fileCache(void *archive,archivefs_allocFile fileFunc);

/*
   Query FIB of all the entries in the archive.
   Does not allocate memory
   Parameters:
	* archive - pointer to archive
	* registerFunc - function to call for each entry
   Returns:
	* error code or 0 if success.
   Notable error codes:
	* ARCHIVEFS_ERROR_OPERATION_CANCELED - callback returned stop condition
*/
int archivefs_dirCache(void *archive,archivefs_registerEntry registerFunc);

/*
   Read a file
   Does not allocate memory
   Parameters:
	* archive - pointer to archive
	* dest - pointer to destination buffer to be filled
	* name - file name w. path
	* length - length of data to be read
	* offset - file offset from where to start reading
   Returns:
	* error code or bytes read if success.
   Notable error codes:
	* Pass through errors from archivefs_integration
	* ARCHIVEFS_ERROR_FILE_NOT_FOUND - file does not exist
	* ARCHIVEFS_ERROR_INVALID_FILE_TYPE - filename points to non-file (f.e. directory)
	* ARCHIVEFS_ERROR_DECOMPRESSION_ERROR - failed to decompress the file
	* ARCHIVEFS_ERROR_INVALID_READ - offset and/or length is not valid for this file
*/
int32_t archivefs_fileRead(void *archive,void *dest,const char *name,uint32_t length,uint32_t offset);

/*
   Returns description for error code
   Parameters:
	* error_code error to be queried
   Returns:
        * Pointer to a string for the error description
*/
const char *archivefs_getErrorString(int error_code);

int archivefs_setProgressCallback(void *archive,archivefs_progressIndicator progressFunc);

#endif
