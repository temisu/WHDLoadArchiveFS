;*---------------------------------------------------------------------------
;  :Module.	whdvfs.i
;  :Contens.	include file for WHDLoad virtual filesystem interface
;  :Author.	Bert Jahn
;  :History.	2020-11-13 created
;  :Copyright.	© 2020 Bert Jahn, All Rights Reserved
;  :Language.	68000 Assembler
;  :Translator.	BASM 2.16, ASM-One 1.44, Asm-Pro 1.17, PhxAss 4.38, Devpac 3.18, VASM 1.8h
;---------------------------------------------------------------------------*

 IFND WHDVFS_I
WHDVFS_I SET 1

	IFND	EXEC_TYPES_I
	INCLUDE	exec/types.i
	ENDC

;=============================================================================
; misc
;=============================================================================

WVFS_HEADER	MACRO
		moveq	#-1,d0
		rts
		dc.b	"WHDLDVFS"
	ENDM

;=============================================================================
; file header	Version 1+
; the file is a standard AmigaDOS hunk file loaded via dos.LoadSeg and
; searched by WHDLoad as PROGDIR:WHDLoad.VFS
;=============================================================================

    STRUCTURE	WHDLoadVFS,0
	STRUCT	wvfs_security,4		; moveq #-1,d0 rts
	STRUCT	wvfs_id,8		; "WHDLDVFS"
	UWORD	wvfs_version		; version of this structure
	UWORD	wvfs_flags		; see below
	CPTR	wvfs_name		; id-string of this interface implementation
					; displayed in informational messages
					; (e.g. "temisu 7.43 (13.11.2020)")
	; function table
	; a pointer to each functions
	; each function uses C-style parameter passing via stack and returns
	; a possible return value in d0
	; all error codes returned by VFS functions must be negative

	; initialize wvfs
	; LONG error initialize(APTR *wvfs, const CPTR filename);
	APTR	wvfs_initialize

	; free wvfs obtained via initialize()
	; LONG error uninitialize(APTR wvfs);
	APTR	wvfs_uninitialize

	; get metadata for all entries
	; iterate over all directories and files and provide metadata in a 
	; dos.FileInfoBlock
	; provided function regEntry will copy the information to private
	; storage
	; LONG cont regEntry(const CPTR name, const APTR fib)
	; parameters:
	;	name - full path + dir/file name
	;	fib - filled FileInfoBlock structure
	; it returns:
	;	0 - stop processing
	;	-1 - continue processing
	; LONG error dirCache(APTR wvfs, APTR regEntry)
	APTR	wvfs_dirCache

	; load all files
	; iterate over all files, call allocFile and load file for each
	; provided function allocFile will allocate memory for the file to load
	; APTR mem allocFile(const CPTR name, ULONG size)
	; parameters:
	;	name - full path + file name
	;	size - size of the file (can be zero!)
	; it returns:
	;	NULL - stop processing
	;	-1 - skip this file
	;	APTR - load file into this memory location
	; LONG error fileCache(APTR wvfs, APTR allocFile)
	APTR	wvfs_fileCache

	; get filesize of an entry
	; return positive numbers for size, negative numbers on error
	; ULONG size getFileSize(APTR wvfs, const CPTR filename)
	APTR	wvfs_getFileSize

	; read part of file into given memory
	; returns error code or bytes read if success
	; LONG length fileRead(APTR wvfs, APTR dest, const CPTR name, ULONG length, ULONG offset)
	APTR	wvfs_fileRead

	; return description for an error code
	; CPTR text getErrorString(LONG code)
	APTR	wvfs_getErrorString

	; end
	LABEL	wvfs_SIZEOF

;=============================================================================
; Flags for wvfs_flags
;=============================================================================

	BITDEF WVFS,Dummy,0		; none yet

;=============================================================================

 ENDC

