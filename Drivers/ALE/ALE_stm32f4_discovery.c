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
/*------------------------------------------------------------------------------
												AUDIO DRIVER CONTROL FUNCTIONS
------------------------------------------------------------------------------*/

/**
  * @brief  Initialize the PDM library.
  * @param  AudioFreq: Audio sampling frequency
  * @param  ChnlNbr: Number of audio channels (1: mono; 2: stereo)
  * @retval None
  */
void PDMDecoder_Init(uint32_t AudioFreq, uint32_t ChnlNbr, PDMFilter_InitStruct *Filter)
{ 
  uint32_t i = 0;
	
  /* Enable CRC peripheral to unlock the PDM library */
  __CRC_CLK_ENABLE();
  
  for(i = 0; i < ChnlNbr; i++)
  {
    /* Filter LP and HP Init */
    Filter[i].LP_HZ = AudioFreq/2;
    Filter[i].HP_HZ = 10;
    Filter[i].Fs = AudioFreq;
    Filter[i].Out_MicChannels = ChnlNbr;
    Filter[i].In_MicChannels = ChnlNbr; 
    PDM_Filter_Init((PDMFilter_InitStruct *)&Filter[i]);
  }
}

/**
  * @brief  Starts audio recording.
  * @param  pbuf: Main buffer pointer for the recorded data storing  
  * @param  size: Current size of the recorded buffer
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t AUDIO_IN_Record(uint16_t* pbuf, uint32_t size)
{
  uint32_t ret = AUDIO_ERROR;
  
  /* Start the process receive DMA */
  HAL_I2S_Receive_DMA(&hi2s2, pbuf, size);
  
  /* Return AUDIO_OK when all operations are correctly done */
  ret = AUDIO_OK;
  
  return ret;
}
/**
  * @brief  Stops audio recording.
  * @param  None
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t AUDIO_IN_Stop(void)
{
  uint32_t ret = AUDIO_ERROR;
  
  /* Call the Media layer pause function */
  HAL_I2S_DMAStop(&hi2s2);  
  
  /* Return AUDIO_OK when all operations are correctly done */
  ret = AUDIO_OK;
  
  return ret;
}

/**
  * @brief  Pauses the audio file stream.
  * @param  None
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t AUDIO_IN_Pause(void)
{    
  /* Call the Media layer pause function */
  HAL_I2S_DMAPause(&hi2s2);
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Resumes the audio file stream.
  * @param  None    
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t AUDIO_IN_Resume(void)
{    
  /* Call the Media layer pause/resume function */
  HAL_I2S_DMAResume(&hi2s2);
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Converts PDM audio format from to PCM.
  * @param  PDMBuf: Pointer to data PDM buffer
  * @param  PCMBuf: Pointer to data PCM buffer
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t AUDIO_IN_PDM2PCM(uint16_t *PDMBuf, uint32_t PDMsize, uint16_t *PCMBuf, uint8_t channel, uint8_t volume, PDMFilter_InitStruct *Filter)
{
  uint16_t AppPDM[PDMsize/2];
  uint32_t index = 0;
  
  /* Swappea los Bytes */
  for(index = 0; index<PDMsize/2; index++)
  {
    AppPDM[index] = HTONS(PDMBuf[index]);
  }
  
  for(index = 0; index < channel; index++)
  {
		/* PDM to PCM filter */ 
//		#if defined __DECIMATOR_LSB
//			#if defined ( __DECIMATOR_64 )
				PDM_Filter_64_LSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#elif defined ( __DECIMATOR_80 )
//				PDM_Filter_80_LSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#elif defined ( __DECIMATOR_128 )
//				PDM_Filter_128_LSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#else
//			#error "No Decimator number defined"
//			#endif
//		#elif defined __DECIMATOR_MSB
//			#if defined __DECIMATOR_64
//				PDM_Filter_64_MSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#elif defined __DECIMATOR_80
//				PDM_Filter_80_MSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#elif	defined __DECIMATOR_128
//				PDM_Filter_128_MSB((uint8_t*)&AppPDM[index], (uint16_t*)&(PCMBuf[index]), volume , (PDMFilter_InitStruct *)&Filter[index]);
//			#else
//			#error "No Decimator number defined"
//			#endif
//		#else
//		#error "No Decimator number defined"
//		#endif
  }
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK; 
}
/**
  * @brief  Rx Transfer completed callbacks
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  /* Call the record update function to get the next buffer to fill and its size (size is ignored) */
  AUDIO_IN_TransferComplete_CallBack();
}

/**
  * @brief  Rx Half Transfer completed callbacks.
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  /* Manage the remaining file size and new address offset: This function 
     should be coded by user (its prototype is already declared in stm32f4_discovery_audio.h) */
  AUDIO_IN_HalfTransfer_CallBack();
}

/**
  * @brief  Error Transfer callbacks.
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
  /* Manage the remaining file size and new address offset: This function 
     should be coded by user (its prototype is already declared in stm32f4_discovery_audio.h) */
  AUDIO_IN_Error_CallBack();
}

/**
  * @brief  User callback when record buffer is filled.
  * @param  None
  * @retval None
  */
__weak void AUDIO_IN_TransferComplete_CallBack(void)
{
  /* This function should be implemented by the user application.
     It is called into this driver when the current buffer is filled
     to prepare the next buffer pointer and its size. */
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @param  None
  * @retval None
  */
__weak void AUDIO_IN_HalfTransfer_CallBack(void)
{
  /* This function should be implemented by the user application.
     It is called into this driver when the current buffer is filled
     to prepare the next buffer pointer and its size. */
}

/**
  * @brief  Audio IN Error callback function.
  * @param  None
  * @retval None
  */
__weak void AUDIO_IN_Error_Callback(void)
{   
  /* This function is called when an Interrupt due to transfer error on or peripheral
     error occurs. */
}
