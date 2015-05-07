/**
  ******************************************************************************
  * @file    msic.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    06-Mayo-2015
  * @brief   Some miscellaneous functions
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/  

#ifndef _MISC_H
#define _MISC_H

#include "arm_math.h"
#include "ff.h"

/**
	* @brief
	* @param
	* @param
	*/
char *		updateFilename 						(char *Filename);

FRESULT open_append (FIL* fp, const char* path);
//---------------------------------------
//						MATH FUNCTIONS
//---------------------------------------
/**
  * @brief  Calcula la derivada del vector. El vector destino debería tener tamaño blockSize-1
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void 			arm_diff_f32 		(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void			cumsum 					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void			sumlog					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
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


#endif // _MISC_H
