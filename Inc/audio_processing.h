/**
  ******************************************************************************
  * @file    audio_processing.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/
	
#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H



/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "ff.h"
#include "arm_math.h"
#include "float.h"
#include "usb_host.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "recognition_specific.h"
#include "error_handler.h"

//enum{
//	FINISH_PROCESSING,
//};

//typedef enum{
//	NEXT_FRAME,
//	LAST_FRAME,
//}ProcMsg;


 typedef struct{
	 
//	uint8_t		time_window;
//	uint8_t		time_overlap;
	 
	uint16_t	frame_len;
	uint16_t	frame_overlap;
	uint16_t	frame_net;
	 
	uint16_t	numtaps;
	float32_t	alpha;
	uint16_t	fft_len;
	uint16_t	mel_banks;
	uint16_t	ifft_len;				// Tiene que ser mayor a 2*MEL_BANKS
	uint16_t	lifter_legnth;
	
	uint8_t		thd_e;
	uint8_t		thd_fl;
	uint8_t		thd_fh;
	uint8_t		thd_sf;
}ProcConf;
 
typedef struct{
	uint32_t	calib_time;
	uint32_t	calib_len;

	uint8_t 	calib_thd_energy;
	uint8_t 	calib_thd_freclow;
	uint8_t 	calib_thd_frechigh;
	float32_t calib_thd_sf;
}CalibConf;
 
typedef struct{
	float32_t Energy;
	uint32_t  Frecmax;
	float32_t SpFlat;
}VADVar;

typedef struct{
	float32_t *speech;			// Señal de audio escalada
	float32_t *FltSig;			// Señal de audio pasada por el Filtro de Pre-Enfasis
	float32_t *WinSig;			// Señal de filtrada multiplicada por la ventana de Hamming
	float32_t *STFTWin;			// Transformada de Fourier en Tiempo Corto
	float32_t *MagFFT;			// Módulo del espectro
	float32_t *MelWin;			// Espectro pasado por los filtros de Mel
	float32_t *LogWin;			// Logaritmo del espectro filtrado
	float32_t *CepWin;			// Señal cepstral
	VADVar		vad;
}Proc_var;

typedef struct {
	FIL HamWinFile;
	FIL CepWeiFile;
	FIL SpeechFile;
	FIL FltSigFile;
	FIL WinSigFile;
	FIL STFTWinFile;
	FIL MagFFTFile;
	FIL MelWinFile;
	FIL LogWinFile;
	FIL CepWinFile;
	FIL EnerFile;
	FIL FrecFile;
	FIL SFFile;
} Proc_files;




//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------
/**
	*	Initialized Processing.
  * @brief  Initialized Processing.
	* @param[in]	configuration: 
	* @param[out]	MFCC: 
	* @param[out]	MFCC_size: 
  */
void		initProcessing				(ProcConf *configuration, float32_t **MFCC, uint32_t *MFCC_size);
/**
  * @brief  De-Initialized Processing
  */
void		deinitProcessing			(float32_t *MFCC);
/**
	* @brief  Allocate Heap memory for Processing variables needed to save it to files
	* @param[in] var	Pointer to a Proc_var struct
	*/
void		allocateProcVariables	(Proc_var *var);
/**
	* @brief  Release Heap memory for Processing variables needed to save it to files
	* @param[in] var	Pointer to a Proc_var struct
	*/
void		freeProcVariables			(Proc_var *var);
/**
	*	Computes the Mel-Frequency Cepstrum Coefficients. If VAD is set but the frame does not reach the threshold
	*	MFCC buffer is filled with 0.
  * @brief  Computes the Mel-Frequency Cepstrum Coefficients
	* @param[in]  frame: Address of the vector with the Speech signal of the Frame to be processed
	* @param[out]  MFCC: Length of the Frame
	* @param[in]  vad: If VAD should be used or no
	* @param[out]  saving_var: Address of the vector with the Window to apply
  */
void		MFCC_float	(uint16_t *frame, float32_t *MFCC, bool vad, Proc_var *saving_var);
void		VAD_float		(uint16_t *frame, VADVar *var, Proc_var *saving_var);
void		Calibrate		(float32_t* SilEnergy, float32_t* SilFrecmax, float32_t* SilSpFlat);


//---------------------------------------
//						HELP FUNCTIONS
//---------------------------------------
/**
	* @brief  Creates and opens files needed for saving processing variables
	* @param[in] files	Pointer to a Proc_files struct
	* @param[in] vad		True if it should also save VAD variables
	* @retval 	0	==> OK		!=0 ==> Error code
	*/
uint8_t Open_proc_files (Proc_files *files, const bool vad);
uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad);
uint8_t Close_proc_files (Proc_files *files, const bool vad);



//---------------------------------------
//			BASE PROCESSING FUNCTIONS
//---------------------------------------	
void			firstProcStage	(bool vad, Proc_var *saving_var);
void			secondProcStage	(float32_t *MFCC, Proc_var *saving_var);
void 			Hamming_float 	(float32_t *Hamming, uint32_t LEN);
void 			Lifter_float 		(float32_t *Lifter, uint32_t L);



//---------------------------------------
//						MATH FUNCTIONS
//---------------------------------------
void 			arm_diff_f32 		(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
void			cumsum 					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);
void			sumlog					(float32_t *pSrc, float32_t *pDst, uint32_t blockSize);


#endif // AUDIO_PROCESSING_H
