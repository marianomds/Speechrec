/**
  ******************************************************************************
  * @file    Application.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    05-Agosto-2014
  * @brief   Application Tasks for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */   

#ifndef _APPLICATION_H
#define _APPLICATION_H

#include "ApplicationConfig.h"


/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "ff.h"
#include "arm_math.h"
#include "float.h"
#include "stdbool.h"

#define osKernelSysTickFrequency   					configTICK_RATE_HZ
#define osKernelSysTickMiliSec(milisec)	   (((uint64_t)milisec * (osKernelSysTickFrequency)) / 1000)

typedef struct AppConfig_ {

	uint8_t time_window;
	uint8_t time_overlap;

	uint8_t 	numtaps;
	float32_t alpha;
	uint8_t 	fft_len;
	uint8_t 	mel_banks;
	uint8_t 	ifft_len;
	uint8_t 	lifter_legnth;

	uint8_t calib_time;
	uint8_t calib_len;

	uint8_t 	calib_thd_energy;
	uint8_t 	calib_thd_freclow;
	uint8_t 	calib_thd_frechigh;
	float32_t calib_thd_sf;
	
	char patdir[15];
	char patfilename[15];
	
} AppConfig;
	
typedef enum {
	TRANSFER_ERROR,
	TRANSFER_COMPLETE,
	TRANSFER_HALF,
} Transfer;

typedef enum {
	BUTTON_IRQ = 1,
	
	BUTTON_RELEASE = 1,
	BUTTON_PRESS = 2,
	BUTTON_RETAIN_1s = 3,
} User_Button_State;

typedef enum{
	RECORD_AUDIO,
	RECOGNIZED,
} States;

#define	BUFFER_READY	0

struct VC {
	float32_t Energy;
	float32_t MFCC[LIFTER_LEGNTH];
};

typedef struct Patterns_ {
	char PatName[PATERN_MAX_NAME_SIZE];		/*< Nombre del Patron */
	uint8_t	PatActvNum;										/*< Numero de activación */
	struct VC* Atrib;											/*< Puntero al listado de atributos */
} Patterns;

typedef enum{
	LEFT = 0,
	MIDDLE = 1,
	BOTTOM = 2,
	DONTCARE = 3,
}path;
//---------------------------------------------------------------------------------
//															GENERAL FUNCTIONS
//---------------------------------------------------------------------------------

void Error_Handler(void);
void Configure_Application (void);
void setEnvVar (void);
uint8_t readConfigFile (const char *filename, AppConfig *Conf);
uint8_t readPaternsConfigFile (const char *filename, Patterns *Conf);
uint8_t LoadPatterns (const char *filename, struct VC* ptr);


//---------------------------------------------------------------------------------
//																	APPLICATIONS TASKS 
//---------------------------------------------------------------------------------
void Main_Thread 			(void const * pvParameters);
void AudioRecord	 		(void const * pvParameters);
void Keyboard 				(void const * pvParameters);
void ProcessAudioFile	(void const * pvParameters);
void Recognize 				(void const * pvParameters);
#ifdef _VAD_
void Calibration	 		(void const * pvParameters);
#endif
//---------------------------------------------------------------------------------
//																INTERRUPTION FUNCTIONS
//---------------------------------------------------------------------------------
void User_Button_EXTI (void);


//---------------------------------------------------------------------------------
//																	AUDIO FUNCTIONS
//---------------------------------------------------------------------------------
#ifdef _VAD_
void 			Calib 					(float32_t *Energy, float32_t *Frecmax, float32_t *SpecFlat);
uint8_t 	VAD_MFCC_float	(float32_t *MFCC, uint16_t *Frame, float32_t *HamWin, float32_t *Lifter);
#endif
void 			Hamming_float 	(float32_t *Hamming, uint32_t LEN);
void 			Lifter_float 		(float32_t *Lifter, uint32_t L);
uint8_t 	MFCC_float 			(float32_t *MFCC, uint16_t *Frame, float32_t *HamWin, float32_t *Lifter);
void 			Error						(void);
void 			arm_diff_f32 		(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
void			cumsum 					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
void			sumlog					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
float32_t dtw 						(const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path);
float32_t dist 						(float32_t *pSrcA, float32_t *pSrcB, uint16_t blockSize);
//---------------------------------------------------------------------------------
//																		TEST TASKS
//---------------------------------------------------------------------------------

void Process_and_Save (void const *pvParameters);
void Calibration_and_Save (void);



//---------------------------------------------------------------------------------
//														WAVE FILE FUNCTIONS MANAGMENT
//---------------------------------------------------------------------------------

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

}WAVE_FormatTypeDef;


typedef struct {
  int32_t offset;
  uint32_t fptr;
}Audio_BufferTypeDef;

FRESULT 	New_Wave_File 						(char *Filename, WAVE_FormatTypeDef* WaveFormat, FIL *WavFile, uint8_t *pHeader,uint32_t *byteswritten);
uint32_t	WaveProcess_EncInit				(WAVE_FormatTypeDef* WaveFormat, uint8_t* pHeader);
uint32_t	WaveProcess_HeaderInit		(uint8_t* pHeader, WAVE_FormatTypeDef* pWaveFormatStruct);
uint32_t	WaveProcess_HeaderUpdate	(uint8_t* pHeader, WAVE_FormatTypeDef* pWaveFormatStruct, uint32_t DataSize);
char *		updateFilename 						(char *Filename);


#endif  // _APPLICATION_H
