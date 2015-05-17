/**
  ******************************************************************************
  * @file    msic.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    06-Mayo-2015
  * @brief   Some miscellaneous functions
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/  

#ifndef _MISC_H
#define _MISC_H

#include "arm_math.h"
#include "ff.h"

/**
	* @brief
	* @param
	* @param
	*/
char *		updateFilename 						(char *Filename);

FRESULT open_append (FIL* fp, const char* path);
//---------------------------------------
//						MATH FUNCTIONS
//---------------------------------------
/**
  * @brief  Calcula la derivada del vector. El vector destino debería tener tamaño blockSize-1
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void 			arm_diff_f32 		(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void			cumsum 					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void			sumlog					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
/**
	* @brief Function to find minimum of x and y
	* @param x value
	* @param y value
	* @retval minimun value
	*/ 
int min(int x, int y);
/**
	* @brief Function to find maximum of x and y
	* @param x value
	* @param y value
	* @retval maximum value
	*/ 
int max(int x, int y);

//---------------------------------------------------------------------------------
//														WAVE FILE FUNCTIONS MANAGMENT
//---------------------------------------------------------------------------------
typedef struct {
	uint32_t SampleRate;        	/* Audio sampling frequency */
	uint8_t  NbrChannels;       	/* Number of channels: 1:Mono or 2:Stereo */
	uint8_t  BitPerSample;				/* Number of bits per sample (16, 24 or 32) */
}WAVE_Audio_config;

typedef struct {
  uint32_t   ChunkID;       /* 0 */ 
  uint32_t   FileSize;      /* 4 */
  uint32_t   FileFormat;    /* 8 */
  uint32_t   SubChunk1ID;   /* 12 */
  uint32_t   SubChunk1Size; /* 16*/  
  uint16_t   AudioFormat;   /* 20 */ 
  uint16_t   NbrChannels;   /* 22 */   
  uint32_t   SampleRate;    /* 24 */
  
  uint32_t   ByteRate;      /* 28 */
  uint16_t   BlockAlign;    /* 32 */  
  uint16_t   BitPerSample;  /* 34 */  
  uint32_t   SubChunk2ID;   /* 36 */   
  uint32_t   SubChunk2Size; /* 40 */    
	uint8_t		 pHeader[44];		
}WAVE_FormatTypeDef;

/**
  * @brief  Create a new Wave File
	* @param  WaveFormat: Pointer to a structure type WAVE_FormatTypeDef
  * @param  WavFile: 
	* @param	Filename:
  * @param  pHeader: Pointer to the Wave file header to be written.  
  * @retval pointer to the Filename string
  */
FRESULT 	newWavFile 		(char *Filename, WAVE_FormatTypeDef* WaveFormat, FIL *WavFile, WAVE_Audio_config config);
FRESULT 	closeWavFile 	(FIL *WavFile, WAVE_FormatTypeDef* WaveFormat, uint32_t audio_size);
/**
  * @brief  Encoder initialization.
	* @param  WaveFormat: Pointer to a structure type WAVE_FormatTypeDef
  * @param  Freq: Sampling frequency.
  * @param  pHeader: Pointer to the Wave file header to be written.  
  * @retval 0 if success, !0 else.
  */
uint32_t WaveProcess_EncInit(WAVE_FormatTypeDef* WaveFormat, WAVE_Audio_config config);
/**
  * @brief  Initialize the Wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the Wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
uint32_t	WaveProcess_HeaderInit		(WAVE_FormatTypeDef* pWaveFormatStruct);
/**
  * @brief  Initialize the Wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the Wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
uint32_t	WaveProcess_HeaderUpdate	(WAVE_FormatTypeDef* pWaveFormatStruct, uint32_t adudio_size);

#endif // _MISC_H
