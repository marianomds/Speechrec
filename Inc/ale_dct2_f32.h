/**
  ******************************************************************************
  * @file    ale_dct2_f32.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    22-Mayo-2015
  * @brief   Application Tasks for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */ 

#ifndef ALE_DCT2_F32_H
#define ALE_DCT2_F32_H

#include "arm_math.h"


/**
 * @brief Instance structure for the floating-point DCT4/IDCT4 function.
 */

typedef struct
{
	uint16_t N;                         /**< length of the DCT4. */
	uint16_t Nby2;                      /**< half of the length of the DCT4. */
	float32_t normalize;                /**< normalizing factor. */
	float32_t *pTwiddle;                /**< points to the twiddle factor table. */
	arm_rfft_fast_instance_f32 *pRfft;  /**< points to the real FFT instance. */
} ale_dct2_instance_f32;


arm_status dct2_init_f32(  ale_dct2_instance_f32 * S,  arm_rfft_fast_instance_f32 * S_RFFT, //  arm_cfft_radix4_instance_f32 * S_CFFT,
														uint16_t N,  uint16_t Nby2,  float32_t normalize);

void ale_dct2_f32(const ale_dct2_instance_f32 * S,  float32_t * pState,  float32_t * pInlineBuffer);

#endif		// ALE_DCT2_F32_H

