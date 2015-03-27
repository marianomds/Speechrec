/**
  ******************************************************************************
  * @file    audio_processing.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/

/* Includes ------------------------------------------------------------------*/
#include "error_handler.h"

void Error_Handler(void) {
	int i;
	
  while(1){
		LED_Toggle(RLED);
		LED_Toggle(GLED);
		LED_Toggle(BLED);
		LED_Toggle(OLED);
			
		for(i=1000000; i ; i--);
//		osDelay(100);
	}
}
