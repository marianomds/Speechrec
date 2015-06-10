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

#ifndef APPLICATION_H
#define APPLICATION_H

//#define FREERTOS

//#ifdef FREERTOS

//#define malloc(size) pvPortMalloc(size)
//#define free(ptr) pvPortFree(ptr)

//#endif





/* Includes ------------------------------------------------------------------*/
#include "ApplicationConfig.h"
#include "arm_math.h"
#include "cmsis_os.h"
#include "stdbool.h"
#include "ALE_STM32F4_DISCOVERY_Audio_Input_Driver.h"
#include "audio_processing.h"
#include "ff.h"
#include <ring_buffer.h>


//---------------------------------------------------------------------------------
//																		TASKS ARGUMENTS
//---------------------------------------------------------------------------------


typedef struct {
	bool			debug;
	bool			save_proc_vars;
	bool			save_clb_vars;
	bool			save_dist;
	uint32_t  usb_buff_size;
}Debug_conf;


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
  *	\brief Recognition task states
	*/
typedef enum {
	RECORD_AUDIO,
	RECOGNIZED,
}Reco_states;

/**
	*\typedef
	*	\enum
  *	\brief AudioSave states
	*/
typedef enum {
	RING_BUFFER_READY,
	FINISH,
}AudioSave_states;

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
	CHANGE_TASK,
	FINISH_SAVING,
	FINISH_PROCESSING,
	FINISH_READING,
	KILL_THREAD,
}Common_task_Messages;


/**
	*\typedef
	*	\struct
  *	\brief Audio Processing task arguments
	*/
typedef struct{
	osMessageQId src_msg_id;
	char *file_path;
	uint16_t *data;
	Proc_conf *proc_conf;
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
	Proc_conf *proc_conf;
	bool vad;
	bool save_to_files;
}File_Processing_args;

/**
	*\typedef
	*	\struct
  *	\brief Audio Capture task arguments
	*/
typedef struct{
	Capt_conf capt_conf;
	ringBuf *buff;
}Audio_Capture_args;

/**
	*\typedef
	*	\struct
  *	\brief Audio Read task arguments
	*/
typedef struct{
	Capt_conf capt_conf;
	ringBuf *buff;
	char * file_name;
	osMessageQId src_msg;
	bool init_complete;
}Audio_Read_args;


/**
	*\typedef
	*	\struct
  *	\brief Audio Save task arguments
	*/
typedef struct{
	uint32_t usb_buff_size;
	ringBuf *buff;
	char *file_name;
	Capt_conf capt_conf;
	osMessageQId src_msg;
}Audio_Save_args;

/**
	*\typedef
	*	\struct
  *	\brief Recognition task arguments
	*/
typedef struct{
	osMessageQId *msg_q;
	ringBuf 		 *buff;
	Debug_conf	 *debug_conf;
	Capt_conf 	 *capt_conf;
	Proc_conf		 *proc_conf;
}Patern_Storing_args;

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
	SAVE_LED 		= RLED,
	PROC_LED 		= BLED,
}AppLEDS;


typedef struct {
	AppStates maintask;

	Capt_conf		capt_conf;
	Proc_conf		proc_conf;
	Calib_conf	calib_conf;
	Debug_conf		debug_conf;	
	
	char patdir[13];
	char patfilename[13];
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
	float32_t MFCC[LIFTER_LENGTH];
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

//---------------------------------------------------------------------------------
//															GENERAL FUNCTIONS
//---------------------------------------------------------------------------------

void Configure_Application (void);
void setEnvVar (void);
uint8_t readConfigFile (const char *filename, AppConfig *Conf);
uint8_t readPaternsConfigFile (const char *filename, Patterns **Pat, uint32_t *pat_num);
uint8_t loadPattern (Patterns *pat, uint32_t vector_length);
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

void AudioSave (void const * pvParameters);

void AudioRead (void const * pvParameters);
//---------------------------------------------------------------------------------
//																INTERRUPTION FUNCTIONS
//---------------------------------------------------------------------------------
void User_Button_EXTI (void);



//---------------------------------------------------------------------------------
//																		TEST TASKS
//---------------------------------------------------------------------------------

#endif  // APPLICATION_H
