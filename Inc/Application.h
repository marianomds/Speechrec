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
#include "recognition.h"
#include "ff.h"
#include <ring_buffer.h>
#include "headerHMM.h"


//---------------------------------------------------------------------------------
//																		APPLICATION STATES
//---------------------------------------------------------------------------------


/**
	*\typedef
	*	\ENUM
  *	\brief Audio Capture STATES
	*/
typedef enum
{
	CAPTURING,
	PAUSE,
	NOT_CAPTURING,
}Capture_states;

/**
	*\typedef
	*	\enum
  *	\brief Application states
	*/
typedef enum
{
	APP_STOP = 0,
	APP_PATTERN_STORING = 1,
	APP_RECOGNITION = 2,
	APP_CALIBRATION = 3,
}App_states;


/**
	*\typedef
	*	\enum
  *	\brief AudioSave states
	*/
typedef enum
{
	PROCESSING,
	CALIBRATING,
	RECOGNIZING,
	NOTHING,
}Proc_states;


//---------------------------------------------------------------------------------
//																		APPLICATION TYPEDEF
//---------------------------------------------------------------------------------


/**
	*\typedef
	*	\ENUM
  *	\brief Audio Capture STATES
	*/
typedef struct
{
	bool			debug;
	bool			save_proc_vars;
	bool			save_clb_vars;
//	bool			save_dist;
	uint32_t  usb_buff_size;
}Debug_conf;


/**
	*\typedef
	*	\ENUM
  *	\brief Audio Capture STATES
	*/
typedef struct
{
	App_states maintask;

	Capt_conf		capt_conf;
	Proc_conf		proc_conf;
	VAD_conf		vad_conf;
	Calib_conf	calib_conf;
//	Reco_conf		reco_conf;
	Debug_conf	debug_conf;	
	
//	char patpath[15];
}AppConfig;


/**
	*\typedef
	*	\struct
  *	\brief Audio Capture task arguments
	*/
//typedef struct
//{
//	char pat_name[PATERN_MAX_NAME_SIZE];		/*< Nombre del Patron */
//	uint8_t	pat_actv_num;										/*< Numero de activación */
//	arm_matrix_instance_f32 pattern_mtx;		/*< Instancia de matriz para los atributos */
//}Patterns;


//---------------------------------------------------------------------------------
//																INTERRUPTION FUNCTIONS
//---------------------------------------------------------------------------------
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void User_Button_EXTI (void);

//---------------------------------------------------------------------------------
//															GENERAL FUNCTIONS
//---------------------------------------------------------------------------------

/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void Configure_Application (void);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void setEnvVar (void);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
uint8_t readConfigFile (const char *filename, AppConfig *Conf);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
//uint8_t readPaternsConfigFile (const char *filename, Patterns **Pat, uint32_t *pat_num);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
//uint8_t loadPattern (Patterns *pat, uint32_t vector_length, char *pat_path);

//---------------------------------------------------------------------------------
//															HMM FUNCTIONS
//---------------------------------------------------------------------------------

extern float32_t Tesis_loglik(float32_t * utterance, uint16_t T, const  float32_t * transmat1, const  float32_t * transmat2, const  float32_t * mixmat, const  float32_t * mu, const  float32_t * Sigma, const  float32_t * invSigma);

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
  * @brief  AudioProcess Task
	* @param  
  * @retval 
  */
void AudioProcess		(void const * pvParameters);
/**
  * @brief  Recognitiond Task
	* @param  
  * @retval 
  */
void Recognition			(void const * pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void AudioCalib (void const *pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void Keyboard 				(void const * pvParameters);
/**
	* @brief  Audio Capture Task
	* @param  
	* @retval 
	*/
void AudioCapture 		(void const * pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void AudioSave (void const * pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void AudioRead (void const * pvParameters);
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void Leds (void const * pvParameters);


//---------------------------------------------------------------------------------
//																		TASKS MESSAGES
//---------------------------------------------------------------------------------

/**
	*\typedef
	*	\enum
  *	\brief Common task messages
	*/
typedef enum {
	BUTTON_RELEASE=4,
	BUTTON_PRESS,
	FRAME_READY,
	FAIL,
	BUTTON_IRQ,
	FINISH_CALIBRATION,
	FINISH_SAVING,
	FINISH_PROCESSING,
	FINISH_RECOGNIZING,
	FINISH_READING,
	KILL_THREAD,
	START_CAPTURE,
	RESUME_CAPTURE,
	STOP_CAPTURE,
	PAUSE_CAPTURE,
	BUFFER_READY,
	KILL_CAPTURE,
	RING_BUFFER_READY,
	FINISH,
}Tasks_Messages;




//---------------------------------------------------------------------------------
//																		TASKS ARGUMENTS
//---------------------------------------------------------------------------------

/**
	*\typedef
	*	\struct
  *	\brief Audio Capture task arguments
	*/
typedef struct
{
	Capt_conf capt_conf;
	ringBuf *buff;
}Audio_Capture_args;

/**
	*\typedef
	*	\struct
  *	\brief Audio Read task arguments
	*/
typedef struct
{
	Capt_conf capt_conf;
	ringBuf *buff;
	char * file_name;
	osMessageQId src_msg;
}Audio_Read_args;

/**
	*\typedef
	*	\struct
  *	\brief Audio Save task arguments
	*/
typedef struct
{
	uint32_t usb_buff_size;
	ringBuf *buff;
	char *file_name;
	Capt_conf capt_conf;
	osMessageQId src_msg;
}Audio_Save_args;

/**
	*\typedef
	*	\struct
  *	\brief Pattern Storing task arguments
	*/
typedef struct
{
	osMessageQId *msg_q;
	ringBuf 		 *buff;
	Debug_conf	 *debug_conf;
	Capt_conf 	 *capt_conf;
	Proc_conf		 *proc_conf;
	VAD_conf		 *vad_conf;
//	Reco_conf		 *reco_conf;
	bool				recognize;
}Audio_Process_args;

/**
	*\typedef
	*	\struct
  *	\brief AudioCalib task
	*/
typedef struct
{
	osMessageQId *msg_q;
	ringBuf 		 *buff;
	Debug_conf	 *debug_conf;
	Capt_conf 	 *capt_conf;
	Proc_conf		 *proc_conf;
	VAD_conf		 *vad_conf;
	Calib_conf	 *calib_conf;
	
	osMessageQId src_msg_id;
	uint32_t 		 src_msg_val;
}Audio_Calibration_args;

/**
	*\typedef
	*	\struct
  *	\brief Recognition task arguments
	*/
typedef struct
{
	Proc_conf		 *proc_conf;
//	Reco_conf		 *reco_conf;
	char 				 *utterance_path;
//	char 				 *patterns_path;
//	bool         save_dist;
	osMessageQId src_msg_id;
	uint32_t 		 src_msg_val;
}Recognition_args;



#endif  // APPLICATION_H


