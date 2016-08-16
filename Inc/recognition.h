/**
  ******************************************************************************
  * @file    audio_processing.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/
	
#ifndef RECOGNITION_H
#define RECOGNITION_H

/* Includes ------------------------------------------------------------------*/
#include "arm_math.h"
#include "stdbool.h"
#include "ff.h"
#include "macrosHMM.h"

//---------------------------------------------------------------------------------
//															HMM FUNCTIONS
//---------------------------------------------------------------------------------

float32_t Tesis_loglik(float32_t * data, uint16_t T, const  float32_t * transmat1, const  float32_t * transmat2, const  float32_t * mixmat, const  float32_t * mu, const  float32_t * Sigma);

void Tesis_mixgauss_logprob(float32_t * data, const  float32_t * mu, const  float32_t * Sigma, const  float32_t * mixmat, float32_t * B, uint16_t T);

float32_t Tesis_gaussian_logprob(float32_t * data, const  float32_t * mu, const  float32_t * Sigma);

float32_t Tesis_forward(const  float32_t * transmat1, const  float32_t * transmat2, float32_t * alphaB, uint16_t T);

float32_t Tesis_lognormalise(float32_t * A, uint16_t T);


/*
enum Path{
	DIAGONAL = 0,
	VERTICAL = 1,
	HORIZONTAL = 2,
};
*/

/**
	*\typedef
	*	\struct
  *	\brief Recognition configuration
	*/
/*typedef struct
{
	// DTW parallelogram width
	uint8_t width;
	
}Reco_conf;*/

/**
  * @brief  DTW Algorithm
	*
	* 		PARAMETERS IN EACH MATRIX
	*	Rows --> time movement
	*	Cols --> Parameters
	*
	* b0 a0	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b1 a1	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b2 a2	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b3 a3	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b4 a4	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b5 a5	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b6 a6	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b7 a7	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b8 a8	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	* b9 a9	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	*
	*
	*			REPRESENTATION OF A POSIBLE PATH
	*
	* 		a0	a1	a2	a3	a4	a5	a6	a7	a8	a9 ...
	* b0
	* b1																		 _____|
	* b2																_____|		
	* b3													 _____|
	* b4											_____|
	* b5								 _____|
	* b6						_____|
	* b7			 _____|
	* b8	_____|
	* b9	|
	*
	* @param  [in] a: Pointer to the matrix of one of the time series to compare
	* @param  [in] b: Pointer to the matrix of the other of the time series to compare
	* @param  [in] path: Pointer to the matrix where the path taken can be seen. Size of path must be length(a)+length(b). If it's not need pass NULL.
  * @retval 
  */
//float32_t dtw  (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, float32_t *dist_mtx);
//float32_t dtw_reduce (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, const bool save_dist_mtx, uint8_t width);
//float32_t dtw_files (FIL *a, FIL *b, const uint8_t parameters_size, const bool save_dist_mtx);
//float32_t dtw_files_reduce (FIL *a, FIL *b, const uint8_t parameters_size, const bool save_dist_mtx);

/**
  * @brief  Inicializa la función distancia
  * @param	[in] Tamaño de los vectores a comparar
  */
//void init_dist (uint32_t blockSize);
/**
  * @brief  De-inicializa la función de distancia
  */
//void deinit_dist (void);
/**
  * @brief  Calcula la distancia entre dos elementos
  * @param	[in] points to the first input vector 
	* @param	[in] points to the second input vector 
	* @retval resultado
  */
//float32_t dist (float32_t *pSrcA, float32_t *pSrcB);


#endif // RECOGNITION_H
