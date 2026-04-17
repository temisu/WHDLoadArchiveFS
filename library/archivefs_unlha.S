; Copyright 2026 Michael Rasmussen
; SPDX-License-Identifier: BSD-2-Clause
; See the LICENSE file for details.

		section code,code

		IFD __VASM
IS020		EQU (__VASM>>2)
		ELSE
IS020		EQU 0
		ENDC

; These are fixed by the format
NT		EQU 19
NC		EQU 510
TBIT		EQU 5
CBIT		EQU 9
MAXBITS		EQU 16

; Adjustable (8/12 are the classic values)
PTTABBITS	EQU 8
CTABBITS	EQU 11 ; 10 is faster on 060

RSEVEN	  	MACRO
		rs.b    (__RS&1)
		ENDM

		; Global register usage (except for MakeTable which doesn't need a3-a4/d3-d4)
		; a6 = State
		; a3 = InBufPtr
		; a4 = InBufEnd
		; d3 = BitBuf
		; d4 = BitBufBits
		;
		; Invriants:
		; BitBuf (d3) has BitBufBits (d4) cleanly masked.
		; I.e. BitBuf >> (RequiredBits-BitBufBits) gives "RequiredBits"
		; There is a bit of input overrun

		rsreset
st_OldStack	rs.l    1
st_Context	rs.l    1
st_ReadCallback	rs.l    1
st_WriteCallback rs.l   1
st_OutSize	rs.l    1
st_PTclen	rs.b    NT
		RSEVEN
st_Cclen	rs.b    NC
		RSEVEN
		rs.w    2*(NT-1)
st_PTtable	rs.w    1<<PTTABBITS
		rs.w    2*(NC-1)
st_Ctable	rs.w    1<<CTABBITS
st_SavedState	rs.l    14
st_SavedRetAddr	rs.l    1
st_SIZEOF	rs.b    0 ; Must be last

		rsreset
mt_LenCnt	rs.w    MAXBITS
mt_Next		rs.w    MAXBITS
mt_SIZEOF	rs.b    0	  ; Must be last

; d0 = NumSymbols
; d1 = TableBits
; a0 = CodeLen
; a1 = Table (mid)
MakeTable:
		movem.l d3-d4/a3-a4,-(sp) ; Other regs are preserved in Decompress
		lea     -mt_SIZEOF(sp),sp

		moveq   #0,d2
		move.w  #2*MAXBITS/4-1,d3
		lea     mt_LenCnt(sp),a3
		move.l  sp,a3
.clr
		move.l  d2,(a3)+
		dbf     d3,.clr

		;
		; Count code lengths
		;
		moveq   #0,d2
		moveq   #0,d3
.count
		move.b  (a0,d2.w),d3
		beq     .nextcount
		cmp.b   #16,d3
		bhi     Fatal
		add.w   d3,d3
		addq.w  #1,mt_LenCnt-2(sp,d3.w)
.nextcount
		addq.w  #1,d2
		cmp.w   d2,d0
		bne     .count

		;
		; Compute starting codes
		;
		moveq   #MAXBITS-2,d2
		lea     mt_Next(sp),a3
		clr.w   (a3)+
		lea     mt_LenCnt(sp),a4
		moveq   #0,d3 		; prev code
.calcnext
		add.w   (a4)+,d3	; prev code + prev lencnt
		add.w   d3,d3
		move.w  d3,(a3)+
		dbf     d2,.calcnext

		; After handling max length, next code should now be "$10000"
		add.w   (a4)+,d3
		add.w   d3,d3
		tst.w   d3
		bne     Fatal

		;
		; Clear table entries for tree roots
		;
		IFNE IS020
		move.w  mt_Next(sp,d1.w*2),d2
		ELSE
		move.w  d1,d2
		add.w   d2,d2
		move.w  mt_Next(sp,d2.w),d2
		ENDC
		lsr.w   #1,d2
		moveq   #0,d3
		bset.l  d1,d3
		sub.w   d2,d3
		subq.w  #1,d3
		bmi     .noclear
		add.w   d2,d2
		lea     (a1,d2.w),a3
		moveq   #0,d2
.cleartab
		move.w  d2,(a3)+
		dbf     d3,.cleartab
.noclear

		;
		; Build table
		;
		moveq   #0,d2		; symbol
		moveq   #0,d3		; avail
.tabloop
		moveq   #0,d4
		move.b  (a0)+,d4	; code length
		beq     .tabiter
		IFNE IS020
		lea     mt_Next-2(sp,d4.w*2),a3
		ELSE
		move.w  d4,d5
		add.w   d5,d5
		lea     mt_Next-2(sp,d5.w),a3
		ENDC
		move.w  (a3),d5		; code
		addq.w  #1,(a3)

		sub.w   d1,d4		; cl -= tbits
		bls     .fill		; cl <= tbits?

		; d4 = tlen
		move.w  d5,d6
		lsr.w   d4,d6		; code >> tlen
		IFNE IS020
		lea     (a1,d6.w*2),a4	; &table[code >> tlen]
		ELSE
		add.w   d6,d6
		lea     (a1,d6.w),a4	; &table[code >> tlen]
		ENDC
		moveq   #16,d6
		sub.w   d4,d6
		lsl.w   d6,d5		; code <<= 16 - tlen
		subq.w  #1,d4
.buildtree
		move.w  (a4),d6
		bne     .haveentry
		subq.w  #2,d3
		move.w  d3,(a4)
		move.w  d3,d6
		IFNE IS020
		clr.l   (a1,d6.w*2)
		ELSE
		move.w  d6,d7
		add.w   d7,d7
		clr.l   (a1,d7.w)
		ENDC
.haveentry
		add.w   d5,d5
		bcc     .left
		addq.w  #1,d6
.left
		IFNE IS020
		lea     (a1,d6.w*2),a4
		ELSE
		add.w   d6,d6
		lea     (a1,d6.w),a4
		ENDC
		dbf     d4,.buildtree
		move.w  d2,(a4)
		bra     .tabiter
.fill
		neg.w   d4
		lsl.w   d4,d5		; code <<= tbits-cl
		IFNE IS020
		lea     (a1,d5.w*2),a4	; &table[code]
		ELSE
		add.w   d5,d5
		lea     (a1,d5.w),a4	; &table[code]
		ENDC
		moveq   #0,d5
		bset.l  d4,d5		; 1 << (tbits-cl)
		subq.w  #1,d5
.fillloop
		move.w  d2,(a4)+
		dbf     d5,.fillloop
.tabiter
		addq.w  #1,d2
		cmp.w   d2,d0
		bne     .tabloop

		lea     mt_SIZEOF(sp),sp
		movem.l (sp)+,d3-d4/a3-a4
		rts

LoadData:       ; d4 = availbits-requested bits
		movem.l d0-d1/a0-a2,-(sp)
.moredata
		move.l  st_Context(a6),a0
		move.l  st_ReadCallback(a6),a2
		lea     -4(sp),sp
		move.l  sp,a1
		jsr     (a2)
		move.l  (sp)+,a3
		lea     (a3,d0.l),a4
		btst.l  #0,d0
		beq     .aligned
		; We require word aligned data, but there are two cases that are OK:
		; 1) We may require a tiny bit of extra (unused) bits at the end
		move.l  a3,d1
		btst.l  #0,d1		; Input buffer aligned?
		beq     .aligned	; OK - we'll allow it
		; 2) The very first byte can be unaligned
		cmp.b   #-16,d4
		bne     Fatal		; This is not the first (lha) block
		move.b  (a3)+,d3
		addq.b  #8,d4
		subq.l  #1,d0
		beq     .moredata	; Was last byte of first (input) block
