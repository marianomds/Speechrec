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

#define FREERTOS

#ifdef FREERTOS

#define malloc(size) pvPortMalloc(size)
#define free(ptr) pvPortFree(ptr)

#endif





/* Includes ------------------------------------------------------------------*/
#include "ApplicationConfig.h"
#include "arm_math.h"
#include "cmsis_os.h"
#include "stdbool.h"
#include "ALE_STM32F4_DISCOVERY_Audio_Input_Driver.h"
#include "audio_processing.h"
#include "ff.h"



//---------------------------------------------------------------------------------
//																		TASKS ARGUMENTS
//---------------------------------------------------------------------------------
/**
	*\typedef
	*	\struct
  *	\brief Audio Processing task arguments
	*/
typedef struct{
	osMessageQId src_msg_id;
	char *file_path;
	uint16_t *data;
	ProcConf *proc_conf;
	bool vad;
	bool save_to_files;
}Audio_Processing_args;
/**
	*\typedef
	*	\struct
  *	\brief File Processing task arguments
	*/
typedef struct{
	osMessageQId src_msg_id;
	char *file_name;
	char *file_path;
	ProcConf *proc_conf;
	bool vad;
	bool save_to_files;
}File_Processing_args;
/**
	*\typedef
	*	\struct
  *	\brief Audio Capture task arguments
	*/
typedef struct{
	Auido_Capture_Config audio_conf;
	uint16_t *data;
	uint32_t data_buff_size;
}Audio_Capture_args;
/**
	*\typedef
	*	\struct
  *	\brief Recognition task arguments
	*/
typedef struct{
	char *patterns_path;
	char *patterns_config_file_name;
}Recognition_args;






/**
	*\typedef
	*	\enum
  *	\brief Application LEDs
	*/
typedef enum {
	CALIB_LED   = OLED,
	PATTERN_LED = GLED,
	RECOG_LED 	= BLED,
	EXECUTE_LED = RLED,
}AppLEDS;
/**
	*\typedef
	*	\enum
  *	\brief Common task messages
	*/
typedef enum {
	FRAME_READY,
	END_CAPTURE,
	FAIL,
	BUTTON_IRQ,	
	BUTTON_RELEASE,
	BUTTON_PRESS,
	CHANGE_STATE,
	FINISH_PROCESSING,
	KILL_THREAD,
}Common_task_Messages;
/**
	*\typedef
	*	\enum
  *	\brief Application states
	*/
typedef enum {
	CALIBRATION = 0,
	PATTERN_STORING  = 1,
	RECOGNITION = 2,
}AppStates;
/**
	*	\enum
  *	\brief Processing task messages
	*/
enum ProcessMsg{
	NEXT_FRAME,
	LAST_FRAME,
};
/**
	*	\enum
  *	\brief Recognition task states
	*/
enum States{
	RECORD_AUDIO,
	RECOGNIZED,
};

typedef struct {
	bool			debug;
	bool			save_proc_vars;
	bool			save_vad_vars;
}DebugConfig;


typedef struct {
	AppStates maintask;
	bool 			vad;

	Auido_Capture_Config audio_capture_conf;
	ProcConf	proc_conf;	
	CalibConf	calib_conf;
	
	char patdir[13];
	char patfilename[13];
	
	DebugConfig		debug_conf;
}AppConfig;


/**
	*\typedef
	*	\struct
  *	\brief Audio Capture task arguments
	*/
typedef struct {
	char pat_name[PATERN_MAX_NAME_SIZE];		/*< Nombre del Patron */
	uint8_t	pat_actv_num;										/*< Numero de activación */
	arm_matrix_instance_f32 pattern_mtx;		/*< Instancia de matriz para los atributos */
}Patterns;
/**
	*	\struct
  *	\brief Vector Cuantization
	*/
struct VC {
//	float32_t Energy;
	float32_t MFCC[LIFTER_LEGNTH];
};






/**
	*\typedef
	*	\struct
  *	\brief Mail
	*/
typedef struct {
	osMessageQId src_msg_id;
	char *file_name;
	char *file_path;
}Mail;
/**
	*\typedef
	*	\ENUM
  *	\brief Audio Capture STATES
	*/
typedef enum{
	START_CAPTURE,
	RESUME_CAPTURE,
	STOP_CAPTURE,
	PAUSE_CAPTURE,
	BUFFER_READY,
	WAIT_FOR_MAIL,
	KILL_CAPTURE,
}Capture_states;

//---------------------------------------------------------------------------------
//															GENERAL FUNCTIONS
//---------------------------------------------------------------------------------

void Error_Handler(void);
void Configure_Application (void);
void setEnvVar (void);
uint8_t readConfigFile (const char *filename, AppConfig *Conf);
uint8_t readPaternsConfigFile (const char *filename, Patterns **Pat, uint32_t *pat_num);
uint8_t loadPattern (Patterns *pat);
/* [OUT] File object to create */
/* [IN]  File name to be opened */


//---------------------------------------------------------------------------------
//																	APPLICATIONS TASKS 
//---------------------------------------------------------------------------------
/**
  * @brief  
  * @param  
  * @retval
  */
void Main_Thread 			(void const * pvParameters);
/**
  * @brief  PatternStoring Task
	* @param  
  * @retval 
  */
void PatternStoring		(void const * pvParameters);
/**
  * @brief  Recognitiond Task
	* @param  
  * @retval 
  */
void Recognition			(void const * pvParameters);
/**
	* @brief  Calibration
	* @param  
	* @retval 
	*/
void Calibration	 		(void const * pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void Keyboard 				(void const * pvParameters);
/**
	* @brief	Procesa el archivo de Audio que se pasa por eveneto y almacena en un nuevo archivo los MFCC indicando también a que Nº de frame corresponde
	* @param	Recives folders name where the audio file si located (folders name and audio file name must be equal).
	*/
void fileProcessing		(void const * pvParameters);
/**
	* @brief  Audio Capture Task
	* @param  
	* @retval 
	*/
void AudioCapture 		(void const * pvParameters);
/**
	* @brief	Procesa el archivo de Audio que se pasa por eveneto y almacena en un nuevo archivo los MFCC indicando también a que Nº de frame corresponde
	* @param	Recives folders name where the audio file si located (folders name and audio file name must be equal).
	*/
void audioProcessing	(void const * pvParameters);

//---------------------------------------------------------------------------------
//																INTERRUPTION FUNCTIONS
//---------------------------------------------------------------------------------
void User_Button_EXTI (void);



//---------------------------------------------------------------------------------
//																		TEST TASKS
//---------------------------------------------------------------------------------




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
FRESULT 	newWavFile 		(char *Filename, WAVE_FormatTypeDef* WaveFormat, FIL *WavFile);
FRESULT 	closeWavFile 	(FIL *WavFile, WAVE_FormatTypeDef* WaveFormat, uint32_t audio_size);
/**
  * @brief  Encoder initialization.
	* @param  WaveFormat: Pointer to a structure type WAVE_FormatTypeDef
  * @param  Freq: Sampling frequency.
  * @param  pHeader: Pointer to the Wave file header to be written.  
  * @retval 0 if success, !0 else.
  */
uint32_t	WaveProcess_EncInit				(WAVE_FormatTypeDef* WaveFormat);
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




#endif  // _APPLICATION_H
