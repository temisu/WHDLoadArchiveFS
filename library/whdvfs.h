#ifndef WHDVFS_H
#define WHDVFS_H
/*
** whdvfs.h
** include file for WHDLoad Virtual File System interface
** Copyright (C) Teemu Suutari, Wepl
*/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* EXEC_TYPES_H */

/*
** Error codes returned by wvfs_* functions
*/
#define WVFS_ERROR_INVALID_FORMAT (-1)		 /* invalid structure of supported file format */
#define WVFS_ERROR_UNSUPPORTED_FORMAT (-2)	 /* file format is not supported */
#define WVFS_ERROR_MEMORY_ALLOCATION_FAILED (-3) /* failed to allocate memory */
#define WVFS_ERROR_FILE_NOT_FOUND (-4)		 /* file does not exist */
#define WVFS_ERROR_NON_AMIGA_ARCHIVE (-5)
#define WVFS_ERROR_INVALID_FILE_TYPE (-6)	 /* filename points to non-file (e.g. directory) */
#define WVFS_ERROR_DECOMPRESSION_ERROR (-7)	 /* failed to decompress a file */
#define WVFS_ERROR_INVALID_READ (-8)		 /* offset and/or length is not valid for this file */
#define WVFS_ERROR_OPERATION_CANCELED (-9)	 /* callback returned stop condition */

/*
** API functions
*/

/*
** Initialize wvfs
**
** Open given file and check for a supported format.
** Allocate all required resources to handle the format (e.g. file handle, directory structure,
** file list, internal state).
**
** Parameters:
**	wvfs - return pointer to APTR wvfs. Will be initialized if successful.
**	filename - filename of the archive to open
**
** Returns:
**	error code or 0 on success. In case of an error all allocated resources are freed.
**
** Notable error codes:
**	WVFS_ERROR_UNSUPPORTED_FORMAT - file format is not supported
**	WVFS_ERROR_INVALID_FORMAT - invalid structure of supported file format
**	WVFS_ERROR_MEMORY_ALLOCATION_FAILED - Failed to allocate memory
*/
LONG wvfs_initialize(APTR *wvfs, const CPTR filename);

/*
** Uninitialize wvfs
**
** Free wvfs handle and all allocated resources.
**
** Parameters:
**	wvfs - pointer to handle
** Returns:
**	error code or 0 on success
** Notable error codes:
**	none
*/
LONG wvfs_uninitialize(APTR wvfs);

/*
** wvfs_registerEntry gets called by wvfs_dirCache for each entry in the archive
**
** Parameters:
**	fullpath - path and filename of the entry
**	fib - filled FileInfoBlock structure
** Returns:
**	0 - stop processing further entries. wvfs_dirCache returns with WVFS_ERROR_OPERATION_CANCELED
**	-1 - continue processing
*/
typedef LONG (*wvfs_registerEntry)(const CPTR fullpath, const void *fib);

/*
** Get metadata for all entries in archive.
**
** Iterate over all directories and files and provide metadata in a dos.FileInfoBlock.
** WHDLoad will copy the information via the provided function wvfs_registerEntry to a private storage.
** Function must not allocate any memory!
**
** Parameters:
**	wvfs - pointer to archive
**	registerFunc - function to call for each entry
** Returns:
**	error code or 0 if success
** Notable error codes:
**	WVFS_ERROR_OPERATION_CANCELED - callback returned stop condition
*/
LONG wvfs_dirCache(APTR wvfs, wvfs_registerEntry registerFunc);

/*
** wvfs_allocFile gets called by wvfs_fileCache for each file in the archive.
**
** Parameters:
**	fullpath - path and filename of the entry
**	length - length of the file
** Returns:
**	NULL - when no more files are wanted. i.e. terminate wvfs_fileCache with VFS_ERROR_OPERATION_CANCELED
**	-1 - skip this file
**	APTR - allocated buffer of at least file length where the file is requested to read into
*/
typedef APTR (*wvfs_allocFile)(const CPTR fullpath, ULONG length);

/*
** Load all files from the archive.
**
** wvfs_fileCache will call wvfs_allocFile for each file in the archive and if the function
** returns a valid pointer the file contents are read to it.
**
** Parameters:
**	wvfs - pointer to archive
**	fileFunc - function to call for each file
** Returns:
**	error code or 0 on success.
** Notable error codes:
**	WVFS_ERROR_DECOMPRESSION_ERROR - failed to decompress a file
**	WVFS_ERROR_OPERATION_CANCELED - callback returned stop condition
*/
LONG wvfs_fileCache(APTR wvfs, wvfs_allocFile fileFunc);

/*
** get filesize of an entry
**
** Parameters:
**	wvfs - pointer to archive
**	name - file name with path
** Returns:
**	error code or file size on success
** Notable error codes:
**	WVFS_ERROR_FILE_NOT_FOUND - file does not exist
**	WVFS_ERROR_INVALID_FILE_TYPE - filename points to non-file (e.g. directory)
*/
LONG wvfs_getFileSize(APTR wvfs, const CPTR name);

/*
** Read a file
**
** Parameters:
**	wvfs - pointer to archive
**	dest - pointer to destination buffer to be filled
**	name - file name with path
**	length - length of data to read
**	offset - file offset from where to start reading
** Returns:
**	error code or bytes read on success.
** Notable error codes:
**	WVFS_ERROR_FILE_NOT_FOUND - file does not exist
**	WVFS_ERROR_INVALID_FILE_TYPE - filename points to non-file (f.e. directory)
**	WVFS_ERROR_DECOMPRESSION_ERROR - failed to decompress the file
**	WVFS_ERROR_INVALID_READ - offset and/or length is not valid for this file
*/
LONG wvfs_fileRead(APTR wvfs, APTR dest, const CPTR name, ULONG length, ULONG offset);

/*
** Return description for error code
**
** Parameters:
**	error_code error to be queried
** Returns:
**	Pointer to a string for the error description
*/
const CPTR wvfs_getErrorString(LONG error_code);

#endif	/* WHDVFS_H */