.aligned
		movem.l (sp)+,d0-d1/a0-a2
		rts

GET_BITS	MACRO
		cmp.l   a4,a3
		bne     .havedata\@
		bsr     LoadData
.havedata\@
		swap    d3
		move.w  (a3)+,d3
		moveq   #16,d0		; d0 = 16 for DECODE_SYM
		add.b   d0,d4
		ENDM

; Decode symbol to d2, \1 = table bits, \2 = table register, \3 code len offset
DECODE_SYM      MACRO
		move.b  d4,d1
		sub.b   #\1,d1
		bpl     .havebits\@
		GET_BITS
		add.b   d0,d1
.havebits\@
		; peek \1 bits
		move.l  d3,d0
		lsr.l   d1,d0
		; lookup in table
		IFNE IS020
		move.w  (\2,d0.w*2),d2
		ELSE
		add.w   d0,d0
		move.w  (\2,d0.w),d2
		ENDC
		bpl     .hit\@
		cmp.b   #16,d4
		bhs     .tab\@
		GET_BITS
		add.b   d0,d1
		; navigate tree (since bit buffer has >= 16 bits, we can query it directly)
.tab\@
		subq.b  #1,d1
		btst.l  d1,d3
		beq     .left\@
		addq.w  #1,d2
.left\@
		IFNE IS020
		move.w  (\2,d2.w*2),d2
		ELSE
		add.w   d2,d2
		move.w  (\2,d2.w),d2
		ENDC
		bmi     .tab\@
