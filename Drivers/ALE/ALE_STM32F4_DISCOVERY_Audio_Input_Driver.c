/**
  ******************************************************************************
  * @file    ALE_STM32F4_DISCOVERY_Audio_Input_Driver.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    26-Febrero-2015
  * @brief   STM32F4 Discovery Board Libraries
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */  
	
/* Includes ------------------------------------------------------------------*/
#include "ALE_STM32F4_DISCOVERY_Audio_Input_Driver.h"



Auido_Capture_Config capture_conf;
uint32_t pdm_buff_size;
uint32_t pcm_buff_size;
uint32_t extend_buff;

uint8_t  *PDM;
uint16_t *PCM;
PDMFilter_InitStruct *Filter;

osMessageQId msgQId;
uint32_t msg_val;

extern int8_t test;

//------------------------------------------------------------------------------
//											PUBLIC AUDIO DRIVER CONTROL FUNCTIONS
//------------------------------------------------------------------------------
uint8_t initCapture(const Auido_Capture_Config* config, uint16_t* data, uint16_t data_buff_size, osMessageQId message_QId, uint32_t message_val){
	
	// Save configuration
	capture_conf = *config;
	msgQId = message_QId;
	msg_val = message_val;
	
	// Calculate buffer sizes
	pdm_buff_size = capture_conf.audio_freq/1000*capture_conf.audio_decimator*capture_conf.audio_channel_nbr/8;
	pcm_buff_size = capture_conf.audio_freq/1000*capture_conf.audio_channel_nbr;
	extend_buff   = data_buff_size/pcm_buff_size;
	
	// Create buffers
	PCM = data;
	PDM = pvPortMalloc(pdm_buff_size*extend_buff*2);					// Get memory for PDM Buffer
	Filter = pvPortMalloc(capture_conf.audio_channel_nbr*sizeof(*Filter));		// Get memory for Filter structure
	
	// Initialize PDM Decoder
	PDMDecoder_Init(capture_conf.audio_freq,capture_conf.audio_channel_nbr,Filter);
	
	return AUDIO_OK;
}


void deinitCapture (void){
	vPortFree(PDM);
	vPortFree(Filter);
}

uint8_t audioRecord(void){
  uint32_t ret = AUDIO_ERROR;
  
  /* Start the process receive DMA */
  HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)PDM, pdm_buff_size*extend_buff);
  
  /* Return AUDIO_OK when all operations are correctly done */
  ret = AUDIO_OK;
  
  return ret;
}

uint8_t audioStop(void){
  uint32_t ret = AUDIO_ERROR;
  
  /* Call the Media layer pause function */
  HAL_I2S_DMAStop(&hi2s2);  
  
  /* Return AUDIO_OK when all operations are correctly done */
  ret = AUDIO_OK;
  
  return ret;
}


uint8_t audioPause(void){    
  /* Call the Media layer pause function */
  HAL_I2S_DMAPause(&hi2s2);
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}


uint8_t audioResume(void){    
  /* Call the Media layer pause/resume function */
  HAL_I2S_DMAResume(&hi2s2);
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

//------------------------------------------------------------------------------
//											PRIVATE AUDIO DRIVER CONTROL FUNCTIONS
//------------------------------------------------------------------------------

void PDMDecoder_Init(uint32_t AudioFreq, uint32_t ChnlNbr, PDMFilter_InitStruct *Filter){ 
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
    PDM_Filter_Init(&Filter[i]);
  }
}

uint8_t audioPDM2PCM(uint16_t *PDMBuf, uint32_t PDMsize, uint16_t *PCMBuf, uint8_t channel, uint8_t volume, PDMFilter_InitStruct *Filter){
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

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s){
  /* First Half of PDM Buffer */
	int i;
	
	for(i=0; i<extend_buff; i++)		
		audioPDM2PCM((uint16_t*)&PDM[i*pdm_buff_size],pdm_buff_size,&PCM[i*pcm_buff_size],capture_conf.audio_channel_nbr, capture_conf.audio_volume, Filter);
	
	osMessagePut(msgQId,msg_val,0);
	test++;	// TODO de prueba, luego borrar
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s){
	int i;
	
	for(i=extend_buff; i<extend_buff*2; i++)
		audioPDM2PCM((uint16_t*)&PDM[i*pdm_buff_size],pdm_buff_size,&PCM[(i-extend_buff)*pcm_buff_size],capture_conf.audio_channel_nbr, capture_conf.audio_volume, Filter);

	osMessagePut(msgQId,msg_val,0);
	test++;		// TODO de prueba, luego borrar
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s){

		Error_Handler();
}
