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
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "ff.h"
#include "arm_math.h"
#include "float.h"
#include "usb_host.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "recognition_specific.h"
#include "error_handler.h"


enum Path{
	DIAGONAL = 0,
	VERTICAL = 1,
	HORIZONTAL = 2,
};

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
float32_t dtw  (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path);
float32_t dtw_reduce (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path);

/**
  * @brief  Inicializa la función distancia
  * @param	[in] Tamaño de los vectores a comparar
  */
void init_dist (uint32_t blockSize);
/**
  * @brief  De-inicializa la función de distancia
  */
void deinit_dist (void);
/**
  * @brief  Calcula la distancia entre dos elementos
  * @param	[in] points to the first input vector 
	* @param	[in] points to the second input vector 
	* @retval resultado
  */
float32_t dist (float32_t *pSrcA, float32_t *pSrcB);


//---------------------------------------
//-				Math Support Functions				-
//---------------------------------------
/**
	* @brief Function to find minimum of x and y
	* @param x value
	* @param y value
	* @retval minimun value
	*/ 
int min(int x, int y);
/**
	* @brief Function to find maximum of x and y
	* @param x value
	* @param y value
	* @retval maximum value
	*/ 
int max(int x, int y);

#endif // RECOGNITION_H
