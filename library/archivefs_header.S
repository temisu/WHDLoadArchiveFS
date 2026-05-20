; Copyright (C) Teemu Suutari

		IFD __VASM
IS020		EQU (__VASM>>2)
		ELSE
IS020		EQU 0
		ENDC

	INCLUDE	exec/execbase.i
	INCLUDE	whdvfs.i

; section name must be .text to become first with m68k-amigaos-ld

	SECTION .text,CODE

		WVFS_HEADER
		dc.w	1	; version
		dc.w	0	; flags
		dc.l	_name

	IFEQ IS020
		dc.l	_archivefs_initialize
	ELSE
		dc.l	.cpucheck
	ENDC
		dc.l	_archivefs_uninitialize
		dc.l	_archivefs_dirCache
		dc.l	_archivefs_fileCache
		dc.l	_archivefs_getFileSize
		dc.l	_archivefs_fileRead
		dc.l	_archivefs_getErrorString

	IFNE IS020
.cpucheck	move.l	(4),a0
		btst	#AFB_68020,(AttnFlags+1,a0)
		bne	.cpuok
		moveq	#WVFS_ERROR_68020_REQUIRED,d0
		rts
.cpuok		jmp	_archivefs_initialize
	ENDC

		dc.b	"$VER: "
_name		dc.b	'ArchiveFS 1.5 '
	INCBIN	.date
	IFNE IS020
		dc.b	" 68020"
	ENDC
	INCBIN	.ccver
		dc.b	0

