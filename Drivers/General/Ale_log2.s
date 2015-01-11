;/*
;* @file, auido.c
;* @author  Alejandro Alvarez
;* @version V1.0.0
;* @date, 05-Agosto-2014
;* @brief,I2S Audio recorder program.
;*/


		AREA    FFTLIB2_CORTEXM3, CODE, READONLY        
	    EXPORT  log2_16q16
        ALIGN 32

;	FLOATING POINT STRUCTURE
;
;			31				30 - 23							22 - 0	
;	*********************************************************************************
;	*	SIGN(1 bit)	*	EXPONENT(8 bits)	*			MANTIZA (23bits)			*
;	*********************************************************************************

	MANT	EQU		0x007FFFFF
	EXPN	EQU		0x007FFFFF
log2_16q16:
	
		AND		R10,R12,MATIZA
		AND		R11,R12,MATIZA
		VMUL.F32	R0,R0				; mant^2
		VMUL	R0,R0				; mant^4
		VMUL	R0,R0				; mant^8
		VMUL	R0,R0				; mant^16
		VMUL	R0,R0				; mant^32
		VMUL	R0,R0				; mant^64
		VMUL	R0,R0				; mant^128


;;*************************************************************
;;*		FAST logarithm for FFT displays 					*
;;* >>>> NEED ONLY ADD ONE INSTRUCTION IN MANY CASES <<<< 	*
;;*************************************************************

	;|| 		||				;
	;VMUL	REAL,REAL,R0 	; calculate the magnitude
	;VMUL	IMAG,IMAG,R1 	; Note: sign bit is zero
	;ADDF	R1,R0			;
	;ASH 	-1,R0			; <- One instruction logarithm!
	;STF		R0,OUT			; scaled externally in DAC
	;||		||				;

;;*************************************************************
;;* _log_E.asm							  DEVICE: TMS320C30 *
;;*************************************************************

;.global _log_E
;_log_E:
	;POP	AR1		; return address -> AR1
	;POPF 	R0		; X -> R0
	;LDF	R0,R1	; use R1 to accumulate answer

	;LDI 	2,RC 	; repeat 3x
	;RPTB 	loop 	; 8 + 13*3 + 9 ASH 7,R1 ;
	;VPUSH	1.0,R0 	; EXP = 0
	;VMUL	R0,R0	; mant^2
	;VMUL	R0,R0	; mant^4
	;VMUL	R0,R0	; mant^8
	;VMUL	R0,R0	; mant^16
	;VMUL	R0,R0	; mant^32
	;VMUL	R0,R0	; mant^64
	;VMUL	R0,R0	; mant^128
	;PUSHF	R0		; offload 7 bits of exponent
	;POP	R3		;
	;ASH 	-24,R3	; remove mantissa
	
;loop:
	;OR		R3,R1	; R2 accumulates EXP <log2(man)>
	;ASH	11,R1	; Jam mant_R1 to top
					;; (concat. EXP_old)
	;ASH	-20,R0	; align and append the
					;; MSBs of mant_R0
	;OR		R0,R1	; (accurate to 3 bits)
	;PUSHF	R1		; PUSH EXP and Mantissa
					;; (sign is now data!)
	;POP	R0		; POP as integer (EXP+FRACTION)
	;BD 	AR1		;
	;FLOAT 	R0		; convert EXP+FRACTION to float
	;VMUL 	@CONST,R0 ; scale the result by 2^-24 ; and change base
	;ADDI	1,SP	; restore stack pointer
	;.data
;CONST_ADR: .word CONST
;CONST	.long 0e7317219h	;Base e hand calc w/1 lsb round
		;.end
		
		
		
		