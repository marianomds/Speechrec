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
#include "stm32f4xx_hal.h"


/*------------------------------------------------------------------------------
											LEDS & BUTTON Driver Configuration parameters
------------------------------------------------------------------------------*/
typedef enum 
{
  LED4 = 0, GLED = LED4,
  LED3 = 1,	OLED = LED3,
  LED5 = 2,	RLED = LED5,
  LED6 = 3,	BLED = LED6,
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

#endif //_ALE_STM32F4_DISCOVERY_H