.hit\@
		; drop bits from buffer
		sub.b   \3(a6,d2.w),d4
		moveq   #0,d0
		bset.l  d4,d0
		subq.l  #1,d0
		and.l   d0,d3
.out\@
		ENDM

DECODE_C	MACRO
		DECODE_SYM CTABBITS,a5,st_Cclen
		ENDM

DECODE_PT       MACRO
		DECODE_SYM PTTABBITS,\1,st_PTclen
		ENDM

ReadPT:
		move.l  d0,d5		; nbit
		move.l  d1,d6		; "special"
		lea     st_PTclen(a6),a5

		moveq   #0,d0
		move.l  a5,a0
		moveq   #NT-1,d1
.clear
		move.b  d0,(a0)+
		dbf     d1,.clear

		move.l  d5,d0
		bsr     ReadBits
		tst.w   d0
		bne     .notempty
		move.l  d5,d0
		bsr     ReadBits
		lea     st_PTtable(a6),a0
		move.w  #(1<<PTTABBITS)-1,d1
.empty
		move.w  d0,(a0)+
		dbf     d1,.empty
		rts
.notempty
		move.l  d0,d5		; "n"
		moveq   #NT,d0
		cmp.w   d0,d5
		bls     .readiter
		move.w  d0,d5
		bra     .readiter
.readloop
		moveq   #3,d0
		bsr     ReadBits
		cmp.b   #7,d0
		bne     .gotcount
		moveq   #6,d2
.count
		addq.b  #1,d2
		bsr     ReadBit
		tst.b   d0
		bne     .count
		move.b  d2,d0
.gotcount
		move.b  d0,(a5)+
		subq.w  #1,d6
		bne     .readiter
		moveq   #2,d0
		bsr     ReadBits
		sub.w   d0,d5
		bmi     Fatal
		add.w   d0,a5
.readiter
		dbf     d5,.readloop
.mktab
		moveq   #NT,d0
		moveq   #PTTABBITS,d1
		lea     st_PTclen(a6),a0
		lea     st_PTtable(a6),a1
		bra     MakeTable

ReadC:
		lea     st_Cclen(a6),a5
		lea     st_PTtable(a6),a2
		move.l  a5,a0
		moveq   #0,d0
		move.w  #NC/2-1,d1
.clear
		move.w  d0,(a0)+
		dbf     d1,.clear

		moveq   #CBIT,d0
		bsr     ReadBits
		move.l  d0,d5		; "n"
		bne     .notempty
		moveq   #CBIT,d0
		bsr     ReadBits
		lea     st_Ctable(a6),a0
		move.w  #(1<<CTABBITS)-1,d1
