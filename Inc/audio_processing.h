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
#include <ring_buffer.h>

//-------------------------------------------------------
//									PROCESSING TYPEDEF
//-------------------------------------------------------

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	 
	// Audio Filtering
	uint16_t	numtaps;
	float32_t	alpha;

	// Frame Blocking
	uint16_t	frame_len;
	uint16_t	frame_overlap;	 
	 
	// FFT
	uint16_t	fft_len;
	 
	// Mel Filter
	uint16_t	mel_banks;
	 
	// Compresion
	uint16_t	dct_len;				// Tiene que ser 2^N y mayor que MEL_BANKS
	
	// Liftering
	uint16_t	lifter_length;
	
	// Delta coefficients
	uint8_t theta;
	
}Proc_conf;

/**
	*\typedef
	*	\struct
  *	\brief VAD
	*/
typedef struct
{
	bool			vad;
	uint8_t		age_thd;
	uint8_t		timeout_thd;
}VAD_conf;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	float32_t	energy;
	uint32_t	fmax;
	float32_t	sp;
	bool 			VAD;					// Indica si es salida de voz o no
}VAD_var;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	float32_t *speech;			// Señal de audio escalada (tiene el largo de frame_net)
	float32_t *FltSig;			// Señal de audio pasada por el Filtro de Pre-Enfasis (tiene el largo de frame_net)
	float32_t *Frame;				// Frame con el overlap incluido (tiene el largo de frame_len)
	float32_t *WinSig;			// Señal de filtrada multiplicada por la ventana de Hamming
	float32_t *STFTWin;			// Transformada de Fourier en Tiempo Corto
	float32_t *MagFFT;			// Módulo del espectro
	float32_t *MelWin;			// Espectro pasado por los filtros de Mel
	float32_t *LogWin;			// Logaritmo del espectro filtrado
	float32_t *CepWin;			// Señal cepstral
	float32_t *MFCC;				// Coeficientes MFCC
	VAD_var   vad_vars;
}Proc_var;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
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
	FIL MFCCFile;
	
	// When VAD Activated
	FIL VADFile;
	FIL EnerFile;
	FIL FrecFile;
	FIL SFFile;
	
} Proc_files;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	Proc_conf *proc_conf;
	VAD_conf	*vad_conf;
	bool save_to_files;	
	ringBuf *audio_buff;
	ringBuf *features_buff;
	osMessageQId src_msg_id;
	uint32_t	src_msg_val;
	char *path;
	osMessageQId proc_msg_id;
}Feature_Extraction_args;


/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef enum
{
	No_Stage = 0x00,
	First_Stage = 0x01,
	Second_Stage = 0x02,
	Third_Stage = 0x04,
	VAD_Stage = 0x08,
	
	FS_Stage = 0x03,
	FST_Stage = 0x07,
	FSV_Stage = 0x0B,
	All_Stages = 0x0F,
}Proc_stages;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef enum
{
	PROC_BUFF_READY,
	PROC_FINISH,
	PROC_KILL,
}Proc_msg;
 
/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef enum
{
	VOICE,
	NO_VOICE,
	PROC_FAILURE
}Proc_status;



//-------------------------------------------------------
//								CALIBRATION TYPEDEF
//-------------------------------------------------------

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	uint16_t	calib_time;
	float32_t	thd_scl_eng;
	uint32_t	thd_min_fmax;
	uint32_t	thd_max_fmax;
	float32_t	thd_scl_sf;
}Calib_conf;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef struct
{
	Proc_conf *proc_conf;
	Calib_conf *calib_conf;
	VAD_conf 	*vad_conf;
	uint32_t audio_freq;
	bool save_to_files;	
	bool save_calib_vars;
	ringBuf *audio_buff;
	osMessageQId src_msg_id;
	uint32_t	src_msg_val;
	char *path;	
	osMessageQId calib_msg_id;
}Calib_args;

/**
	*\typedef
	*	\struct
  *	\brief Processing task arguments
	*/
typedef enum
{
	CALIB_BUFF_READY,
	CALIB_FINISH,
	CALIB_KILL,
}Calib_msg;


//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------

/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void featureExtraction (void const *pvParameters);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void calibration (void const *pvParameters);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void initProcessing (Proc_conf *proc_config);
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
Proc_status		MFCC_float				(uint16_t *frame, Proc_stages *stages, bool last_frame);

//---------------------------------------
//						CALIBRATION
//---------------------------------------

/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
//void		initCalibration			(Calib_conf *calib_config, Proc_conf *proc_config, uint32_t audio_freq);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
//void		finishCalibration		(void);
//---------------------------------------
//						HELP FUNCTIONS
//---------------------------------------
/**
	* @brief  Creates and opens files needed for saving processing variables
	* @param[in] files	Pointer to a Proc_files struct
	* @param[in] vad		True if it should also save VAD variables
	* @retval 	0	==> OK		!=0 ==> Error code
	*/
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
uint8_t Open_proc_files (Proc_files *files, Proc_stages stage);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
uint8_t Append_proc_files (Proc_files *files, const Proc_var *var, Proc_stages stage);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
uint8_t Close_proc_files (Proc_files *files, Proc_stages stage);



//---------------------------------------
//			BASE PROCESSING FUNCTIONS
//---------------------------------------	
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void initBasics (Proc_conf *proc_config);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void finishBasics	(void);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void	firstProcStage	(float32_t *filt_signal, uint16_t *audio, Proc_var *saving_var);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void	secondProcStage	(float32_t *MagFFT, float32_t *Energy, float32_t *frame_block, Proc_var *saving_var);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void	thirdProcStage	(float32_t *MFCC, float32_t *MagFFT, Proc_var *saving_var);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void	VAD			(VAD_var *vad, float32_t *MagFFT, Proc_var *saving_var);
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void deltaCoeff				(float32_t *output, float32_t *input, uint8_t theta, uint8_t num_params);
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
void		allocateProcVariables	(Proc_var *var, Proc_stages stage);
/**
	* @brief  Release Heap memory for Processing variables needed to save it to files
	* @param[in] var	Pointer to a Proc_var struct
	*/
void		freeProcVariables			(Proc_var *var, Proc_stages stage);

#endif // AUDIO_PROCESSING_H
