/**
  ******************************************************************************
  * @file    audio_processing.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @version V2.0.0
  * @author  Mariano Marufo da Silva
	* @email 	 mmarufodasilva@est.frba.utn.edu.ar
  ******************************************************************************
	*/
	
#ifndef RECOGNITION_H
#define RECOGNITION_H

/* Includes ------------------------------------------------------------------*/
#include "arm_math.h"
#include "ff.h"
#include "macrosHMM.h"

//---------------------------------------------------------------------------------
//															HMM FUNCTIONS
//---------------------------------------------------------------------------------

float32_t Tesis_loglik(float32_t * data, uint16_t T, const  float32_t * transmat1, const  float32_t * transmat2, const  float32_t * mixmat, const  float32_t * mu, const  float32_t * logDenom, const  float32_t * invSigma);

void Tesis_mixgauss_logprob(float32_t * data, const  float32_t * mu, const  float32_t * logDenom, const  float32_t * invSigma, const  float32_t * mixmat, float32_t * B, uint16_t T);

float32_t Tesis_gaussian_logprob(float32_t * data, const  float32_t * mu, const  float32_t logDenom, const  float32_t * invSigma);

float32_t Tesis_forward(const  float32_t * transmat1, const  float32_t * transmat2, float32_t * alphaB, uint16_t T);

float32_t Tesis_lognormalise(float32_t * A, uint16_t T);




#endif // RECOGNITION_H
