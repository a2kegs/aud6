
zp0	equ	$00
zp1	equ	$01	; and $02,$03
zp4	equ	$04	; and $05

	org	$8000

start
	jmp	real_start
constant
* Copy the value at zp0 to (zp4) until $8000
	lda	zp0
	ldy	#$00
const_loop
	sta	(zp1),y
	iny
	bne	const_loop
	inc	zp1+1
	bpl	const_loop
	rts
; Fall through to real_write_bank

real_start
	jsr	$fe1f		; IDROUTINE, c=0 only on a IIgs
	bcs	nota2gs
	lda	$c036		; Get speed
	pha
	and	#$7f
	sta	$c036		; Force slow speed
nota2gs
*	lda	#$10
*	sta	zp1+1
	ldx	#$00
	stx	zp1
	stx	zp4
	stx	zp0
	ldy	#$00
	php
	sei
	jmp	nextsamplo

real_stop
	plp
	jsr	$fe1f		; IDROUTINE, c=0 only on a IIgs
	bcs	stop_nota2gs
	pla
	sta	$c036		; Restore speed
stop_nota2gs
	rts

*	ds	\		; page align
* Repeat lda #$a9 20 times
	lup	20
	lda	#$a9
	--^
	lda	#$a9		; -5,-4
	lda	$ea		; -3,-2
nextsamplo
	lda	(zp1),y		; 5 cycs
	and	#$fc		; 2
	sta	nextpatchlo+1	; 4
	iny			; 2 cycs
nextsamplo2
	bne	no_inclo	; 2/3
	inc	zp1+1		; 5
	bmi	stoplo		; 2/3
				; +22

nextpatchlo
	jmp	tablelo		; 3+3 
; as of here, 28 cycles

no_inclo
	sta	noincpatchlo+1	; 4
	nop			; 2
noincpatchlo
	jmp	tablelo		; 3+3


stoplo	jmp	real_stop

	lup	20
	lda	#$a9
	--^
	lda	#$a9		; -5,-4
	lda	$ea		; -3,-2
nextsamphi
	lda	(zp1),y		; 5 cycs
	and	#$fc		; 2
	sta	nextpatchhi+1	; 4
	iny			; 2 cycs
nextsamphi2
	bne	no_inchi	; 2/3
	inc	zp1+1		; 5
	bmi	stophi		; 2/3
				; +21

nextpatchhi
	jmp	tablehi		; 3+3
no_inchi
	sta	noincpatchhi+1	; 4
	nop			; 2
noincpatchhi
	jmp	tablehi		; 3+3

; as of here, 28 cycles


*	ldx	#0		; 2
*	sta	$c030		; 4
*	jmp	nextsamp	; 3
; total: 27+9=36

stophi	jmp	real_stop
; stop must be at least $08 from the "beq stop", but no more than $60

	put	gen_routs.s

