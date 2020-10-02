/* Copyright (C) Teemu Suutari */

#ifndef CONTAINER_API_H
#define CONTAINER_API_H


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
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

/*
   These are the memory allocation functions that need to be available
   in order to container_api to link.
   Semantics should be the same as in malloc/free (memory from heap)
   wepl: I don't see why this should be in api.h it may be used container internal and is not api
*/

extern void *container_malloc(uint32_t size);
extern void container_free(void *ptr);

/*
   Define this and the code tries to open non-amiga archives as well
*/
//#define CONTAINER_ALLOW_NON_AMIGA_ARCHIVES 1

/*
   Error codes returned by the container_* functions
*/
#define CONTAINER_ERROR_INVALID_FORMAT (-1)
#define CONTAINER_ERROR_UNSUPPORTED_FORMAT (-2)
#define CONTAINER_ERROR_MEMORY_ALLOCATION_FAILED (-3)
#define CONTAINER_ERROR_FILE_NOT_FOUND (-4)
#define CONTAINER_ERROR_END_OF_ENTRIES (-5)
#define CONTAINER_ERROR_NON_AMIGA_ARCHIVE (-6)
#define CONTAINER_ERROR_INVALID_FILE_TYPE (-7)
#define CONTAINER_ERROR_DECOMPRESSION_ERROR (-8)
#define CONTAINER_ERROR_INVALID_READ (-9)

/*
   The read function given as a parameter to the container_initialize function
   will be called by the container_* functions in order to read the file
   Parameters:
	* dest - pointer to destination buffer to be filled
	* length - length of data to be read
	* offset - file offset from where to start reading
	* context - a pass through pointer for context
   Returns:
	* bytes read if succesfull - it is expected that length bytes is returned i.e. no partial reads
	* negative values in case of errors - these negative values are passed through on container_* functions are their return values
   wepl: there is no need for such a function to be provided by whdload
*/
//typedef int (*container_read_func)(void *dest,uint32_t length,uint32_t offset,void *context);

/*
   Initialize container. Will allocate memory for the directory structure, file list and internal state.
   Parameters:
	* container - return pointer to void* container. Will be initialized if successfull
	* context - a pass through pointer to the read function
	* read_func - function pointer to the read function which will be used to read the archive
	* length - file length for the archive
   Returns:
	* error code or 0 if success. In case error occured all allocated memory is freed0
   wepl: I don't understand for what container And context should be needed, container should be sufficient
   additionally must be the archive to work on referenced, we can do this by filename, filelock or filehandle
   I probably would prefer a filelock but we can maybe adapt that later, filename is more portable for now maybe
*/
//extern int container_initialize(void **container,void *context,container_read_func read_func,uint32_t length);
extern int container_initialize(void **container,const char *filename);

/*
   Uninitialize container. frees all memory
   Parameters:
	* container - pointer to container
   Returns:
	* error code or 0 if success.
*/
extern int container_uninitialize(void *container);

/*
   Query a file list. Does not allocate memory, will always succeed
   Parameters:
	* container - pointer to container
   Returns:
	* Pointer to null terminated string table container filenames with paths of all the files in the archive
   wepl: I don't want a filelist. I like that the container iterates over all files in the archive and calls
   a provided function. The function returns either 0 for terminate, -1 to skip this file or a memory pointer to
   read the file into.
*/
//extern const char **container_getFileList(void *container);
typedef int (*whd_allocFile)(const char *name,uint32_t length);
extern const char **container_fileCache(void *container,whd_allocFile func);


/*
   Get file size. Does not allocate memory
   Parameters:
	* container - pointer to container
	* name - file name w. path
   Returns:
	* error code or 0 if success.
*/
extern int container_getFileSize(void *container,const char *name);

/*
   Test if file compressed. Does not allocate memory
   Parameters:
	* container - pointer to container
	* name - file name w. path
   Returns:
	* error code or 0 if success.
	* error code or 0 for non-compressed, 1 for compressed
   wepl: ATM this is not needed, it would be only needed if we want support that whdload preloads
   compressed data, this will not be part of phase 1
*/
extern int container_isCompressed(void *container,const char *name);

/*
   Fill in relevant portions 232-byte FIB for a file entry. initialize internal state if file is a directory for exnext
   Does not allocate memory
   Parameters:
	* dest_fib - pointer to the FIB to be filled
	* container - pointer to container
	* name - file name w. path
   Returns:
	* error code or 0 if success.
   wepl: similar to container_fileCache I like to have a function which iterates over
   all entries in the archive (including (empty) dirs) and calls a whdload function.
   The function receives a fib for this entry. whdload will copy relevant parts of the fib
   to own memory and returns a flag to continue=-1 or stop=0
*/
typedef int (*whd_registerEntry)(const char *path, const FIB *fib);
extern int container_examine(void *container,whd_registerEntry func);

/*
   Fill in relevant portions 232-byte FIB for a file entry for next item in the directory
   Does not allocate memory
   Parameters:
	* dest_fib - pointer to the FIB to be filled
	* container - pointer to container
   Returns:
	* error code or 0 if success.
   wepl: this isn't needed
*/
extern int container_exnext(void *dest_fib,void *container);

/*
   Read a file
   Does not allocate memory
   Parameters:
	* dest - pointer to destination buffer to be filled
	* container - pointer to container
	* name - file name w. path
	* length - length of data to be read
	* offset - file offset from where to start reading
   Returns:
	* error code or bytes read if success.
*/
extern int container_fileRead(void *dest,void *container,const char *name,uint32_t length,uint32_t offset);

#endif