.empty
		move.w  d0,(a0)+
		dbf     d1,.empty
		rts
.notempty
		move.w  #NC,d0
		cmp.w   d0,d5
		bls     .oksize
		move.w  d0,d5
.oksize
		move.l  d5,d6
		bra     .readiter
.readloop
		DECODE_PT a2
		dbf     d2,.not0
		moveq   #1,d0
		bra     .skip
.not0
		dbf     d2,.not1
		moveq   #4,d0
		bsr     ReadBits
		addq.w  #3,d0
		bra     .skip
.not1
		tst.b   d2
		bne     .normal
		moveq   #CBIT,d0
		bsr     ReadBits
		add.w   #20,d0
.skip
		sub.w   d0,d5
		bmi     Fatal
		add.w   d0,a5
		bra     .readloop
.normal
		move.b  d2,(a5)+
.readiter
		dbf     d5,.readloop
.mktab
		move.l  d6,d0
		moveq   #CTABBITS,d1
		lea     st_Cclen(a6),a0
		lea     st_Ctable(a6),a1
		bra     MakeTable

Fatal:
		moveq   #-1,d0
Exit:
		move.l  st_OldStack(a6),sp
		movem.l (sp)+,d2-d7/a2-a5
		rts

ReadBit:
		moveq   #1,d0
ReadBits:
		sub.b   d0,d4
		bpl     .havebits
		GET_BITS
.havebits
		move.l  d3,d0
		lsr.l   d4,d0
		moveq   #0,d1
		bset.l  d4,d1
		subq.l  #1,d1
		and.l   d1,d3
		rts

Unlha::
_Unlha::
		movem.l d2-d7/a2-a5,-(sp)
		move.l  sp,st_OldStack(a6)
		move.l  a0,st_Context(a6)
		move.l  a1,st_ReadCallback(a6)
		move.l  a2,st_WriteCallback(a6)
		move.l  d0,st_OutSize(a6)
		move.l  a3,a2		; Window
		moveq   #0,d3		; BitBuf
		moveq   #0,d4		; BitBufBits
		sub.l   a3,a3		; InBufPtr
		sub.l   a4,a4		; InBufEnd
		lea     st_Ctable(a6),a5
		;moveq   #0,d5		; BlockSize
		moveq   #0,d6		; OutPos
		moveq   #0,d7
		bset.l  d1,d7
		subq.l  #1,d7		; WindowMask
		bra     .readheader
.loop
		DECODE_C
		cmp.w   #256,d2
		bhs     .match
		move.l  d6,d0
		and.l   d7,d0
		beq     .flush0
.store0
		move.b  d2,(a2,d0.l)
		addq.l  #1,d6
		dbf     d5,.loop
		bra     .readheader
.ofs1
		moveq   #0,d0
		bra     .haveoffset
.match
		sub.w   #256-2,d2	; Length (-1)
		move.l  d2,a1		; Save length

		lea     st_PTtable(a6),a0
		DECODE_PT a0
		subq.w  #1,d2
		bmi     .ofs1
		sub.b   d2,d4
		bpl     .havebits
		GET_BITS
.havebits
		move.l  d3,d0
		lsr.l   d4,d0
		moveq   #0,d1
		bset.l  d4,d1
		subq.l  #1,d1
		and.l   d1,d3
		bset.l  d2,d0
.haveoffset
		move.l  a1,d2		; Restore length

		sub.l   d6,d0
		not.l   d0
		bmi     .spaces

		; Check if copy can be done without running into edge cases

		and.l   d7,d0
		lea     (a2,d0.l),a0
		move.l  d0,d1
		add.l   d2,d1
		cmp.l   d7,d1
		bhi     .copymatchslow

		move.l  d6,d1
		and.l   d7,d1
		beq     .copymatchslow
		lea     (a2,d1.l),a1
		add.l   d2,d1
		cmp.l   d7,d1
		bhi     .copymatchslow

		add.l   d2,d6
		addq.l  #1,d6
