/* ----------------------------------------------------------------------    
* Copyright (C) 2010-2014 ARM Limited. All rights reserved.    
*    
* $Date:        31. July 2014 
* $Revision: 	V1.4.4  
*    
* Project: 	    CMSIS DSP Library    
* Title:	    arm_dct4_f32.c    
*    
* Description:	Processing function of DCT4 & IDCT4 F32.    
*    
* Target Processor: Cortex-M4/Cortex-M3/Cortex-M0
*  
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions
* are met:
*   - Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   - Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the 
*     distribution.
*   - Neither the name of ARM LIMITED nor the names of its contributors
*     may be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.      
* -------------------------------------------------------------------- */

#include "ale_dct2_f32.h"

/**    
 * @ingroup groupTransforms    
 */

/**    
 * @defgroup DCT4_IDCT4 DCT Type IV Functions    
 */

 /**    
 * @addtogroup DCT4_IDCT4    
 * @{    
 */

/**    
 * @brief Processing function for the floating-point DCT4/IDCT4.   
 * @param[in]       *S             points to an instance of the floating-point DCT4/IDCT4 structure.   
 * @param[in]       *pState        points to state buffer.   
 * @param[in,out]   *pInlineBuffer points to the in-place input and output buffer.   
 * @return none.   
 */

void ale_dct2_f32(
  const ale_dct2_instance_f32 * S,
  float32_t * pState,
  float32_t * pInlineBuffer)
{
  uint32_t i;                                    /* Loop counter */
  float32_t *weights = S->pTwiddle;              /* Pointer to the Weights table */
  float32_t *pS1, *pS2, *pbuff;                  /* Temporary pointers for input buffer and pState buffer */


  /* DCT2 computation (calculated using RFFT)       
   * Calculation of DCT2 using FFT is divided into three steps:    
   *                  Step1: Re-ordering of even and odd elements of input.    
   *                  Step2: Calculating FFT of the re-ordered input.    
   *                  Step3: Taking the real part of the product of FFT output and weights.    
   *                  Step4: Normalizing factor sqrt(2/N).    
   */

  /* ----------------------------------------------------------------    
   * Step1: Re-ordering of even and odd elements as,    
   *             pState[i] =  pInlineBuffer[2*i] and    
   *             pState[N-i-1] = pInlineBuffer[2*i+1] where i = 0 to N/2    
   ---------------------------------------------------------------------*/

  /* pS1 initialized to pState */
  pS1 = pState;

  /* pS2 initialized to pState+N-1, so that it points to the end of the state buffer */
  pS2 = pState + (S->N - 1u);

  /* pbuff initialized to input buffer */
  pbuff = pInlineBuffer;

  /* Initializing the loop counter to N/2 >> 2 for loop unrolling by 4 */
  i = (uint32_t) S->Nby2 >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
   ** a second loop below computes the remaining 1 to 3 samples. */
  do
  {
    /* Re-ordering of even and odd elements */
    /* pState[i] =  pInlineBuffer[2*i] */
    *pS1++ = *pbuff++;
    /* pState[N-i-1] = pInlineBuffer[2*i+1] */
    *pS2-- = *pbuff++;

    *pS1++ = *pbuff++;
    *pS2-- = *pbuff++;

    *pS1++ = *pbuff++;
    *pS2-- = *pbuff++;

    *pS1++ = *pbuff++;
    *pS2-- = *pbuff++;

    /* Decrement the loop counter */
    i--;
  } while(i > 0u);

  /* pbuff initialized to input buffer */
  pbuff = pInlineBuffer;

  /* pS1 initialized to pState */
  pS1 = pState;

  /* Initializing the loop counter to N/4 instead of N for loop unrolling */
  i = (uint32_t) S->N >> 2u;

  /* Processing with loop unrolling 4 times as N is always multiple of 4.    
   * Compute 4 outputs at a time */
  do
  {
    /* Writing the re-ordered output back to inplace input buffer */
    *pbuff++ = *pS1++;
    *pbuff++ = *pS1++;
    *pbuff++ = *pS1++;
    *pbuff++ = *pS1++;

    /* Decrement the loop counter */
    i--;
  } while(i > 0u);


  /* ---------------------------------------------------------    
   *     Step2: Calculate RFFT for N-point input    
   * ---------------------------------------------------------- */
  /* pInlineBuffer is real input of length N , pState is the complex output of length 2N */
  arm_rfft_fast_f32(S->pRfft, pInlineBuffer, pState, 0);

  /*----------------------------------------------------------------------    
	 *  Step3: Multiply the FFT output with the weights.    
	 *----------------------------------------------------------------------*/
  arm_cmplx_mult_cmplx_f32(pState, weights, pState, S->N);

  /* ----------- Post-processing ---------- */
	/*------------ Normalizing the output by multiplying with the normalizing factor ----------*/
  /* Getting only real part from the output */

  /* Initializing the loop counter to N/4 instead of N for loop unrolling */
  i = (uint32_t) S->N >> 2u;

  /* pbuff initialized to input buffer. */
  pbuff = pInlineBuffer;

  /* pS1 initialized to pState */
  pS1 = pState;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
   ** a second loop below computes the remaining 1 to 3 samples. */
  do
  {
    /* pState pointer (pS1) is incremented twice as the real values are located alternatively in the array */
    *pbuff++ = *pS1++ * S->normalize;
    /* points to the next real value */
    pS1++;

    *pbuff++ = *pS1++ * S->normalize;
    /* points to the next real value */
    pS1++;

    *pbuff++ = *pS1++ * S->normalize;
    /* points to the next real value */
    pS1++;

    *pbuff++ = *pS1++ * S->normalize;
    /* points to the next real value */
    pS1++;

    /* Decrement the loop counter */
    i--;
  } while(i > 0u);

	pInlineBuffer[0] *= sqrt(1.0f/S->N) / S->normalize;
}

/**    
   * @} end of DCT4_IDCT4 group    
   */
