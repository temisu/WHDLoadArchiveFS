; Copyright (C) Teemu Suutari

	INCLUDE	whdvfs.i

; section name must be .text to become first with m68k-amigaos-ld

	SECTION .text,CODE

	WVFS_HEADER
	dc.w	1	; version
	dc.w	0	; flags
	dc.l	_name

	dc.l	_archivefs_initialize
	dc.l	_archivefs_uninitialize
	dc.l	_archivefs_dirCache
	dc.l	_archivefs_fileCache
	dc.l	_archivefs_getFileSize
	dc.l	_archivefs_fileRead
	dc.l	_archivefs_getErrorString

	dc.b	"$VER: "
_name	dc.b 'ArchiveFS 1.4 '
	INCBIN	.date
	INCBIN	.ccver
	dc.b	0

