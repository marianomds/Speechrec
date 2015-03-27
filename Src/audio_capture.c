/**
  ******************************************************************************
  * @file    audio_capture.c
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
#include "audio_capture.h"


Auido_Capture_Config capture_conf;
uint32_t pdm_buff_size;
uint32_t pcm_buff_size;
uint32_t extend_buff;

uint8_t  *PDM;
uint16_t *PCM;
PDMFilter_InitStruct *Filter;


//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------
uint32_t initCapture(const Auido_Capture_Config* config, uint16_t** data, uint16_t buff_size){
	
	// Save configuration
	capture_conf = *config;
	
	// Calculate buffer sizes
	pdm_buff_size = capture_conf.audio_freq*capture_conf.audio_decimator*capture_conf.audio_channel_nbr/8;
	pcm_buff_size = capture_conf.audio_freq*capture_conf.audio_channel_nbr;
	extend_buff   = buff_size/pcm_buff_size;
	
	// Create buffers
	PDM = pvPortMalloc(pdm_buff_size*extend_buff*2);					// Get memory for PDM Buffer
	PCM = pvPortMalloc(pcm_buff_size*extend_buff);						// Get memory for PCM Buffer
	Filter = pvPortMalloc(capture_conf.audio_channel_nbr);		// Get memory for Filter structure
	
	// Initialize PDM Decoder
	PDMDecoder_Init(capture_conf.audio_freq,capture_conf.audio_channel_nbr,Filter);
	
	*data = PCM;
	
	return pcm_buff_size*extend_buff;
}

void deinitCapture (void){
	vPortFree(PDM);
	vPortFree(PCM);
	vPortFree(Filter);
}

void audioRecord (void){
	if(AUDIO_IN_Record((uint16_t *)PDM, pdm_buff_size*extend_buff) == AUDIO_ERROR)
		Error_Handler();
}

void audioPause (void){
	AUDIO_IN_Pause();
}

void audioResume (void){
	if(AUDIO_IN_Resume() == AUDIO_ERROR)
		Error_Handler();						
}

void audioStop (void){
	AUDIO_IN_Stop();
}





//---------------------------------------
//				AUDIO CAPTURE BASE FUNCTIONS
//---------------------------------------
/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_HalfTransfer_CallBack(void) { 
	/* First Half of PDM Buffer */
	int i;
	
	for(i=0; i<extend_buff; i++)		
		AUDIO_IN_PDM2PCM((uint16_t*)&PDM[i*pdm_buff_size],pdm_buff_size,&PCM[i*pcm_buff_size],capture_conf.audio_channel_nbr, capture_conf.audio_volume, Filter);

	osMessagePut(capture_conf.msgQId,capture_conf.msg,0);
}
/**
  * @brief  Manages the DMA Transfer Complete interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_TransferComplete_CallBack(void) {
	/* Second Half of PDM Buffer */
	int i;
	
	for(i=extend_buff; i<extend_buff*2; i++)
		AUDIO_IN_PDM2PCM((uint16_t*)&PDM[i*pdm_buff_size],pdm_buff_size,&PCM[(i-extend_buff)*pcm_buff_size],capture_conf.audio_channel_nbr, capture_conf.audio_volume, Filter);

	osMessagePut(capture_conf.msgQId,capture_conf.msg,0);
}
/**
  * @brief  Manages the DMA Error interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_Error_CallBack	(void){
	Error_Handler();
}

