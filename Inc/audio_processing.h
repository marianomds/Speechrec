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
#include "arm_math.h"
#include "ff.h"
#include "stdbool.h"


typedef enum
{
	First_Stage = 0x01,
	Second_Stage = 0x02,
	Third_Stage = 0x04,
}ProcStages;
														
//enum{
//	FINISH_PROCESSING,
//};

typedef enum{
	VOICE,
	NO_VOICE,
	PROC_FAILURE
}ProcStatus;

typedef enum{
	CALIB_INITIATED,
	CALIB_IN_PROCESS,
	CALIB_FINISH,
	CALIB_FAILURE,
	CALIB_OK,
}CalibStatus;

 typedef struct{
	 
//	uint8_t		time_window;
//	uint8_t		time_overlap;
	 
	uint16_t	frame_len;
	uint16_t	frame_net;
	uint16_t	frame_overlap;
	uint16_t	zero_padding_left;
	uint16_t	zero_padding_right;
	 
	uint16_t	numtaps;
	float32_t	alpha;
	uint16_t	fft_len;
	uint16_t	mel_banks;
	uint16_t	dct_len;				// Tiene que ser 2^N y mayor que MEL_BANKS
	uint16_t	lifter_legnth;
	
}ProcConf;
 
typedef struct{
	uint16_t	calib_time;
	uint32_t	calib_len;
	float32_t	thd_scl_eng;
	uint32_t	thd_min_fmax;
	float32_t	thd_scl_sf;
}CalibConf;

typedef struct{
	float32_t *speech;			// Señal de audio escalada (tiene el largo de frame_net)
	float32_t *FltSig;			// Señal de audio pasada por el Filtro de Pre-Enfasis (tiene el largo de frame_net)
	float32_t *Frame;				// Frame con el overlap incluido (tiene el largo de frame_len)
	float32_t *WinSig;			// Señal de filtrada multiplicada por la ventana de Hamming
	float32_t *STFTWin;			// Transformada de Fourier en Tiempo Corto
	float32_t *MagFFT;			// Módulo del espectro
	float32_t *MelWin;			// Espectro pasado por los filtros de Mel
	float32_t *LogWin;			// Logaritmo del espectro filtrado
	float32_t *CepWin;			// Señal cepstral
	uint8_t 	VAD;					// Indica si es salida de voz o no
}Proc_var;

typedef struct {
	FIL HamWinFile;
	FIL CepWeiFile;
	FIL SpeechFile;
	FIL FltSigFile;
	FIL FrameFile;
	FIL WinSigFile;
	FIL STFTWinFile;
	FIL MagFFTFile;
	FIL MelWinFile;
	FIL LogWinFile;
	FIL CepWinFile;
	
	// When VAD Activated
	FIL VADFile;
	FIL EnerFile;
	FIL FrecFile;
	FIL SFFile;
	
} Proc_files;




//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------


void initBasics		(ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers);
void finishBasics	(void);

/**
	*	Initialized Processing.
  * @brief  Initialized Processing.
	* @param[in]	configuration: processing configuration information
	* @param[in]	vad:	if VAD will be use or not 
	* @param[in]	save_vars: if it should save processing variables
  */
void		initProcessing				(float32_t **MFCC, uint32_t *MFCC_size, ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers);
/**
  * @brief  De-Initialized Processing
  */
void		finishProcessing			(void);
/**
	*	Computes the Mel-Frequency Cepstrum Coefficients. If VAD is set but the frame does not reach the threshold
	*	MFCC buffer is filled with 0.
  * @brief  Computes the Mel-Frequency Cepstrum Coefficients
	* @param[in]  frame: Address of the vector with the Speech signal of the Frame to be processed
	* @param[out]  MFCC: Length of the Frame
	* @param[in]  vad: If VAD should be used or no
	* @param[out]  saving_var: Address of the vector with the Window to apply
  */
ProcStatus		MFCC_float				(uint16_t *frame, ProcStages *stages);

// CALIBRATION

CalibStatus		initCalibration		(uint32_t *num_frames, CalibConf *calib_config, ProcConf *configuration, Proc_var *ptr_vars_buffers);
CalibStatus		Calibrate					(uint16_t *frame, uint32_t frame_num, ProcStages *stages);
CalibStatus		endCalibration		(const bool save_calib_vars);
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
uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad, ProcStages stage);
uint8_t Close_proc_files (Proc_files *files, const bool vad);



//---------------------------------------
//			BASE PROCESSING FUNCTIONS
//---------------------------------------	
void			secondProcStage	(bool vad, Proc_var *saving_var);
void			thirdProcStage	(float32_t *MFCC, Proc_var *saving_var);
void			firstProcStage	(Proc_var *saving_var);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void 			Hamming_float 	(float32_t *Hamming, uint32_t LEN);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void 			Lifter_float 		(float32_t *Lifter, uint32_t L);



//---------------------------------------
//						OTHER FUNCTIONS
//---------------------------------------
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

#endif // AUDIO_PROCESSING_H
