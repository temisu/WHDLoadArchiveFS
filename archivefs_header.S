; Copyright (C) Teemu Suutari

	INCDIR include
	INCLUDE whdvfs.i

	SECTION code,CODE

	WVFS_HEADER
	dc.w	1	; version
	dc.w	0	; flags
	dc.l	_name

	dc.l	_archivefs_initialize
	dc.l	_archivefs_uninitialize
	dc.l	0	; dircache
	dc.l	_archivefs_fileCache
	dc.l	_archivefs_getFileSize
	dc.l	_archivefs_fileRead
	dc.l	_archivefs_getErrorString

_name:
	dc.b 'ArchiveFS pre-release 0.1',0