.copymatchfast
		move.b  (a0)+,(a1)+
		dbf     d2,.copymatchfast
		dbf     d5,.loop
		bra     .readheader

.copymatchslow
		and.l   d7,d0

		move.l  d6,d1
		and.l   d7,d1
		beq     .flush1
.store1
		move.b  (a2,d0.l),(a2,d1.l)
		addq.l  #1,d6
		addq.w  #1,d0
		dbf     d2,.copymatchslow
		dbf     d5,.loop
.readheader
		cmp.l   st_OutSize(a6),d6
		beq     .done
		bhi     Fatal
		moveq   #16,d0
		bsr     ReadBits
		move.w  d0,d5
		subq.w  #1,d5

		movem.l d5-d7/a2/a5,-(sp)	; Note below btst if changing
		moveq   #TBIT,d0
		moveq   #3,d1
		bsr     ReadPT
		bsr     ReadC
		moveq   #4,d0
		;btst.l  #13,d7 ; d7 could be trashed!
		btst.b  #13-8,10(sp)
		beq     .smallwindow
		addq.w  #1,d0		; 5 bits when WindowBits > 13
.smallwindow
		moveq   #-1,d1
		bsr     ReadPT
		movem.l (sp)+,d5-d7/a2/a5
		bra     .loop
.done
		move.l  d6,d0
		and.l   d7,d0
		bne     .finalbuf
		; The window was exactly filled
		move.l  d7,d0
		addq.l  #1,d0
.finalbuf
		move.l  st_Context(a6),a0
		move.l  a2,a1
		move.l  st_WriteCallback(a6),a2
		jsr     (a2)
		move.l  d6,d0
		movem.l (sp)+,d2-d7/a2-a5
		rts
.flush0
		pea     .store0(pc)
		bra     .flush
.flush1
		pea     .store1(pc)
		; fall through
.flush
		tst.l   d6
		beq     .noflush
		; Make sure not call Write Callback in case of overrun
		cmp.l   st_OutSize(a6),d6
		bhi     Fatal
		movem.l d0-d1/a0-a2,-(sp)
		move.l  d7,d0
		addq.l  #1,d0
		move.l  st_Context(a6),a0
		move.l  a2,a1
		move.l  st_WriteCallback(a6),a2
		jsr     (a2)
		tst.l   d0
		movem.l (sp)+,d0-d1/a0-a2
		bne     .noflush
		movem.l d0-d7/a0-a5,st_SavedState(a6)
		move.l  (sp),st_SavedRetAddr(a6)
		moveq   #-2,d0
		bra     Exit
.noflush
		rts
		; The dictionary is intially filled with spaces...
		; Amiga LHA doesn't seem to use it, and should be quite rare
		; so not really optimized much
.spaces
		add.w   d0,d2		; update length
		bmi     Fatal
.sploop
		move.l  d6,d1
		and.l   d7,d1
		beq     .spflush
.spresume
		move.b  #' ',(a2,d1.l)
		addq.l  #1,d6
		addq.l  #1,d0
		bne     .sploop
		tst.w   d2
		bpl     .copymatchslow
		dbf     d5,.loop
		bra     .readheader
.spflush
		pea     .spresume(pc)
		bra     .flush

UnlhaResume::
_UnlhaResume::
		movem.l d2-d7/a2-a5,-(sp)
		move.l  sp,st_OldStack(a6)
		movem.l st_SavedState(a6),d0-d7/a0-a5
		move.l  st_SavedRetAddr(a6),-(sp)
		rts

UnlhaStateSize::
_UnlhaStateSize::
		move.l  #st_SIZEOF,d0
		rts

; vim: set softtabstop=0 tabstop=8 noexpandtab:
