/**
  ******************************************************************************
  * @file    auido.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    05-Agosto-2014
  * @brief   STM32F4 Discovery Board Libraries
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */   

/* Includes ------------------------------------------------------------------*/
#include "ALE_stm32f4_discovery.h"


GPIO_TypeDef* GPIO_PORT[LEDn] = {LED4_GPIO_PORT, 
                                 LED3_GPIO_PORT, 
                                 LED5_GPIO_PORT,
                                 LED6_GPIO_PORT};
const uint16_t GPIO_PIN[LEDn] = {LED4_PIN, 
                                 LED3_PIN, 
                                 LED5_PIN,
                                 LED6_PIN};
GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {USER_BUTTON_GPIO_PORT}; 
const uint16_t BUTTON_PIN[BUTTONn] = {USER_BUTTON_PIN}; 




/*------------------------------------------------------------------------------
												LEDS AND BUTTON DRIVERS CONTROL FUNCTIONS
------------------------------------------------------------------------------*/
/**
  * @brief  Turns selected LED On.
  * @param  Led: Specifies the Led to be set on. 
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6  
  * @retval None
  */
void LED_On(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_SET); 
}
/**
  * @brief  Turns selected LED Off.
  * @param  Led: Specifies the Led to be set off. 
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6 
  * @retval None
  */
void LED_Off(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET); 
}
/**
  * @brief  Toggles the selected LED.
  * @param  Led: Specifies the Led to be toggled. 
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6  
  * @retval None
  */
void LED_Toggle(Led_TypeDef Led)
{
  HAL_GPIO_TogglePin(GPIO_PORT[Led], GPIO_PIN[Led]);
}
/**
  * @brief  Returns the selected Button state.
  * @param  Button: Specifies the Button to be checked.
  *   This parameter should be: BUTTON_KEY  
  * @retval The Button GPIO pin value.
  */
uint32_t PB_GetState(Button_TypeDef Button)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT[Button], BUTTON_PIN[Button]);
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	User_Button_EXTI();
}

__weak void User_Button_EXTI (void) {
	  /* This function should be implemented by the user application.
     It is called into this driver when the current buffer is filled
     to prepare the next buffer pointer and its size. */
}
