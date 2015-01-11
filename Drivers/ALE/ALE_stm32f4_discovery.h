/**
  ******************************************************************************
  * @file    stm32f4_discovery_audio.h
  * @author  MCD Application Team
  * @version V2.0.2
  * @date    26-June-2014
  * @brief   This file contains the common defines and functions prototypes for
  *          stm32f4_discovery_audio.c driver.
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */ 
	

#ifndef _ALE_STM32F4_DISCOVERY_H
#define _ALE_STM32F4_DISCOVERY_H

/* Includes ------------------------------------------------------------------*/
//#include "string.h"
#include "i2s.h"
#include "pdm_filter.h"

/*------------------------------------------------------------------------------
											LEDS & BUTTON Driver Configuration parameters
------------------------------------------------------------------------------*/
typedef enum 
{
  LED4 = 0,
  GLED = 0,
	
  LED3 = 1,
	OLED = 1,
	
  LED5 = 2,
	RLED = 2,
	
  LED6 = 3,
  BLED = 3,

} Led_TypeDef;

#define LEDn                             4

#define LED4_PIN                         GPIO_PIN_12
#define LED4_GPIO_PORT                   GPIOD

#define LED3_PIN                         GPIO_PIN_13
#define LED3_GPIO_PORT                   GPIOD
  
#define LED5_PIN                         GPIO_PIN_14
#define LED5_GPIO_PORT                   GPIOD

#define LED6_PIN                         GPIO_PIN_15
#define LED6_GPIO_PORT                   GPIOD

void LED_On(Led_TypeDef Led);
void LED_Off(Led_TypeDef Led);
void LED_Toggle(Led_TypeDef Led);

typedef enum 
{  
  USER_BUTTON = 0,
} Button_TypeDef;

typedef enum 
{  
  BUTTON_MODE_GPIO = 0,
  BUTTON_MODE_EXTI = 1
} ButtonMode_TypeDef;    

#define BUTTONn													1
#define USER_BUTTON_PIN									GPIO_PIN_0
#define USER_BUTTON_GPIO_PORT          	GPIOA

uint32_t PB_GetState(Button_TypeDef Button);
void User_Button_EXTI (void);

/*------------------------------------------------------------------------------
											Audio Driver Configuration parameters
------------------------------------------------------------------------------*/


/* Audio status definition */     
enum {
	AUDIO_OK,
	AUDIO_ERROR,
	AUDIO_TIMEOUT
};

uint8_t AUDIO_IN_Record(uint16_t* pbuf, uint32_t size);
uint8_t AUDIO_IN_Stop(void);
uint8_t AUDIO_IN_Pause(void);
uint8_t AUDIO_IN_Resume(void);
uint8_t AUDIO_IN_PDM2PCM(uint16_t *PDMBuf, uint32_t PDMsize, uint16_t *PCMBuf, uint8_t channel,
												 uint8_t volume, PDMFilter_InitStruct *Filter);

void PDMDecoder_Init(uint32_t AudioFreq, uint32_t ChnlNbr, PDMFilter_InitStruct *Filter);

/* User Callbacks: user has to implement these functions in his code if they are needed. */
/* This function should be implemented by the user application.
   It is called into this driver when the current buffer is filled to prepare the next
   buffer pointer and its size. */
void    AUDIO_IN_TransferComplete_CallBack	(void);
void    AUDIO_IN_HalfTransfer_CallBack			(void);
void    AUDIO_IN_Error_CallBack							(void);

/* This function is called when an Interrupt due to transfer error on or peripheral
   error occurs. */
void    AUDIO_IN_Error_Callback(void);

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s);
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s);

#endif //_ALE_STM32F4_DISCOVERY_H
