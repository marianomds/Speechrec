/**
  ******************************************************************************
  * @file    ALE_STM32F4_DISCOVERY_Audio_Input_Driver.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    26-Febrero-2015
  * @brief   STM32F4 Discovery Board Libraries
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */  
	

#ifndef ALE_STM32F4_DISCOVERY_AUDIO_H
#define ALE_STM32F4_DISCOVERY_AUDIO_H

/* Includes ------------------------------------------------------------------*/

#include "i2s.h"
#include "pdm_filter.h"
#include "error_handler.h"
#include "cmsis_os.h"


/**	
	* \enum
  *	\brief Audio status definition
	*/
enum {
	AUDIO_OK,						/*!< */
	AUDIO_ERROR,				/*!< */
	AUDIO_TIMEOUT				/*!< */
};

/**	
	* \enum
  *	\brief Audio status definition
	*/
enum {
	I2S_ERROR,						/*!< */
	I2S_FIRST_HALF,				/*!< */
	I2S_SECOND_HALF,			/*!< */
	I2S_KILL,
};

/**
	*\typedef
	*	\struct
  *	\brief Audio configuration variables
	*/
typedef struct{	
	uint16_t frame_size;			/*!< Auido frame size */
	uint16_t ring_buff_size;	/*!< Ring Buff size */
	uint16_t freq;						/*!< Auido frequency in Hz */
	
	uint16_t bw_high;					/*!< High pass filter for PDM decoder in Hz */
	uint16_t bw_low;					/*!< High pass filter for PDM decoder in Hz */
	uint8_t  decimator;				/*!< Decimator used for PDM decoder */
	
	uint8_t bit_resolution;		/*!< High pass filter for PDM decoder in Hz */
	uint8_t channel_nbr;			/*!< Number of channels for audio (Mono = 1, Stereo = 2) */
	uint8_t volume;						/*!< Audio output volume */
	
}Capt_conf;

/**	
	*\typedef
	*	\struct
  *	\brief 
	*/
typedef struct {
  int32_t offset;
  uint32_t fptr;
}Audio_BufferTypeDef;


//------------------------------------------------------------------------------
//											PUBLIC AUDIO DRIVER CONTROL FUNCTIONS
//------------------------------------------------------------------------------
/**
  * @brief  Initalized Audio Capture
	* @param  config: Audio capture configuration
  * @param  data: Adress of the pointer where the data will be store
	* @param  data_buff_size: Current size of the recorded buffer
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t initCapture(const Capt_conf* config, uint16_t* data, uint16_t data_buff_size, osMessageQId message_QId, uint32_t message_val);
/**
  * @brief  De-initalized Audio Capture
  * @param  
  * @retval 
  */
void deinitCapture (void);
/**
  * @brief  Starts audio recording.
  * @param  
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t audioRecord(void);
/**
  * @brief  Stops audio recording.
  * @param  
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t audioStop(void);
/**
  * @brief  Pauses the audio file stream.
  * @param  
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t audioPause(void);
/**
  * @brief  Resumes the audio file stream.
  * @param  
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t audioResume(void);


//------------------------------------------------------------------------------
//														CALLBACK AUDIO FUNCTIONS
//------------------------------------------------------------------------------
//	This function should be implemented by the user application.
//	It is called into this driver when the current buffer is filled to prepare the next buffer pointer and its size. */
/**
  * @brief  User callback when record buffer is filled.
  * @param  None
  * @retval None
  */
void audioTransferComplete_CallBack(void);
/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @param  None
  * @retval None
  */
void audioHalfTransfer_CallBack(void);
/**
  * @brief  Audio IN Error callback function.
  * @param  None
  * @retval None
  */
void audioError_Callback(void);

//------------------------------------------------------------------------------
//											PRIVATE AUDIO DRIVER CONTROL FUNCTIONS
//------------------------------------------------------------------------------
/**
  * @brief  Converts PDM audio format from to PCM.
  * @param  PDMBuf: Pointer to data PDM buffer
  * @param  PCMBuf: Pointer to data PCM buffer
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t audioPDM2PCM(uint16_t *PDMBuf, uint32_t PDMsize, uint16_t *PCMBuf, uint8_t channel,uint8_t volume, PDMFilter_InitStruct *Filter);
/**
  * @brief  Initialize the PDM library.
  * @param  AudioFreq: Audio sampling frequency
  * @param  ChnlNbr: Number of audio channels (1: mono; 2: stereo)
  * @retval None
  */
void PDMDecoder_Init(uint32_t AudioFreq, uint32_t ChnlNbr, PDMFilter_InitStruct *Filter);
/**
  * @brief  Rx Transfer completed callbacks
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s);
/**
  * @brief  Rx Half Transfer completed callbacks.
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s);
/**
  * @brief  Error Transfer callbacks.
  * @param  hi2s: I2S handle
  * @retval None
  */
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s);

void DMA_Interrup_Handler_Task (void const *pvParameters);
	
#endif //ALE_STM32F4_DISCOVERY_AUDIO_H

