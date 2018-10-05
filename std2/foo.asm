	TITLE	test.c
	.386P
include listing.inc
if @Version gt 510
.model FLAT
else
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT DWORD USE32 PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT DWORD USE32 PUBLIC 'BSS'
_BSS	ENDS
_TLS	SEGMENT DWORD USE32 PUBLIC 'TLS'
_TLS	ENDS
FLAT	GROUP _DATA, CONST, _BSS
	ASSUME	CS: FLAT, DS: FLAT, SS: FLAT
endif
PUBLIC	_main
EXTRN	_printf:NEAR
_DATA	SEGMENT
$SG33	DB	'%08X, %08X', 00H
	ORG $+1
$SG35	DB	'%08X, %08X', 00H
_DATA	ENDS
_TEXT	SEGMENT
_x$ = -4
_main	PROC NEAR
; File test.c
; Line 2
	push	ebp
	mov	ebp, esp
	push	ecx
	push	ebx
	push	esi
	push	edi
; Line 3
	mov	DWORD PTR _x$[ebp], -15			; fffffff1H
; Line 5
	mov	eax, DWORD PTR _x$[ebp]
	sar	eax, 1
	push	eax
	mov	ecx, DWORD PTR _x$[ebp]
	push	ecx
	push	OFFSET FLAT:$SG33
	call	_printf
	add	esp, 12					; 0000000cH
; Line 6
	mov	edx, DWORD PTR _x$[ebp]
	shr	edx, 1
	push	edx
	mov	eax, DWORD PTR _x$[ebp]
	push	eax
	push	OFFSET FLAT:$SG35
	call	_printf
	add	esp, 12					; 0000000cH
; Line 17
	push	ax
; Line 18
	mov	ah, 0
; Line 19
	mov	al, 19					; 00000013H
; Line 20
	int	16					; 00000010H
; Line 21
	pop	ax
; Line 23
	pop	edi
	pop	esi
	pop	ebx
	mov	esp, ebp
	pop	ebp
	ret	0
_main	ENDP
_TEXT	ENDS
END
