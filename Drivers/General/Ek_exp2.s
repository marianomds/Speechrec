;  exp2_5q27:  32 bit exp2 function for ARM Cortex-M3                   KEIL
; --------------------------------------------------------------------------
; (c) 2010 Ivan Mellen                                            April 2010 
; --------------------------------------------------------------------------
; Free Personal, Non-Commercial Use License. 
; The Software is licensed to you for your personal, NON-COMMERCIAL USE.
; If you have questions about this license, need other sizes or would like
; a different license please email : imellen(at)embeddedsignals(dot)com


; This file contains only one function:

; unsigned int exp2_5q27( int x);  //y=2^x

; input   5q27 signed   -15.99999 to 15.99999
; output 16q16 unsigned   1/65536 to 65535+65535/65536  

; exp2_5q27(0x70123456)=0x40654536;  e.g.  exp2_5q27(14.0088888853788) = 16485.2703552246  (ye)
; correct value y is 16485.2584566925   error is (ye-y)/y = 7.21 E-7


; max error: +-3 E-6  = +-3 ppm   (calculated - correct)/correct
; 1+10 cycles + call overhead

; Power with base B:    B^x=exp2(k*x)
;  where k= log2(B);  ; for 10^x: k=3.32192809;      for e^x: k=1.44269504


         THUMB
	     AREA    FFTLIB2_CORTEXM3, CODE, READONLY        
	     EXPORT  exp2_5q27
	     ALIGN 8

exp2_5q27

            adr r12, LUTexp2        ;value not changed, reused in batch processing
            
            asrs r1, r0, #0x1B      ;core  9 instructions, 10 cycles
            rsb.w r1, r1, #0x0000000F
            ubfx r3, r0, #0x04, #0x10
            ubfx r0, r0, #0x14, #0x07
            ldr.w r0, [r12, r0, lsl #0x2]
            ubfx r2, r0, #0x00, #0x0C
            mul r2, r3
            adds.w r0, r0, r2, lsr #0x03
            lsrs r0, r1

            bx lr  ;only r0-r3, r12 modified
        
            ALIGN 2
            
LUTexp2 

  DCD 0x7ffff58f,0x80b1e597,0x8164c59f,0x821895a7,0x82cd75af,0x838355b6,0x843a25be,0x84f1f5c6
  DCD 0x85aab5ce,0x866485d6,0x871f55df,0x87db25e7,0x8897f5ef,0x8955e5f7,0x8a14c600,0x8ad4b608
  DCD 0x8b95b610,0x8c57b619,0x8d1ad621,0x8ddef62a,0x8ea42632,0x8f6a763b,0x9031d643,0x90fa464c
  DCD 0x91c3c655,0x928e665e,0x935a1667,0x9426e670,0x94f4e678,0x95c3f681,0x9694268a,0x97656694
  DCD 0x9837d69d,0x990b76a6,0x99e036af,0x9ab626b8,0x9b8d26c2,0x9c6566cb,0x9d3ec6d5,0x9e1956de
  DCD 0x9ef516e8,0x9fd216f1,0xa0b036fb,0xa18f9705,0xa270370e,0xa3520718,0xa4350722,0xa519472c
  DCD 0xa5fec736,0xa6e58740,0xa7cd874a,0xa8b6c754,0xa9a1475e,0xaa8d0769,0xab7a2773,0xac68877d
  DCD 0xad582788,0xae492792,0xaf3b679d,0xb02ef7a7,0xb123d7b2,0xb21a17bd,0xb311b7c7,0xb40a97d2
  DCD 0xb504d7dd,0xb60077e8,0xb6fd77f3,0xb7fbd7fe,0xb8fb9809,0xb9fcc814,0xbaff481f,0xbc03382b
  DCD 0xbd088836,0xbe0f4842,0xbf17884d,0xc0211859,0xc12c3864,0xc238b870,0xc346b87c,0xc4562887
  DCD 0xc5671893,0xc679789f,0xc78d58ab,0xc8a2c8b7,0xc9b9a8c3,0xcad218cf,0xcbebf8dc,0xcd0778e8
  DCD 0xce2478f4,0xcf42f901,0xd063290d,0xd184c91a,0xd2a7f927,0xd3ccd933,0xd4f34940,0xd61b494d
  DCD 0xd744e95a,0xd8701967,0xd99cf974,0xdacb7981,0xdbfba98e,0xdd2d699c,0xde60d9a9,0xdf95f9b6
  DCD 0xe0ccc9c4,0xe20549d1,0xe33f69df,0xe47b49ed,0xe5b8e9fb,0xe6f84a08,0xe8395a16,0xe97c2a24
  DCD 0xeac0aa33,0xec06fa41,0xed4f1a4f,0xee98fa5d,0xefe49a6c,0xf1321a7a,0xf2815a89,0xf3d27a97
  DCD 0xf5255aa6,0xf67a2ab5,0xf7d0bac4,0xf9293ad3,0xfa839ae2,0xfbdfcaf1,0xfd3deb00,0xfe9dfb0f
  
  END