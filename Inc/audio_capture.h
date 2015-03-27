/**
  ******************************************************************************
  * @file    audio_capture.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/
	
#ifndef AUDIO_H
#define AUDIO_H

#include "ALE_stm32f4_discovery.h"
//#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "ff.h"
#include "string.h"
//#include "stdlib.h"
#include "error_handler.h"


typedef struct{	
	uint16_t audio_freq;						//[Hz]
	uint16_t audio_bw_high;					//[Hz]
	uint16_t audio_bw_low;					//[Hz]
	
	uint8_t audio_bit_resolution;
	uint8_t audio_channel_nbr;			// Mono = 1, Stereo = 2
	uint8_t audio_volume;
	uint8_t audio_decimator;
}Auido_Capture_Config;

	

typedef struct {
  int32_t offset;
  uint32_t fptr;
}Audio_BufferTypeDef;

uint32_t initCapture(const Auido_Capture_Config* config, uint16_t** data, uint16_t buff_size);
void audioRecord	(void);
void audioPause		(void);
void audioResume	(void);
void audioStop		(void);


#endif  // AUDIO_H
