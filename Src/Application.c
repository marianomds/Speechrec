/**
  ******************************************************************************
  * @file    Aplication.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    05-Agosto-2014
  * @brief   Application Tasks for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "Application.h"
#include "stm32f4xx_hal.h"
#include "ff.h"
#include "float.h"
#include "usb_host.h"
#include "string.h"
#include "stdlib.h"
#include "minIni.h"
#include "recognition.h"
#include "misc.h"

/* Private Variables used within Application ---------------------------------*/

extern char USBH_Path[4];     /* USBH logical drive path */
FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */

osMessageQId appli_event;
osMessageQId key_msg;

osMessageQId calibration_msg;
osMessageQId recognition_msg;
osMessageQId pattern_storring_msg;

osMessageQId file_processing_msg;
osMessageQId audio_processing_msg;

osMessageQId audio_capture_msg;
osMailQId audio_capture_mail;

uint32_t tick_start ,elapsed_time;

AppConfig appconf;	/* Configuration structure */
uint16_t *audio;

int8_t test = 0;
UINT bwrt;

FIL log_file;
//---------------------------------------
//						APPLICATIONS TASKS
//---------------------------------------

void Main_Thread (void const *pvParameters) {
	
	// Message variables
	osEvent event;
	osMessageQId *msgID = NULL;
	
	// Task variables
	osThreadId current_task_ID  = NULL;
	osThreadId audio_task_ID  = NULL;
	Recognition_args reco_args = {NULL};
	Audio_Capture_args audio_cap_args = {NULL};
	bool appstarted = false;
	
	//TODO: Titilar led
	LED_On(OLED);
	
	for(;;) {
		
		event = osMessageGet(appli_event, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v) {
				
				case APPLICATION_READY:
				{
					if (!appstarted)
					{
						//TODO: Se apaga la secuencia de leds
						LED_Off(OLED);
						
						/* Register the file system object to the FatFs module */
						if(f_mount(&USBDISKFatFs, (TCHAR const*)USBH_Path, 0) != FR_OK)
							Error_Handler();	/* FatFs Initialization Error */

						// Archivo de log
						if(f_open(&log_file,"log.txt",FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();		//TODO de prueba						
						
						/* Read Configuration File */
						readConfigFile(CONFIG_FILE_NAME,&appconf);

						/* Set Environment variables */
						setEnvVar();
		
						// Allocate memory for audio buffer
						if( (audio = pvPortMalloc(appconf.proc_conf.frame_net*sizeof(*audio)) ) == NULL )		Error_Handler();
												
						// Create Audio Capture Task
						audio_cap_args.audio_conf = appconf.audio_capture_conf;
						audio_cap_args.data	= audio;
						audio_cap_args.data_buff_size = appconf.proc_conf.frame_net;
						osThreadDef(AudioCaptureTask, AudioCapture, osPriorityHigh,	1, configMINIMAL_STACK_SIZE*12);
						audio_task_ID = osThreadCreate (osThread(AudioCaptureTask), &audio_cap_args);
					
						/* If VAD was configured*/
						if(appconf.vad)
						{
							/* Create Calibration Task */
							osThreadDef(CalibrationTask,		Calibration, 		osPriorityNormal,	1, configMINIMAL_STACK_SIZE*10);
							current_task_ID = osThreadCreate (osThread(CalibrationTask), NULL);
							msgID = &calibration_msg;
						}
						
						appstarted = true;
					}
					break;
				}
					
				case APPLICATION_DISCONNECT:
				{
					if(appstarted)
					{
						// Send KILL message to running task
						if(*msgID != NULL)
							osMessagePut(*msgID,KILL_THREAD,0);

						// Send KILL message to Auido Capture Task
						osMessagePut (audio_capture_msg, KILL_CAPTURE, 0);
						
						// Kill message queue
						msgID = NULL;

						// Release memory
						vPortFree(audio);																		audio = NULL;
						vPortFree(reco_args.patterns_path);									reco_args.patterns_path = NULL;
						vPortFree(reco_args.patterns_config_file_name);			reco_args.patterns_config_file_name = NULL;
						
						// Desmonto el FileSystem del USB
						f_mount(NULL, (TCHAR const*)"", 0);
						
						//TODO: Secuencia de leds
						//TODO: Titilar led
						LED_On(OLED);

						// Set appstarted to false
						appstarted = false;
					}
					break;
				}					
				case BUTTON_RELEASE:
				{
					if(appstarted && msgID != NULL)
						osMessagePut(*msgID,BUTTON_RELEASE,0);
					break;
				}
				case BUTTON_PRESS:
				{
					if(appstarted && msgID != NULL)
						osMessagePut(*msgID,BUTTON_PRESS,0);
					break;
				}
				case CHANGE_TASK:
				{
					// Send kill to running task if necesary
					if (msgID != NULL)
					{
						osMessagePut(*msgID,KILL_THREAD,0);
						msgID = NULL;
						current_task_ID = NULL;
					}
					
					// Release memory
					vPortFree(reco_args.patterns_path);									reco_args.patterns_path = NULL;
					vPortFree(reco_args.patterns_config_file_name);			reco_args.patterns_config_file_name = NULL;
					
					// Set new task
					switch (appconf.maintask)
					{
						case CALIBRATION:
						{
							/* Create Calibration Task */
							osThreadDef(CalibrationTask,		Calibration, 		osPriorityNormal,	1, configMINIMAL_STACK_SIZE*10);
							current_task_ID = osThreadCreate (osThread(CalibrationTask), NULL);
							msgID = &calibration_msg;
							break;
						}
						
						case PATTERN_STORING:
						{
							/* Create Pattern_Storing Task */
							osThreadDef(PatternStoringTask,	PatternStoring, osPriorityNormal,	1, configMINIMAL_STACK_SIZE*5);
							current_task_ID = osThreadCreate (osThread(PatternStoringTask), NULL);
							msgID = &pattern_storring_msg;

							break;
						}
						case RECOGNITION:
						{
							// Set task arguments to pass
							if( (reco_args.patterns_path  = pvPortMalloc(strlen(appconf.patdir))) == NULL )										Error_Handler();
							if( (reco_args.patterns_config_file_name = pvPortMalloc(strlen(appconf.patfilename))) == NULL )		Error_Handler();
							strcpy(reco_args.patterns_path, appconf.patdir);
							strcpy(reco_args.patterns_config_file_name, appconf.patfilename);
							
							/* Create Tasks*/
							osThreadDef(RecognitionTask, 		Recognition, 		osPriorityNormal, 1, configMINIMAL_STACK_SIZE*8);
							current_task_ID = osThreadCreate (osThread(RecognitionTask), &reco_args);
							msgID = &recognition_msg;

							break;
						}
						default:
							break;
					}
				break;
				}
				default:
					break;
			}
		}
	}
}
void PatternStoring (void const *pvParameters) {

	// Message variables
	osEvent event;
	Mail *mail;
	Audio_Processing_args args_audio = {NULL};
	File_Processing_args args_file = {NULL};
	
	// Task Variables
	char file_name[] = {"PTRN_00"};
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool recording = false;
	
		// Create Pattern Sotring Message Queue
	osMessageQDef(pattern_storring_msg,10,uint32_t);
	pattern_storring_msg = osMessageCreate(osMessageQ(pattern_storring_msg),NULL);
	
	// Turn on LED
	LED_On((Led_TypeDef)PATTERN_LED);
	
	/* START TASK */
	for (;;)
	{
		// Espero por mensaje
		event = osMessageGet(pattern_storring_msg, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch(event.value.v)
			{			
				case BUTTON_PRESS:
				{
					// check if it is still processing audio
					if (processing)
						continue;
						
					// Check if filename exist, otherwise update
					for(; f_stat(file_name,NULL)!= FR_NO_FILE; updateFilename(file_name));
					
					// Create subdirectory for saving files here
					f_mkdir (file_name);

					//Send mail to Audio Capture Task to start
					mail = osMailAlloc(audio_capture_mail, osWaitForever); 															// Allocate memory
					if (appconf.debug_conf.debug)
					{
						mail->file_path = pvPortMalloc(strlen(file_name)+1);																// Allocate memory for file_path
						strcpy(mail->file_path, file_name);																									// Set file_path
						mail->file_name = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory for file_name
						strcat(strcpy(mail->file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name with extension		
					}
					else
					{
						mail->file_path = NULL;
						mail->file_name = NULL;
					}
					mail->src_msg_id = pattern_storring_msg;																						// Set Message ID
					osMessagePut(audio_capture_msg, START_CAPTURE, osWaitForever);											// Send Message to Audio Capture Task
					osMailPut(audio_capture_mail, mail);																								// Send Mail
					
					// Set recording state
					recording = true;

					if(!appconf.debug_conf.debug)
					{
						// Create arguments for passing to Audio Processing Task
						args_audio.file_path = pvPortMalloc(strlen(file_name)+1);				// Allocate memory
						strcpy(args_audio.file_path, file_name);												// Set file_path
						args_audio.src_msg_id = pattern_storring_msg;										// Set Message ID
						args_audio.data = audio;																				// Audio buffer
						args_audio.proc_conf = &appconf.proc_conf;											// Processing configuration
						args_audio.vad  = appconf.vad;																	// Set VAD variable
						args_audio.save_to_files  = false;															// Set if it should save to files
						
						//Create Audio Processing Task
						osThreadDef(ProcessTask, audioProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
						osThreadCreate (osThread(ProcessTask), &args_audio);
						
						// Set processing state
						processing = true;
					}
					break;
				}
				
				case BUTTON_RELEASE:
				{
					if(recording)
						// Send Message to Auido Capture Task
						osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
					
					break;
				}
				
				case FRAME_READY:
				{		
					if(recording && !appconf.debug_conf.debug)
						// Send Message to Audio Processing Task
						osMessagePut(audio_processing_msg, NEXT_FRAME, 0);
					
					break;
				}
				
				case END_CAPTURE:
				{
					if(recording)
					{
						if(!appconf.debug_conf.debug)
							// Send Message to Audio Processing Task
							osMessagePut(audio_processing_msg, LAST_FRAME, 0);
						else
						{
							/******** Create arguments for passing to Audio Processing Task ********/
							// File path
							args_file.file_path = pvPortMalloc(strlen(file_name)+1);																// Allocate memory
							strcpy(args_file.file_path, file_name);																									// Set file_path
							
							// File name
							args_file.file_name = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory
							strcat(strcpy(args_file.file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name (copy file_name and add extension)
							
							args_file.src_msg_id = pattern_storring_msg;										// Set Message ID
							args_file.proc_conf = &appconf.proc_conf;												// Processing Configuration
							args_file.vad = appconf.vad;																		// VAD
							args_file.save_to_files = appconf.debug_conf.save_proc_vars;		// Save to files
							
							//Create Audio Processing Task
							osThreadDef(ProcessTask, fileProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
							osThreadCreate (osThread(ProcessTask), &args_file);
							
							processing = true;
						}
							
						recording = false;
					}
					break;
				}
				
				case FINISH_PROCESSING:
				{
					if(processing)
					{
						// Free memory
						vPortFree(args_audio.file_path);				args_audio.file_path = NULL;
						vPortFree(args_file.file_path);					args_file.file_path = NULL;
						vPortFree(args_file.file_name);					args_file.file_name = NULL;

						processing = false;
					}
					break;
				}
				case KILL_THREAD:
				{
					// Send Message to Processing Task
					if(processing)
					{
						if(!appconf.debug_conf.debug)
							osMessagePut (audio_processing_msg, KILL_THREAD, 0);
						else
							osMessagePut (file_processing_msg, KILL_THREAD, 0);
						
						processing = false;
					}
					
					// Free memory
					vPortFree(args_audio.file_path);				args_audio.file_path = NULL;
					vPortFree(args_file.file_path);					args_file.file_path = NULL;
					vPortFree(args_file.file_name);					args_file.file_name = NULL;
					
					// Turn off LED
					LED_Off((Led_TypeDef)PATTERN_LED);
					
					//Kill message queue
					pattern_storring_msg = NULL;
					
					// Kill thread
					osThreadTerminate (osThreadGetId());
					
					break;
				}
				default:
					break;
			}
		}
	}
}

void Recognition (void const *pvParameters) {

	// Message variables
	osEvent event;
	Mail *mail;
	Audio_Processing_args args_audio = {NULL};
	File_Processing_args args_file = {NULL};
	Recognition_args *args = NULL;
	
	// Task Variables
	char file_name[] = {"REC_00"};
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool recording = false;
	Patterns *pat;
	uint32_t pat_num = 0;
	FIL utterance_file;
	uint32_t utterance_size;
	float32_t *utterance_data;
	arm_matrix_instance_f32 utterance_mtx;
	float32_t *dist;
	uint32_t pat_reco, bytesread;
	FIL result;
	
	// Get arguments
	args = (Recognition_args*) pvParameters;
	
	// Create Recognition Message Queue
	osMessageQDef(recognition_msg,10,uint32_t);
	recognition_msg = osMessageCreate(osMessageQ(recognition_msg),NULL);
		
	// Read Paterns file
	if (f_chdir (args->patterns_path) != FR_OK)																			Error_Handler();		// Go to file path
	if (!readPaternsConfigFile (args->patterns_config_file_name, &pat, &pat_num))		Error_Handler();		// Read Patterns File
	if (f_chdir ("..") != FR_OK)																										Error_Handler();		// Go back to original directory
	
	/* START TASK */
	for (;;)
	{
		// Espero por mensaje
		event = osMessageGet(recognition_msg, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch(event.value.v)
			{			
				case BUTTON_PRESS:
				{
					// check if Audio Processing Task is still active
					if (processing)
						continue;
						
					// Check if filename exist, otherwise update
					for(; f_stat(file_name,NULL)!= FR_NO_FILE; updateFilename(file_name));
					
					// Create subdirectory for saving files here
					f_mkdir (file_name);
					
					//Send mail to Audio Capture Task to start
					mail = osMailAlloc(audio_capture_mail, osWaitForever); 																// Allocate memory
					if (appconf.debug_conf.debug)
					{
						mail->file_path = pvPortMalloc(strlen(file_name)+1);																// Allocate memory for file_path
						strcpy(mail->file_path, file_name);																									// Set file_path
						mail->file_name = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory for file_name
						strcat(strcpy(mail->file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name with extension		
					}
					else
					{
						mail->file_path = NULL;
						mail->file_name = NULL;
					}
					mail->src_msg_id = recognition_msg;																									// Set Message ID
					osMessagePut(audio_capture_msg, START_CAPTURE, osWaitForever);												// Send Message to Audio Capture Task
					osMailPut(audio_capture_mail, mail);																									// Send Mail

					// Set recording state
					recording = true;

					if(!appconf.debug_conf.debug)
					{
						// Create arguments for passing to Audio Processing Task
						args_audio.file_path = pvPortMalloc(strlen(file_name)+1);			// Allocate memory
						strcpy(args_audio.file_path, file_name);											// Set file_path
						args_audio.src_msg_id = recognition_msg;											// Set Message ID
						args_audio.data = audio;																			// Audio buffer
						args_audio.proc_conf = &appconf.proc_conf;										// Processing configuration
						args_audio.vad  = appconf.vad;																// Set VAD variable
						args_audio.save_to_files  = false;														// Set if it should save to files
						
						//Create Audio Processing Task
						osThreadDef(ProcessTask, audioProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
						osThreadCreate (osThread(ProcessTask), &args_audio);
						
						// Set processing state
						processing = true;
					}

					break;
				}
				
				case BUTTON_RELEASE:
				{
					if(recording)
						// Send Message to Auido Capture Task
						osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
					break;
				}
				
				case FRAME_READY:
				{
					if(recording && !appconf.debug_conf.debug)
						// Send Message to Audio Processing Task
						osMessagePut(audio_processing_msg, NEXT_FRAME, 0);
					break;
				}
				
				case END_CAPTURE:
				{
					if(recording)
					{
						if(!appconf.debug_conf.debug)
							osMessagePut(audio_processing_msg, LAST_FRAME, 0);			// Send Message to Audio Processing Task
						else
						{
							/******** Create arguments for passing to Audio Processing Task ********/
							// File path
							args_file.file_path = pvPortMalloc(strlen(file_name)+1);																// Allocate memory
							strcpy(args_file.file_path, file_name);																									// Set file_path
							
							// File name
							args_file.file_name = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory
							strcat(strcpy(args_file.file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name (copy file_name and add extension)
							
							args_file.src_msg_id = recognition_msg;													// Set Message ID
							args_file.proc_conf = &appconf.proc_conf;												// Processing Configuration
							args_file.vad = appconf.vad;																		// VAD
							args_file.save_to_files = appconf.debug_conf.save_proc_vars;		// Save to files
							
							//Create Audio Processing Task
							osThreadDef(ProcessTask, fileProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
							osThreadCreate (osThread(ProcessTask), &args_file);
							
							processing = true;
						}
							
						recording = false;
					}
					break;
				}
				
				case FINISH_PROCESSING:
				{
					if(processing)
					{
						// Free memory
						vPortFree(args_audio.file_path);				args_audio.file_path = NULL;
						vPortFree(args_file.file_path);					args_file.file_path = NULL;
						vPortFree(args_file.file_name);					args_file.file_name = NULL;
						
		/*********** RECOGNITION ************/
						// Turn on LED
						LED_On((Led_TypeDef)RECOG_LED);
		
						// Load spoken word
						if (f_chdir (file_name) != FR_OK)																									Error_Handler();		// Go to file path
						if (f_open(&utterance_file, "MFCC.bin", FA_OPEN_EXISTING | FA_READ) != FR_OK)			Error_Handler();		// Load utterance's MFCC
						utterance_size = f_size (&utterance_file);																														// Check data size
						utterance_data = pvPortMalloc(utterance_size);																												// Allocate memory
						if(f_read (&utterance_file, utterance_data, utterance_size, &bytesread) != FR_OK)	Error_Handler();		// Read data
						f_close(&utterance_file);																																							// Close file
						arm_mat_init_f32 (&utterance_mtx, (utterance_size / sizeof(*utterance_data)) / appconf.proc_conf.lifter_legnth , appconf.proc_conf.lifter_legnth, utterance_data);
						if (f_chdir ("..") != FR_OK)																											Error_Handler();		// Go back to original directory					 
						
						
						// Allocate memroy for distance and initialize
						dist = pvPortMalloc(pat_num * sizeof(*dist));
						for(int i=0; i < pat_num ; i++)
							dist[i] = FLT_MAX;
						
						
						// Search for minimun distance between patterns and utterance
						pat_reco = pat_num-1;
						for(int i=0; i < pat_num ; i++)
						{
							// Go to file path
							if (f_chdir (args->patterns_path) != FR_OK)
								Error_Handler();
							
							// load pattern
							if( !loadPattern (&pat[i]) )
								continue;
							
							// Go back to original directory
							if (f_chdir ("..") != FR_OK)
								Error_Handler();
							
							// Go to file path
							if (f_chdir (file_name) != FR_OK)
								Error_Handler();

							// Get distance
							dist[i] = dtw_reduce (&utterance_mtx, &pat[i].pattern_mtx, appconf.debug_conf.save_dist);								
							
							// Check if distance is shorter
							if( dist[i] <  dist[pat_reco])
								pat_reco = i;
							
							// Free memory
							vPortFree(pat[i].pattern_mtx.pData);
							pat[i].pattern_mtx.pData = NULL;
							pat[i].pattern_mtx.numCols = 0;
							pat[i].pattern_mtx.numRows = 0;
							
							if (f_chdir ("..") != FR_OK)
								Error_Handler();
						}
														
						// Record in file wich pattern was spoken
						if ( open_append (&result,"Spoken") != FR_OK)		Error_Handler();				// Load result file
						if (dist[pat_reco] != FLT_MAX)
							f_printf(&result, "%s %s\n", "Patron:", pat[pat_reco].pat_name);			// Record pattern name
						else
							f_printf(&result, "%s %s\n", "Patron:", "No reconocido");							// If no pattern was found
						f_close(&result);

						// Save distances
						f_open(&result,"Dist.bin",FA_WRITE | FA_OPEN_ALWAYS);				// Load result file
						for(int i=0 ; i<pat_num ; i++)
							f_write(&result,&dist[i],sizeof(float32_t),&bytesread);
						f_close(&result);
						
						// Free memory
						vPortFree(utterance_data);				utterance_data = NULL;
						vPortFree(dist);									dist = NULL;
						
						// Turn off LED
						LED_Off((Led_TypeDef)RECOG_LED);
		/*************************************/
						
						// Set processing to false
						processing = false;
					}
					break;
				}
				case KILL_THREAD:
				{
					// Send Message to Processing Task
					if(processing)
					{
						if(!appconf.debug_conf.debug)
							osMessagePut (audio_processing_msg, KILL_THREAD, 0);
						else
							osMessagePut (file_processing_msg, KILL_THREAD, 0);
						
						processing = false;
					}
					
					// Free memory
					vPortFree(args_audio.file_path);				args_audio.file_path = NULL;
					vPortFree(args_file.file_path);					args_file.file_path = NULL;
					vPortFree(args_file.file_name);					args_file.file_name = NULL;
					vPortFree(utterance_data);							utterance_data = NULL;
					vPortFree(dist);												dist = NULL;
					
					// Kill message queue
					recognition_msg = NULL;
					
					// Kill thread
					osThreadTerminate (osThreadGetId());
					
					break;
				}
				default:
					break;
			}
		}
	}
}
void Calibration (void const * pvParameters) {
	
	// Message variables
	osEvent event;
	Mail *mail;
	
	// Task Variables
	char file_name[] = {"CLB_00"};
	uint16_t *frame = NULL;
	uint32_t frame_num = 0, calib_length = 0;
	Proc_var ptr_vars_buffers;
	Proc_files *files = NULL;
	FIL WaveFile;
	UINT bytesread;
	CalibStatus calib_status = CALIB_INITIATED;
	
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool capture = false;
	
	// Create Calibration Message Queue
	osMessageQDef(calibration_msg,10,uint32_t);
	calibration_msg = osMessageCreate(osMessageQ(calibration_msg),NULL);
	
	// Init Calibration configuration
	if(appconf.debug_conf.debug && appconf.debug_conf.save_proc_vars)
		calib_status = initCalibration	(&calib_length, &appconf.calib_conf, &appconf.proc_conf, &ptr_vars_buffers);
	else
		calib_status = initCalibration	(&calib_length, &appconf.calib_conf, &appconf.proc_conf, NULL);
	
	// Check if filename exist, otherwise update
	for(; f_stat(file_name,NULL)!= FR_NO_FILE; updateFilename(file_name));
	
	// Create subdirectory for saving files here
	f_mkdir (file_name);
		
	//Send mail to Audio Capture Task to start
	mail = osMailAlloc(audio_capture_mail, osWaitForever); 															// Allocate memory
	if (appconf.debug_conf.debug)
	{
		mail->file_path = pvPortMalloc(strlen(file_name)+1);																// Allocate memory for file_path
		strcpy(mail->file_path, file_name);																									// Set file_path
		mail->file_name = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory for file_name
		strcat(strcpy(mail->file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name with extension		
	}
	else
	{
		mail->file_path = NULL;
		mail->file_name = NULL;
	}
	mail->src_msg_id = calibration_msg;																									// Set Message ID
	osMessagePut(audio_capture_msg, START_CAPTURE, osWaitForever);											// Send message to Audio Capture Task to start
	osMailPut(audio_capture_mail, mail);																								// Send Mail
	
	// Starts capture and processing
	capture = true;
	processing = true;
	
	// Turn-on LED
	LED_On((Led_TypeDef)CALIB_LED);
	
	for(;;)
	{
		// Espero por mensaje
		event = osMessageGet(calibration_msg, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch(event.value.v)
			{
				case FRAME_READY:
				{
					if(capture)
					{
						// Process frame
						if(!appconf.debug_conf.debug && calib_status != CALIB_FINISH)
							calib_status =	Calibrate(audio,frame_num);
						
						// Send Finish Message to Audio Capture Task
						if (++frame_num == calib_length)
						{
							osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
							capture = false;
						}
					}

					break;
				}
				case END_CAPTURE:
				{
					// Go to audio file directory
					if (f_chdir (file_name) != FR_OK)
						Error_Handler();
						
					// Process frame
					if(appconf.debug_conf.debug)
					{						
						// Open audio file
						char *file_aux = pvPortMalloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory
						strcat(strcpy(file_aux, file_name),AUDIO_FILE_EXTENSION);														// Set file_name (copy file_name and add extension)
						if(f_open(&WaveFile,file_aux,FA_READ) != FR_OK)
							Error_Handler();
						vPortFree(file_aux);
						
						// Open files to save
						if(appconf.debug_conf.save_proc_vars)
						{
							files = pvPortMalloc(sizeof(Proc_files));
							Open_proc_files (files, true);
						}

						// Go where audio starts
						if(f_lseek(&WaveFile,44) != FR_OK)						
							Error_Handler();
						
						// Allocate memory for frame buffer
						frame = pvPortMalloc(appconf.proc_conf.frame_net * sizeof(*frame));
						
						// Process data until the file reachs the end
						for(frame_num = 0; frame_num < calib_length && !f_eof (&WaveFile); frame_num++)
						{
							// Leo un Frame del archivo
							if(f_read (&WaveFile, frame, appconf.proc_conf.frame_net * sizeof(*frame), &bytesread) != FR_OK)
								Error_Handler();
								
							// Processo el frame
							Calibrate(frame, frame_num);
							
							// Save to files
							if(appconf.debug_conf.save_proc_vars)
								Append_proc_files (files, &ptr_vars_buffers, true);
						}
						
						// Cierro los archivos
						f_close(&WaveFile);
						if(appconf.debug_conf.save_proc_vars)
							Close_proc_files (files, true);
					}					
					
					//End calibration
					endCalibration(appconf.debug_conf.debug & appconf.debug_conf.save_vad_vars);
					
					// Set processing state to false
					processing = false;
						
					// Go back to original directory
					if (f_chdir ("..") != FR_OK)
						Error_Handler();					
					
					// Finish Thread
					osMessagePut(calibration_msg, KILL_THREAD, 0);
					
					break;
				}
				case KILL_THREAD:
				{
					if (processing)						
						//End calibration
						endCalibration(false);

					// Free memory
					vPortFree(frame);
					vPortFree(files);

					// Turn off LED
					LED_Off((Led_TypeDef)CALIB_LED);
					
					//Kill message queue
					calibration_msg = NULL;

					// Send message to change task
					osMessagePut(appli_event,CHANGE_TASK,osWaitForever);
					
					// Kill thread
					osThreadTerminate (osThreadGetId());
					
					break;
				}
				default:
					break;
			}
		}
	}
}
void audioProcessing (void const *pvParameters) {
	
	// Message variables
	Audio_Processing_args *args;
	osEvent event;
	
	// Task variables
	UINT byteswritten;
	FIL MFCCFile;
	bool finish = false;
	uint32_t frameNum=0;
	float32_t *MFCC;
	uint32_t MFCC_size;
	
	// Create Process Task State MessageQ
	osMessageQDef(audio_processing_msg,10,uint32_t);
	audio_processing_msg = osMessageCreate(osMessageQ(audio_processing_msg),NULL);	
	
	// Get arguments
	args = (Audio_Processing_args*) pvParameters;
	
	//---------------------------- START TASK ----------------------------
	for(;;)
	{
		// Turn on Processing LED
		LED_On(BLED);
		
		// Init processing
		initProcessing(&MFCC, &MFCC_size, args->proc_conf, args->vad, NULL);

		// Go to file path
		if (f_chdir (args->file_path) != FR_OK)
			Error_Handler();

		// Open MFCC file (where variables will be save)
		if(f_open(&MFCCFile, "MFCC.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
			Error_Handler();
		
		// Go back to original directory
		if (f_chdir ("..") != FR_OK)
			Error_Handler();

		// START PROCESSING
		while(!finish)
		{
			event = osMessageGet(audio_processing_msg,osWaitForever);
			if(event.status == osEventMessage)
			{
				switch (event.value.v)
				{
					case LAST_FRAME:
					{
						finish = true;
					}
					case NEXT_FRAME:
					{
						// Process frame and write MFCC in a file
						if( MFCC_float (args->data) == VOICE)
							if(f_write(&MFCCFile, MFCC, MFCC_size, &byteswritten) != FR_OK)
								Error_Handler();
					
						// Incremento el Nº de Frame
						frameNum++;
						
						break;
					}
					case KILL_THREAD:
					{
						finish = true;
						break;
					}
					default:
						break;
				}
			}
		}
		
		// Cierro los archivos
		f_close(&MFCCFile);

		// De-Inicializo el proceso
		finishProcessing();
		
		// Send message to parent task telling it's finish
		osMessagePut(args->src_msg_id,FINISH_PROCESSING,0);

		// Turn off Processing LED
		LED_Off(BLED);
		
		// Destroy Message Que
		audio_processing_msg = NULL;
		
		// Elimino la tarea
		osThreadTerminate(osThreadGetId());
	}
}	

void fileProcessing (void const *pvParameters) {
	
	// Message variables
	File_Processing_args *args;
	osEvent event;													// Message Events
	
	// File processing variables
	UINT bytesread, byteswritten;
	FIL WaveFile, MFCCFile;
	bool finish = false;
	Proc_files *files = NULL;
	Proc_var save_vars;
	uint32_t frameNum=0;
	uint16_t *frame;
	float32_t *MFCC;
	uint32_t MFCC_size;
	
	// Create Process Task State MessageQ
	osMessageQDef(file_processing_msg,10,uint32_t);
	file_processing_msg = osMessageCreate(osMessageQ(file_processing_msg),NULL);	
	
	// Get arguments
	args = (File_Processing_args*) pvParameters;
		
	//---------------------------- START TASK ----------------------------
	for(;;)
	{
		// Turn on Processing LED
		LED_On(BLED);
		
		// Go to file path
		if (f_chdir (args->file_path) != FR_OK)
			Error_Handler();

		// Open audio file
		if(f_open(&WaveFile,args->file_name,FA_READ) != FR_OK)
			Error_Handler();

		// Allocate memory for frame buffer
		frame = pvPortMalloc(appconf.proc_conf.frame_net * sizeof(*frame));

		// Open MFCC file (where variables will be save)
		if(f_open(&MFCCFile, "MFCC.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
			Error_Handler();
		
		// Init processing
		if(args->save_to_files)
		{
			initProcessing(&MFCC, &MFCC_size, args->proc_conf, args->vad, &save_vars);
			files = pvPortMalloc(sizeof(Proc_files));
			Open_proc_files (files,args->vad);
		}
		else
			initProcessing(&MFCC, &MFCC_size, args->proc_conf, args->vad, NULL);		
		
		// Go back to original directory
		if (f_chdir ("..") != FR_OK)
			Error_Handler();
		
		// Go where audio starts
		if(f_lseek(&WaveFile,44) != FR_OK)						
			Error_Handler();
		
		// START PROCESSING
		while(!finish)
		{
			// Check Kill message
			event = osMessageGet(file_processing_msg,0);
			if(event.status == osEventMessage && event.value.v == KILL_THREAD)
			{
				finish = true;
				break;
			}
				
			// Leo un Frame del archivo
			if(f_read (&WaveFile, frame, appconf.proc_conf.frame_net * sizeof(*frame), &bytesread) != FR_OK)
				Error_Handler();
			
			// Si estoy en el último Frame relleno el final del Frame con ceros de ser necesario
			if(bytesread < appconf.proc_conf.frame_net * sizeof(*frame))
			{
				memset(&frame[bytesread], 0, appconf.proc_conf.frame_net * sizeof(*frame) - bytesread);
				finish = true;
			}
			
			// Proceso el frame obtenido y escribo los MFCC en un archivo
			if (MFCC_float (frame) == VOICE)
				if(f_write(&MFCCFile, MFCC, MFCC_size, &byteswritten) != FR_OK)
					Error_Handler();					
			
			// Escribo los valores intermedios en archivos
			if(args->save_to_files)
				Append_proc_files (files, &save_vars, args->vad);
			
			// Incremento el Nº de Frame
			frameNum++;
		}

		// Free memory
		vPortFree(frame);
		vPortFree(files);
		
		// Cierro los archivos
		f_close(&WaveFile);
		f_close(&MFCCFile);
		if(args->save_to_files)
			Close_proc_files (files,args->vad);

		// De-Inicializo el proceso
		finishProcessing();
		
		// Send message to parent task telling it's finish
		osMessagePut(args->src_msg_id,FINISH_PROCESSING,0);

		// Turn off Processing LED
		LED_Off(BLED);
		
		// Delete Message Queue
		file_processing_msg = NULL;
		
		// Elimino la tarea
		osThreadTerminate(osThreadGetId());
	}
}
void AudioCapture (void const * pvParameters) {

	// Function variables
	bool finish;														// Indicates wether the process is finished
	bool save_to_file = true;
	uint16_t* data;
	uint32_t data_size;

	// Message Variables
	Audio_Capture_args *args;
	osMessageQId parent_msg_id;
	osEvent event;													// Message Events
	Mail *mail;															// Mail structure
	
	//Wave File variables
	WAVE_FormatTypeDef WaveFormat;					// Wave Header structre
	FIL WavFile;                   					// File object
  uint32_t audio_size;										// Size of the Wave File
	uint32_t byteswritten;     							// File write/read counts
	WAVE_Audio_config wave_config;

	// Set Wave Configuration
	wave_config.SampleRate = appconf.audio_capture_conf.audio_freq;
	wave_config.NbrChannels = appconf.audio_capture_conf.audio_channel_nbr;
	wave_config.BitPerSample = appconf.audio_capture_conf.audio_bit_resolution;

	// Create Mail
	osMailQDef(audio_capture_mail, 10, Mail);
	audio_capture_mail = osMailCreate(osMailQ(audio_capture_mail),NULL);

	// Create Message
	osMessageQDef(audio_capture_msg, 10, uint32_t);
	audio_capture_msg = osMessageCreate(osMessageQ(audio_capture_msg),NULL);

	// Get arguments
	args = (Audio_Capture_args*) pvParameters;
	data = args->data;
	data_size = args->data_buff_size;
	
	// Intialized Audio Driver
	if(initCapture(&args->audio_conf, data, data_size, audio_capture_msg, BUFFER_READY) != AUDIO_OK)
		Error_Handler();

	
	/* START TASK */
	for (;;) {
		event = osMailGet(audio_capture_mail,osWaitForever);
		if(event.status == osEventMail)
		{
			// Read mail
			mail = event.value.p;
			
			// Get parent message id
			parent_msg_id = mail->src_msg_id;

			// Check if it should save audio to file or not
			if (mail->file_name == NULL || mail->file_path == NULL)
				save_to_file = false;
			else
				save_to_file = true;
			
			if(save_to_file)
			{
				// Go to file path
				f_chdir (mail->file_path);
				
				// Create New Wave File
				if(newWavFile(mail->file_name,&WaveFormat,&WavFile, wave_config) != FR_OK)
					Error_Handler();
				//TODO: Send message back saying that it has fail
				
				//Init filesize
				audio_size = 0;
				
				// Go back to original directory
				if (f_chdir ("..") != FR_OK)
					Error_Handler();
			}

			// Free Mail memory
			vPortFree(mail->file_path);
			vPortFree(mail->file_name);
			osMailFree(audio_capture_mail,mail);
			
			// Set variable
			finish = false;

			while(!finish)
			{
				event = osMessageGet(audio_capture_msg,osWaitForever);
				if(event.status == osEventMessage)
				switch(event.value.v){
					case START_CAPTURE:
					{
						// Starts Capturing Audio Process
						if(!finish)
						{
							audioRecord ();
							LED_On((Led_TypeDef)EXECUTE_LED);
						}

						break;
					}
					
					case PAUSE_CAPTURE:
					{					
						// Pause Audio Record
						audioPause();
						
						// TODO: Que titile el led
						break;
					}
								
					case RESUME_CAPTURE:
					{
						audioResume();
						break;
					}
					
					case STOP_CAPTURE:
					{
						if(!finish)
						{
						// Pause Audio Record
						audioStop();
							
						if(save_to_file)
							closeWavFile(&WavFile, &WaveFormat, audio_size);
						
						// Send Message that finish capturing
						osMessagePut(parent_msg_id,END_CAPTURE,0);
						
						// Turn off led
						LED_Off((Led_TypeDef)EXECUTE_LED);
						
						f_close(&log_file);
						
						// Set finish variable
						finish = true;
						}
						break;
					}
					
					case BUFFER_READY:
					{
						if(save_to_file)
						{
							tick_start = osKernelSysTick();
							/* write buffer in file */
							if(f_write(&WavFile, data, data_size*sizeof(*data), (void*)&byteswritten) != FR_OK)
							{
								f_close(&WavFile);
								Error_Handler();
							}
							audio_size += byteswritten;
							
							elapsed_time = osKernelSysTick() - tick_start;
							f_printf(&log_file, "elapsed_time: %d\n", elapsed_time  );
						}
						
						// Send message back telling that the frame is ready
						osMessagePut(parent_msg_id,FRAME_READY,0);
						break;
					}
					case KILL_CAPTURE:
					{
						// Pause Audio Record
						audioStop();
						
						// Turn off led
						LED_Off((Led_TypeDef)EXECUTE_LED);
						
						// Set finish variable
						finish = true;
						
						// Kill message queue
						audio_capture_msg = NULL;
						audio_capture_mail = NULL;
						
						// Elimino la tarea
						osThreadTerminate(osThreadGetId());

						break;
					}
					default:
						break;
				}
			}
		}
	}
}

void Keyboard (void const * pvParameters) {
	osEvent event;
	static uint8_t state=0;
	uint8_t var;
	for(;;) {
		event = osMessageGet(key_msg,osWaitForever);
		if(event.status == osEventMessage) {
			
			osDelay(300);													// Waint 300msec for establishment time
			var = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0);
			
			// Pregunto para eliminiar falsos
			if(var != state)
			{
				state = var;
				if(var)
					osMessagePut(appli_event,BUTTON_PRESS,0);
				else
					osMessagePut(appli_event,BUTTON_RELEASE,0);
			}
//				
//			event = osMessageGet(key_msg,1000);
//			switch(event.status) {
//				case osEventMessage:
//					osMessagePut(audio_capture_msg,BUTTON_PRESS,500);
//					break;
//				case osEventTimeout:
//					osMessagePut(audio_capture_msg,BUTTON_RETAIN_1s,500);
//					break;
//				default:
//					break;
//			}
			
			HAL_NVIC_EnableIRQ(EXTI0_IRQn);				// Enable once more EXTI0 IRQ
		}
	}
}


//---------------------------------------
//						GENERAL FUNCTIONS
//---------------------------------------
/**
  * @brief  
  * @param  
  * @retval
  */
void Configure_Application (void) {
	
	/* Create Application State MessageQ */
	osMessageQDef(appli_event,10,uint16_t);
	appli_event = osMessageCreate(osMessageQ(appli_event),NULL);

	/* Create User Button MessageQ */
	osMessageQDef(key_msg,10,uint8_t);
	key_msg = osMessageCreate(osMessageQ(key_msg),NULL);
	
	/* Create Keyboard Task*/
	osThreadDef(KeyboardTask, Keyboard, osPriorityRealtime, 1, configMINIMAL_STACK_SIZE);
	osThreadCreate (osThread(KeyboardTask), NULL);
	
	/* Create Main Task*/
	osThreadDef(MainTask, Main_Thread, osPriorityNormal, 1, configMINIMAL_STACK_SIZE * 5);
	osThreadCreate (osThread(MainTask), NULL);
}
/**
  * @brief  
  * @param  
  * @retval
  */
void setEnvVar (void){
	//TODO: Calcular las variables que va a utilizar el sistema y dejar de usar los defines
}
/**
  * @brief  Lee el archivo de configuración del sistema
	*
	* @param  filename  [in] 	Filename to read
	* @param  appconf [out]	Puntero a la estructura de configuración
  * @retval OK-->"1"  Error-->"0"
  */
uint8_t readConfigFile (const char *filename, AppConfig *config) {
	
	// Read System configuration
	config->maintask	= (AppStates) ini_getl	("System", "MAIN_TASK", PATTERN_STORING,	filename);
	config->vad				= ini_getbool	("System", "VAD", 	true,		filename);
	
	// Read Debug configuration
	config->debug_conf.debug					= ini_getbool	("Debug", "Debug",	false,	filename);
	config->debug_conf.save_proc_vars	= ini_getbool	("Debug", "save_proc_vars",	false,	filename);
	config->debug_conf.save_vad_vars	= ini_getbool	("Debug", "save_vad_vars",	false,	filename);
	config->debug_conf.save_dist			= ini_getbool	("Debug", "save_dist",	false,	filename);
	
	// Read Auido configuration
	config->audio_capture_conf.audio_freq						= (uint16_t)	ini_getl("AudioConf", "FREQ",						AUDIO_IN_FREQ,					filename);
	config->audio_capture_conf.audio_bw_high				= (uint16_t) 	ini_getl("AudioConf", "BW_HIGH", 				AUDIO_IN_BW_HIGH,				filename);
	config->audio_capture_conf.audio_bw_low					= (uint16_t) 	ini_getl("AudioConf", "BW_LOW",					AUDIO_IN_BW_LOW,				filename);
	config->audio_capture_conf.audio_bit_resolution	= (uint8_t)		ini_getl("AudioConf", "BIT_RESOLUTION", AUDIO_IN_BIT_RESOLUTION,filename);
	config->audio_capture_conf.audio_channel_nbr		= (uint8_t)		ini_getl("AudioConf", "CHANNEL_NBR", 		AUDIO_IN_CHANNEL_NBR,		filename);
	config->audio_capture_conf.audio_volume					= (uint8_t)		ini_getl("AudioConf", "VOLUME",					AUDIO_IN_VOLUME,				filename);
	config->audio_capture_conf.audio_decimator			= (uint8_t)		ini_getl("AudioConf", "DECIMATOR", 			AUDIO_IN_DECIMATOR,			filename);
	

	// Read Speech Processing configuration
//	config->proc_conf.time_window		= (uint16_t) 	ini_getl("SPConf", "TIME_WINDOW", 	TIME_WINDOW,		filename);
//	config->proc_conf.time_overlap	= (uint16_t) 	ini_getl("SPConf", "TIME_OVERLAP",	TIME_OVERLAP,		filename);
	
	config->proc_conf.frame_len			= (uint16_t)	ini_getl("SPConf", "FRAME_LEN", 		FRAME_LEN,			filename);
	config->proc_conf.frame_overlap	= (uint16_t)	ini_getl("SPConf", "FRAME_OVERLAP", FRAME_OVERLAP,	filename);
	config->proc_conf.frame_net			= config->proc_conf.frame_len - config->proc_conf.frame_overlap;

	config->proc_conf.numtaps				= (uint16_t)		ini_getl("SPConf", "NUMTAPS",				NUMTAPS,				filename);
	config->proc_conf.alpha					= (float32_t)		ini_getf("SPConf", "ALPHA",					ALPHA,					filename);
	config->proc_conf.fft_len				= (uint16_t)		ini_getl("SPConf", "FFT_LEN", 			FFT_LEN,				filename);
	config->proc_conf.mel_banks			= (uint16_t)		ini_getl("SPConf", "MEL_BANKS",			MEL_BANKS,			filename);
	config->proc_conf.ifft_len			= (uint16_t)		ini_getl("SPConf", "IFFT_LEN", 			IFFT_LEN,				filename);
	config->proc_conf.lifter_legnth	= (uint16_t)		ini_getl("SPConf", "LIFTER_LEGNTH",	LIFTER_LEGNTH,	filename);
	
	// Read Calibration configuration
	config->calib_conf.calib_time		= (uint16_t)	ini_getl("CalConf", "CALIB_TIME", CALIB_TIME, filename);
	config->calib_conf.calib_len		= (uint32_t)	floorf((config->calib_conf.calib_time * config->audio_capture_conf.audio_freq ) / config->proc_conf.frame_net * 1.0);
	config->calib_conf.thd_scl_eng	= (float32_t)	ini_getf("CalConf", "THD_Scale_ENERGY", THD_Scl_ENERGY,		filename);
	config->calib_conf.thd_min_fmax	= (uint32_t)	ini_getf("CalConf", "THD_min_FMAX",			THD_min_FMAX,	filename);
	config->calib_conf.thd_min_fmax = (uint32_t)  config->calib_conf.thd_min_fmax * (config->proc_conf.fft_len/2) /(config->audio_capture_conf.audio_freq/2) * 1.0;
	config->calib_conf.thd_scl_sf		= (float32_t)	ini_getf("CalConf", "THD_Scale_SF",			THD_Scl_SF,				filename);
	
	
	// Read Patterns configuration
	ini_gets("PatConf", "PAT_DIR", 				PAT_DIR, 				config->patdir, 			sizeof(config->patdir), 			filename);
	ini_gets("PatConf", "PAT_CONF_FILE",	PAT_CONF_FILE,	config->patfilename,	sizeof(config->patfilename),	filename);

	return 1;
}
/**
  * @brief  Carga los patrones
	* 				Carga el archivo de configuración de los patrones y también los patrones en si.
	*
	* @param [in] 	Filename to read
	* @param [out]	Puntero a la estructura de patrones 
	* @param [out]	Number of Patterns
  * @retval 			OK-->"1"  Error-->"0"
  */
uint8_t readPaternsConfigFile (const char *filename, Patterns **Pat, uint32_t *pat_num) {
	
	char section[30];
	uint32_t i;
	
	// Count how many Patterns there are
	for ((*pat_num) = 0; ini_getsection((*pat_num), section, sizeof(section), filename) > 0; (*pat_num)++);
	
	// Allocate memory for the Patterns
	(*Pat) = (Patterns *) pvPortMalloc(sizeof(Patterns)*(*pat_num));
	if(!Pat)	return 0;

	// Loop for all sections ==> All Patterns
	for (i = 0; ini_getsection(i, section, sizeof(section), filename) > 0; i++)
	{
		// Get Pattern Name
		ini_gets(section, "name", "", (*Pat)[i].pat_name, sizeof((*Pat)->pat_name), filename);
		
		// Get Pattern Activation Number
		(*Pat)[i].pat_actv_num = (uint8_t) ini_getl(section, "activenum", 0, filename);
	}
		
	return 1;
}
/**
  * @brief  Read Atributes from Pattern File
	* @param  Filename to read
  * @retval OK-->"1"  Error-->"0"
  */
uint8_t loadPattern (Patterns *pat) {
	
	FIL File;
	UINT bytesread;
	uint8_t output = 0;
	struct VC *ptr;
	uint32_t file_size;
	uint32_t VC_length;
	
	// Abro la carpeta donde estan los patrones
	if (f_chdir (pat->pat_name) == FR_OK)
	{	
		//Abro el archivo
		if(f_open(&File, "MFCC.BIN", FA_READ) == FR_OK)		// Abro el archivo
		{
			// Me fijo el tamaño del archivo
			file_size = f_size (&File);
			VC_length = file_size / sizeof(float32_t);
			
			// Aloco la memoria necesaria para almacenar en memoria todos los Vectores de Características
			if( ( ptr = (struct VC *) pvPortMalloc(file_size) ) != NULL )
				// Leo el archivo con los patrones
				if(f_read (&File, ptr, file_size, &bytesread) == FR_OK)
				{
					output = 1;
					arm_mat_init_f32 (&pat->pattern_mtx, VC_length/ appconf.proc_conf.lifter_legnth , appconf.proc_conf.lifter_legnth, (float32_t *) ptr);
				}
			
			// Cierro el archivo
			f_close(&File);
		}
		
		// Go back to original directory
		f_chdir ("..");
	}

	
	return output;
}

//---------------------------------------
//				INTERRUPTION FUNCTIONS
//---------------------------------------
/**
  * @brief  
  * @param  
  * @retval
  */
void User_Button_EXTI (void) {
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);			// Disable EXTI0 IRQ
	osMessagePut(key_msg,BUTTON_IRQ,0);
}
//tick_start = osKernelSysTick();
//elapsed_time = osKernelSysTick() - tick_start;
//f_printf(&log_file, "elapsed_time: %d\n", elapsed_time  );
//f_close(&log_file);

//FIL nosirve;
//f_open(&nosirve,"noSirve.txt", FA_CREATE_ALWAYS | FA_WRITE);
//f_printf(&nosirve, "Esto es una mierda");
//f_close(&nosirve);	//TODO de prueba

//	osMessagePut(calibration_msg, END_CAPTURE, osWaitForever);		// BORRAR ESTA LINEA DE ABAJO
