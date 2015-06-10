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
#include <Application.h>
#include <stm32f4xx_hal.h>
#include <ff.h>
#include <float.h>
#include <usb_host.h>
#include <string.h>
#include <stdlib.h>
#include <minIni.h>
#include <misc.h>
#include <recognition.h>
#include <audio_processing.h>
#include <ring_buffer.h>

#include <ale_stdlib.h>

/* Private Variables used within Application ---------------------------------*/

extern char USBH_Path[4];     /* USBH logical drive path */
FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */

osMessageQId appli_event;
osMessageQId key_msg;
osMessageQId pattern_storring_msg;
osMessageQId audio_capture_msg;
osMessageQId audio_save_msg;
extern osMessageQId proc_msg;

//--------------------------------
// 					Debug info
//--------------------------------
size_t freertos_mem_allocated = 0;
size_t max_allocated = 0;
size_t allocated = 0;
size_t freed = 0;

uint8_t Main_T = 0;
uint8_t Keyboard_T = 0;
uint8_t AudioCapture_T = 0;
uint8_t AudioSave_T = 0;
uint8_t AudioRead_T = 0;
uint8_t AudioProc_T = 0;
uint8_t PatStoring_T = 0;
//---------------------------------

AppConfig appconf;

//--------------------------------
// 					Task Defines
//--------------------------------

osThreadDef(PatternStoring,	PatternStoring, osPriorityNormal,	1, configMINIMAL_STACK_SIZE*5);
osThreadDef(AudioCapture, AudioCapture, osPriorityHigh,	1, configMINIMAL_STACK_SIZE*12);
osThreadDef(AudioSave, AudioSave, osPriorityAboveNormal,	1, configMINIMAL_STACK_SIZE*7);
osThreadDef(AudioRead, AudioRead, osPriorityNormal,	1, configMINIMAL_STACK_SIZE*5);
osThreadDef(AudioProc, audioProc, osPriorityAboveNormal,	1, configMINIMAL_STACK_SIZE*30);

//---------------------------------------
//						APPLICATIONS TASKS
//---------------------------------------

void Main_Thread (void const *pvParameters) {
	
	// Message variables
	osEvent event;
	osMessageQId msgID = NULL;
	
	// Task variables
	osThreadId current_task_ID  = NULL;
	osThreadId audio_task_ID  = NULL;
	Audio_Capture_args audio_cap_args = {NULL};
	bool appstarted = false;
	ringBuf audio_ring_buffer;
	
	// Task arguments
	Patern_Storing_args pat_stor_args;
	Recognition_args reco_args = {NULL};
	
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
							Error_Handler("Error on mount USB");	/* FatFs Initialization Error */

						/* Read Configuration File */
						readConfigFile(CONFIG_FILE_NAME,&appconf);

						/* Set Environment variables */
						setEnvVar();
						
						// Create Audio Capture Task
						audio_cap_args.capt_conf = appconf.capt_conf;
						audio_cap_args.buff	= &audio_ring_buffer;
						audio_task_ID = osThreadCreate (osThread(AudioCapture), &audio_cap_args);
					
//						/* If VAD was configured*/
//						if(appconf.vad)
//						{
//							/* Create Calibration Task */
//							osThreadDef(Calibration,		Calibration, 		osPriorityNormal,	1, configMINIMAL_STACK_SIZE*10);
//							current_task_ID = osThreadCreate (osThread(Calibration), NULL);
//							msgID = &calibration_msg;
//						}
//						else
//						{
							osMessagePut(appli_event,CHANGE_TASK,osWaitForever);
//						}
						
						appstarted = true;
					}
					break;
				}
					
				case APPLICATION_DISCONNECT:
				{
					if(!appstarted)
						break;
					
					// Send KILL message to running task
					if (msgID != NULL)
					{
						osMessagePut(msgID,KILL_THREAD,0);
						current_task_ID = NULL;
					}

					// Send KILL message to Auido Capture Task
					osMessagePut (audio_capture_msg, KILL_CAPTURE, 0);
					
					// Release memory
					free(reco_args.patterns_path);									reco_args.patterns_path = NULL;
					free(reco_args.patterns_config_file_name);			reco_args.patterns_config_file_name = NULL;
					
					// Desmonto el FileSystem del USB
					f_mount(NULL, (TCHAR const*)"", 0);
					
					//TODO: Secuencia de leds
					//TODO: Titilar led
					LED_On(OLED);

					// Set appstarted to false
					appstarted = false;
					
					break;
				}					
				case BUTTON_RELEASE:
				{
					if(msgID != NULL)
						osMessagePut(msgID,BUTTON_RELEASE, osWaitForever);
					break;
				}
				case BUTTON_PRESS:
				{
					if(msgID != NULL)
						osMessagePut(msgID,BUTTON_PRESS,osWaitForever);
					break;
				}
				case CHANGE_TASK:
				{
					// Send kill to running task if necesary
					if (msgID != NULL)
					{
						osMessagePut(msgID,KILL_THREAD,osWaitForever);
						current_task_ID = NULL;
					}
					
					// Release memory
					free(reco_args.patterns_path);									reco_args.patterns_path = NULL;
					free(reco_args.patterns_config_file_name);			reco_args.patterns_config_file_name = NULL;
					
					// Set new task
					switch (appconf.maintask)
					{
						case CALIBRATION:
						{
//							/* Create Calibration Task */
//							osThreadDef(Calibration,		Calibration, 		osPriorityNormal,	1, configMINIMAL_STACK_SIZE*10);
//							current_task_ID = osThreadCreate (osThread(Calibration), NULL);
//							msgID = &calibration_msg;
							break;
						}
						
						case PATTERN_STORING:
						{
							/* Create Pattern_Storing Task */
							pat_stor_args.msg_q = &msgID;
							pat_stor_args.buff = &audio_ring_buffer;
							pat_stor_args.debug_conf = &appconf.debug_conf;
							pat_stor_args.capt_conf = &appconf.capt_conf;
							pat_stor_args.proc_conf = &appconf.proc_conf;
							current_task_ID = osThreadCreate (osThread(PatternStoring), &pat_stor_args);

							break;
						}
						case RECOGNITION:
						{
//							// Set task arguments to pass
//							if( (reco_args.patterns_path  = malloc(strlen(appconf.patdir))) == NULL )										Error_Handler();
//							if( (reco_args.patterns_config_file_name = malloc(strlen(appconf.patfilename))) == NULL )		Error_Handler();
//							strcpy(reco_args.patterns_path, appconf.patdir);
//							strcpy(reco_args.patterns_config_file_name, appconf.patfilename);
//							
//							/* Create Tasks*/
//							osThreadDef(Recognition, 		Recognition, 		osPriorityNormal, 1, configMINIMAL_STACK_SIZE*8);
//							current_task_ID = osThreadCreate (osThread(Recognition), &reco_args);
//							msgID = &recognition_msg;

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
	Patern_Storing_args *args;
	Audio_Save_args audio_save = {NULL};
	Audio_Read_args audio_read = {NULL};
	Proc_args audio_proc_args = {NULL};
	
	// Task Variables
	ringBuf audio_read_buff, features_buff = {NULL};
	uint8_t features_buff_client_num;
	char file_path[] = {"PTRN_00"};
	char file_name[ (strlen(file_path)+1)*2 + strlen(AUDIO_FILE_EXTENSION) ];
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool recording = false;
	
	// Para salvar los features
	float32_t *features;
	FIL features_file;
	UINT bytes_written;
	
	// Get arguments
	args = (Patern_Storing_args *) pvParameters;
	
	// Create Pattern Sotring Message Queue
	osMessageQDef(pattern_storring_msg,5,uint32_t);
	pattern_storring_msg = osMessageCreate(osMessageQ(pattern_storring_msg),NULL);
	*(args->msg_q) = pattern_storring_msg;
	
	// Alloco memoria para los features
	features = malloc( (1+args->proc_conf->lifter_length) * 3 * sizeof(*features));
	
	// Turn on LED
	LED_On((Led_TypeDef)PATTERN_LED);
	
	
	
//	uint8_t *aux;
//	uint8_t test_client;
	
	
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
					if (processing || recording)
						break;
						
					// Check if filename exist, otherwise update
					for(; f_stat(file_path,NULL)!= FR_NO_FILE; updateFilename(file_path));
					
					// Create subdirectory for saving files here
					f_mkdir (file_path);

					// Set file_name "PTRN_xx/PTR_xx.WAV"
					strcpy(file_name, file_path);
					strcat(file_name, "/");
					strcat(file_name, file_path);
					strcat(file_name, AUDIO_FILE_EXTENSION);
					
					// Open file for saving features
					f_chdir(file_path);
					f_open(&features_file, "features.bin", FA_CREATE_ALWAYS | FA_WRITE );
					f_chdir("..");
					
					// Record audio and then process file
					if (args->debug_conf->debug)
					{
				 		// Create Audio Save Task
						audio_save.usb_buff_size = args->debug_conf->usb_buff_size;
						audio_save.buff	= args->buff;
						audio_save.capt_conf = *args->capt_conf;
						audio_save.file_name = file_name;
						audio_save.src_msg = pattern_storring_msg;
						osThreadCreate (osThread(AudioSave), &audio_save);
					}
					else
					// Process Audio in Real Time
					{
						// Start Processing Audio
						audio_proc_args.audio_buff = args->buff;
						audio_proc_args.features_buff = &features_buff;
						audio_proc_args.proc_conf = args->proc_conf;
						audio_proc_args.save_to_files = args->debug_conf->save_proc_vars;
						audio_proc_args.src_msg_id =  pattern_storring_msg;
						audio_proc_args.src_msg_val = FINISH_PROCESSING;
						audio_proc_args.path = file_path;
						audio_proc_args.init_complete = false;
						osThreadCreate (osThread(AudioProc), &audio_proc_args);

						// Espero a que este todo inicializado
						while(!audio_proc_args.init_complete);
						
						// Me registro como cliente en el buffer que crea audio_proc
						ringBuf_registClient ( &features_buff,	(1 + args->proc_conf->lifter_length) * sizeof(float32_t),
																		(1 + args->proc_conf->lifter_length)  * sizeof(float32_t), pattern_storring_msg,
																		FRAME_READY,	&features_buff_client_num);
																		
						processing = true;
					}
					
					// Start capturing audio
					osMessagePut(audio_capture_msg,START_CAPTURE,osWaitForever);
					recording = true;
					break;
				}
				
				case BUTTON_RELEASE:
				{
					if( !recording)
						break;
					
					// Send Message to Auido Capture Task
					osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
										
					recording = false;
					
					if (args->debug_conf->debug)
						// Send Message to Auido Save Task
						osMessagePut(audio_save_msg, FINISH, osWaitForever);
					else
						// Send Message to Auido Procesing Task
						osMessagePut(proc_msg, PROC_FINISH, osWaitForever);
					break;
				}
				case FINISH_SAVING:
				{
					if (args->debug_conf->debug)
					{
						// Create Read Audio Task
						audio_read.buff = &audio_read_buff;
						audio_read.capt_conf = *args->capt_conf;
						audio_read.file_name = file_name;
						audio_read.src_msg = pattern_storring_msg;
						audio_read.init_complete = false;
						osThreadCreate (osThread(AudioRead), &audio_read);
						
						// Espero a que este todo inicializado
						while(!audio_read.init_complete);
						
						// Start Processing Audio
						audio_proc_args.audio_buff = &audio_read_buff;
						audio_proc_args.features_buff = &features_buff;
						audio_proc_args.proc_conf = args->proc_conf;
						audio_proc_args.save_to_files = args->debug_conf->save_proc_vars;
						audio_proc_args.src_msg_id =  pattern_storring_msg;
						audio_proc_args.src_msg_val = FINISH_PROCESSING;
						audio_proc_args.path = file_path;
						audio_proc_args.init_complete = false;
						osThreadCreate (osThread(AudioProc), &audio_proc_args);
						
						// Espero a que este todo inicializado
						while(!audio_proc_args.init_complete);
						
						// Me registro como cliente en el buffer que crea audio_proc
						ringBuf_registClient ( &features_buff,	(1 + args->proc_conf->lifter_length) * sizeof(float32_t),
																		(1 + args->proc_conf->lifter_length)  * sizeof(float32_t), pattern_storring_msg,
																		FRAME_READY,	&features_buff_client_num);


//						ringBuf_registClient ( &audio_read_buff, args->proc_conf->frame_overlap * 1 * sizeof(uint16_t),
//																		args->proc_conf->frame_overlap	* sizeof(uint16_t),	pattern_storring_msg, FRAME_READY,	&test_client);
//						// Allocate space for aux variable
//						if ( (aux = malloc(args->proc_conf->frame_overlap * sizeof(uint16_t) )) == NULL)
//							Error_Handler("Error on malloc aux in audio processing");					
					
					
						processing = true;
					}

					break;
				}
				case FINISH_READING:
				{
					// Send finish message
					if(!processing)
						break;
					
					osMessagePut(proc_msg, PROC_FINISH, osWaitForever);
					
//					size_t pepe;
//					ringBuf_read_all( &audio_read_buff, test_client, aux, &pepe);
//					free(aux);
//					processing = false;

					break;
				}
				case FRAME_READY:
				{
					// Write down features
					if(processing)
						while (ringBuf_read_const ( &features_buff, features_buff_client_num, (uint8_t*) features ) == BUFF_OK)
							f_write(&features_file, features, (1+args->proc_conf->lifter_length) * 3 * sizeof(*features), &bytes_written);
					
//					while (ringBuf_read_const( &audio_read_buff, test_client, aux) == BUFF_OK)
//						printf("Datoooooooooooooooooooo\n");
					
					break;
				}
				case FINISH_PROCESSING:
				{
					if(processing)
					{
						ringBuf_read_all ( &features_buff, features_buff_client_num, (uint8_t*) features, &bytes_written );
						f_write(&features_file, features, bytes_written, &bytes_written);
						f_close(&features_file);
						processing = false;
					}
					break;
				}
				case KILL_THREAD:
				{
					// Stop Recording Tasks
					if (recording)
					{
						osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
						osMessagePut(audio_save_msg, FINISH, osWaitForever);
					}
					
					// Stop Processing Task
					if(processing)
					{
						ringBuf_unregistClient(&features_buff, features_buff_client_num);
						f_close(&features_file);
						osMessagePut(audio_proc_args.proc_msg_id, KILL_THREAD, osWaitForever);
						processing = false;
					}
					
					// Free memory
					free(features);
					
					// Turn off LED
					LED_Off((Led_TypeDef)PATTERN_LED);
					
					//Kill message queue
					osMessageDelete(&pattern_storring_msg);
					
					// Kill thread
					osThreadTerminate(osThreadGetId());
					
					break;
				}
				default:
					break;
			}
		}
	}
}

#ifdef ahora_no
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
						mail->file_path = malloc(strlen(file_name)+1);																// Allocate memory for file_path
						strcpy(mail->file_path, file_name);																									// Set file_path
						mail->file_name = malloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory for file_name
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
						args_audio.file_path = malloc(strlen(file_name)+1);			// Allocate memory
						strcpy(args_audio.file_path, file_name);											// Set file_path
						args_audio.src_msg_id = recognition_msg;											// Set Message ID
						args_audio.data = audio;																			// Audio buffer
						args_audio.proc_conf = &appconf.proc_conf;										// Processing configuration
						args_audio.vad  = appconf.vad;																// Set VAD variable
						args_audio.save_to_files  = false;														// Set if it should save to files
						
						//Create Audio Processing Task
						osThreadDef(Process, audioProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
						osThreadCreate (osThread(Process), &args_audio);
						
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
						osMessagePut(proc_msg, NEXT_FRAME, 0);
					break;
				}
				
				case END_CAPTURE:
				{
					if(recording)
					{
						if(!appconf.debug_conf.debug)
							osMessagePut(proc_msg, LAST_FRAME, 0);			// Send Message to Audio Processing Task
						else
						{
							/******** Create arguments for passing to Audio Processing Task ********/
							// File path
							args_file.file_path = malloc(strlen(file_name)+1);																// Allocate memory
							strcpy(args_file.file_path, file_name);																									// Set file_path
							
							// File name
							args_file.file_name = malloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory
							strcat(strcpy(args_file.file_name, file_name),AUDIO_FILE_EXTENSION);										// Set file_name (copy file_name and add extension)
							
							args_file.src_msg_id = recognition_msg;													// Set Message ID
							args_file.proc_conf = &appconf.proc_conf;												// Processing Configuration
							args_file.vad = appconf.vad;																		// VAD
							args_file.save_to_files = appconf.debug_conf.save_proc_vars;		// Save to files
							
							//Create Audio Processing Task
							osThreadDef(Process, fileProcessing,	osPriorityAboveNormal, 1, configMINIMAL_STACK_SIZE * 22);
							osThreadCreate (osThread(Process), &args_file);
							
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
						free(args_audio.file_path);				args_audio.file_path = NULL;
						free(args_file.file_path);					args_file.file_path = NULL;
						free(args_file.file_name);					args_file.file_name = NULL;
						
		/*********** RECOGNITION ************/
						// Turn on LED
						LED_On((Led_TypeDef)RECOG_LED);
		
						// Load spoken word
						if (f_chdir (file_name) != FR_OK)																									Error_Handler();		// Go to file path
						if (f_open(&utterance_file, "MFCC.bin", FA_OPEN_EXISTING | FA_READ) != FR_OK)			Error_Handler();		// Load utterance's MFCC
						utterance_size = f_size (&utterance_file);																														// Check data size
						utterance_data = malloc(utterance_size);																												// Allocate memory
						if(f_read (&utterance_file, utterance_data, utterance_size, &bytesread) != FR_OK)	Error_Handler();		// Read data
						f_close(&utterance_file);																																							// Close file
						arm_mat_init_f32 (&utterance_mtx, (utterance_size / sizeof(*utterance_data)) / appconf.proc_conf.lifter_length , appconf.proc_conf.lifter_length, utterance_data);
						if (f_chdir ("..") != FR_OK)																											Error_Handler();		// Go back to original directory					 
						
						
						// Allocate memroy for distance and initialize
						dist = malloc(pat_num * sizeof(*dist));
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
							free(pat[i].pattern_mtx.pData);
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
						if(appconf.debug_conf.save_dist)
						{
							f_chdir (file_name);
							f_open (&result, "Dist.bin", FA_WRITE | FA_OPEN_ALWAYS);				// Load result file
							f_write(&result, dist, pat_num*sizeof(*dist),&bytesread);
							f_close(&result);
							f_chdir ("..");
						}
						
						// Free memory
						free(utterance_data);				utterance_data = NULL;
						free(dist);									dist = NULL;
						
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
							osMessagePut (proc_msg, KILL_THREAD, 0);
						else
							osMessagePut (file_processing_msg, KILL_THREAD, 0);
						
						processing = false;
					}
					
					// Free memory
					free(args_audio.file_path);				args_audio.file_path = NULL;
					free(args_file.file_path);					args_file.file_path = NULL;
					free(args_file.file_name);					args_file.file_name = NULL;
					free(utterance_data);							utterance_data = NULL;
					free(dist);												dist = NULL;
					
					// Kill message queue
					osMessageDelete(&recognition_msg);
					
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
	Calib_status calib_status = CALIB_INITIATED;
	Proc_stages stages;
	
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool capture = false;
	
	// Create Calibration Message Queue
	osMessageQDef(calibration_msg,10,uint32_t);
	calibration_msg = osMessageCreate(osMessageQ(calibration_msg),NULL);
	
	// Init Calibration configuration
	if(appconf.debug_conf.debug && appconf.debug_conf.save_proc_vars)
		calib_status = initCalibration	(&calib_length, &appconf.Proc_conf, &appconf.proc_conf, &ptr_vars_buffers);
	else
		calib_status = initCalibration	(&calib_length, &appconf.Proc_conf, &appconf.proc_conf, NULL);
	
	// Check if filename exist, otherwise update
	for(; f_stat(file_name,NULL)!= FR_NO_FILE; updateFilename(file_name));
	
	// Create subdirectory for saving files here
	f_mkdir (file_name);
		
	//Send mail to Audio Capture Task to start
	mail = osMailAlloc(audio_capture_mail, osWaitForever); 															// Allocate memory
	if (appconf.debug_conf.debug)
	{
		mail->file_path = malloc(strlen(file_name)+1);																// Allocate memory for file_path
		strcpy(mail->file_path, file_name);																									// Set file_path
		mail->file_name = malloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory for file_name
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
							calib_status =	Calibrate(audio,frame_num, &stages);
						
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
						char *file_aux = malloc(strlen(file_name)+strlen(AUDIO_FILE_EXTENSION)+1);		// Allocate memory
						strcat(strcpy(file_aux, file_name),AUDIO_FILE_EXTENSION);														// Set file_name (copy file_name and add extension)
						if(f_open(&WaveFile,file_aux,FA_READ) != FR_OK)
							Error_Handler();
						free(file_aux);
						
						// Open files to save
						if(appconf.debug_conf.save_proc_vars)
						{
							files = malloc(sizeof(Proc_files));
							Open_proc_files (files, true);
						}

						// Go where audio starts
						if(f_lseek(&WaveFile,44) != FR_OK)						
							Error_Handler();
						
						// Allocate memory for frame buffer
						frame = malloc(appconf.proc_conf.frame_net * sizeof(*frame));
						
						// Process data until the file reachs the end
						for(frame_num = 0; frame_num < calib_length && !f_eof (&WaveFile); frame_num++)
						{
							// Leo un Frame del archivo
							if(f_read (&WaveFile, frame, appconf.proc_conf.frame_net * sizeof(*frame), &bytesread) != FR_OK)
								Error_Handler();
								
							// Processo el frame
							Calibrate(frame, frame_num, &stages);
							
							// Save to files
							if(appconf.debug_conf.save_proc_vars)
								Append_proc_files (files, &ptr_vars_buffers, true, stages);
						}
						
						// Cierro los archivos
						f_close(&WaveFile);
						if(appconf.debug_conf.save_proc_vars)
							Close_proc_files (files, true);
					}					
					
					//End calibration
					endCalibration(appconf.debug_conf.debug & appconf.debug_conf.save_clb_vars);
					
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
					free(frame);
					free(files);

					// Turn off LED
					LED_Off((Led_TypeDef)CALIB_LED);
					
					//Kill message queue
					osMessageDelete(&calibration_msg);

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
#endif
void AudioCapture (void const * pvParameters) {

	// Function variables
	uint16_t *audio_frame;
	Audio_Capture_args *args;
	osEvent event;
	
	// Create Message
	osMessageQDef(audio_capture_msg, 5, uint8_t);
	audio_capture_msg = osMessageCreate(osMessageQ(audio_capture_msg),NULL);

	// Get arguments
	args = (Audio_Capture_args*) pvParameters;
	
	// Allocate memory for audio frame
	if( (audio_frame = malloc( args->capt_conf.frame_size * sizeof(*audio_frame) ) ) == NULL )
		Error_Handler("Error on malloc audio_frame in AudioCapture");
						
	// Intialized Audio Driver
	if(initCapture(&args->capt_conf, audio_frame, args->capt_conf.frame_size, audio_capture_msg, BUFFER_READY) != AUDIO_OK)
		Error_Handler("Error on initCapturein AudioCapture");
			
	// Create buffer for audio
	if( ringBuf_init(args->buff, args->capt_conf.frame_size * args->capt_conf.ring_buff_size * sizeof(*audio_frame), true) != BUFF_OK )
		Error_Handler("Error on ring buffer init in AudioCapture");
	
	/* START TASK */
	for (;;) {
		event = osMessageGet(audio_capture_msg,osWaitForever);
		if(event.status == osEventMessage)
			switch(event.value.v){
				case START_CAPTURE:
				{
					// Starts Capturing Audio Process
					audioRecord ();
					LED_On((Led_TypeDef)SAVE_LED);

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
					// Resume Audio Record
					audioResume();
					break;
				}
				
				case STOP_CAPTURE:
				{
					// Pause Audio Record
					audioStop();
					
					// Turn off led
					LED_Off((Led_TypeDef)SAVE_LED);
					
					break;
				}
				
				case BUFFER_READY:
				{
					// Copy frame to buffer
					ringBuf_write ( args->buff, (uint8_t*) audio_frame, args->capt_conf.frame_size * sizeof(*audio_frame) );
					
					break;
				}
				case KILL_CAPTURE:
				{
					// Pause Audio Record
					audioStop();						
					
					// Remove buffer
					ringBuf_deinit(args->buff);
					
					// De-init capture
					deinitCapture();
						
					// Release memory
					free(audio_frame);
					
					// Kill message queue
					osMessageDelete(&audio_capture_msg);
					
					// Turn off led
					LED_Off((Led_TypeDef)SAVE_LED);
					
					// Elimino la tarea
					osThreadTerminate(osThreadGetId());

					break;
				}
				default:
					break;
			}
	}
}
void AudioSave (void const * pvParameters) {
	
	// Task Variables
	uint16_t *usb_buff;
	uint8_t buff_client;
	size_t read_size;
	Audio_Save_args *args;
	osEvent event;
	
	//Wave File variables
	WAVE_FormatTypeDef WaveFormat;					// Wave Header structre
	FIL WavFile;                   					// File object
  uint32_t file_size = 0;									// Size of the Wave File
	uint32_t byteswritten;     							// File write/read counts
	WAVE_Audio_config wave_config;

	// Get arguments
	args = (Audio_Save_args*) pvParameters;
	
	// Create Message
	osMessageQDef(audio_save_msg, 5, uint32_t);
	audio_save_msg = osMessageCreate(osMessageQ(audio_save_msg),NULL);

	// Set Wave Configuration
	wave_config.SampleRate = args->capt_conf.freq;
	wave_config.NbrChannels = args->capt_conf.channel_nbr;
	wave_config.BitPerSample = args->capt_conf.bit_resolution;
	
	// Allocate memory for buffer
	if ( (usb_buff = malloc(args->usb_buff_size * sizeof(*usb_buff))) == NULL)
		Error_Handler("Error on malloc usb_buff in Audio Save");
	
	// Me registro en el ring buffer
	ringBuf_registClient ( args->buff, args->usb_buff_size * sizeof(*usb_buff), args->usb_buff_size * sizeof(*usb_buff), audio_save_msg, RING_BUFFER_READY, &buff_client);

	// Create New Wave File
	if(newWavFile(args->file_name,&WaveFormat,&WavFile, wave_config) != FR_OK)
		Error_Handler("Error on newWavFile in Audio Save");

	/* START TASK */
	for (;;)
	{
		event = osMessageGet(audio_save_msg,osWaitForever);
		if(event.status == osEventMessage)
		switch(event.value.v)
		{
			case RING_BUFFER_READY:
			{
				// Copio los datos al buffer del USB
				ringBuf_read_const ( args->buff, buff_client, (uint8_t*) usb_buff);
				
				// Grabo lo último que me quedo en el buffer
				f_write(&WavFile, usb_buff, args->usb_buff_size * sizeof(*usb_buff), &byteswritten);
				file_size += byteswritten;
				
				break;
			}
			case FINISH:
			{
				// Copio los datos que quedan del buffer
				ringBuf_read_all ( args->buff, buff_client, (uint8_t*) usb_buff, &read_size);
				
				// Grabo lo último que me quedo en el buffer
				f_write(&WavFile, usb_buff, read_size, &byteswritten);
				file_size += byteswritten;
				
				// Close file
				closeWavFile(&WavFile, &WaveFormat, file_size);
				
				// Kill Thread
				osMessagePut(audio_save_msg,KILL_THREAD,0);
				
				break;
			}
			case KILL_THREAD:
			{
				// Unregister from ring buffer
				ringBuf_unregistClient ( args->buff, buff_client );
				
				// Free memroy
				free(usb_buff);
				
				// Kill message queue
				osMessageDelete(&audio_save_msg);
				
				//Send message telling i'll terminate
				osMessagePut(args->src_msg, FINISH_SAVING, osWaitForever);
				
				// Elimino la tarea
				osThreadTerminate(osThreadGetId());

				break;
			}
			default:
				break;
		}
	}
}
void AudioRead (void const * pvParameters) {

	// Function variables
	uint16_t *audio_frame;
	Audio_Read_args *args;
	FIL WaveFile;
	UINT bytesread;
	ringBufStatus ring_status;
	
	// Get arguments
	args = (Audio_Read_args*) pvParameters;
	
	// Allocate memory for audio frame
	if( (audio_frame = malloc( args->capt_conf.frame_size * sizeof(*audio_frame) ) ) == NULL )
		Error_Handler("Error on malloc audio_frame in AudioCapture");
						
	// Create buffer for audio
	if( ringBuf_init(args->buff, args->capt_conf.frame_size * args->capt_conf.ring_buff_size * sizeof(*audio_frame), false) != BUFF_OK )
		Error_Handler("Error on ring buffer init in AudioCapture");
	
	args->init_complete = true;
	
	/* START TASK */
	for (;;)
	{
		// Open audio file
		if(f_open(&WaveFile,args->file_name,FA_READ) != FR_OK)
			Error_Handler("Unable to open file");
		
		// Go where audio starts
		if(f_lseek(&WaveFile,44) != FR_OK)						
			Error_Handler("Unable to seek file");
		
		// Read audio file until end
		while(!f_eof(&WaveFile))
		{
			// Leo un Frame del archivo
			if(f_read (&WaveFile, audio_frame, args->capt_conf.frame_size * sizeof(*audio_frame), &bytesread) != FR_OK)
				Error_Handler("Error al leer el archivo");
			
			do
			{
				// Copy frame to buffer
				ring_status = ringBuf_write ( args->buff, (uint8_t*) audio_frame, bytesread );
			}while (ring_status != BUFF_OK);
		}
		
		// Cierro el archivo
		f_close(&WaveFile);
		
		// Envío mensaje inidicando que termine
		osMessagePut(args->src_msg, FINISH_READING, osWaitForever);
		
		// Espero a que todos los clinetes terminen de leer el buffer
		while( !is_ringBuf_empty ( args->buff ) )
			osDelay(10);

		// Elimino el buffer
		ringBuf_deinit(args->buff);
		
		// Release memory
		free(audio_frame);
		
		// Elimino la tarea
		osThreadTerminate(osThreadGetId());
	}
}
void Keyboard (void const * pvParameters) {
	
	osEvent event;
	static uint8_t state=0;
	uint8_t var;
	
	for(;;)
	{
		event = osMessageGet(key_msg,osWaitForever);
		if(event.status == osEventMessage) {
			
			osDelay(300);													// Waint 300msec for establishment time
			var = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0);
			
			// Pregunto para eliminiar falsos
			if(var != state)
			{
				state = var;
				if(var)
					osMessagePut(appli_event,BUTTON_PRESS,osWaitForever);
				else
					osMessagePut(appli_event,BUTTON_RELEASE,osWaitForever);
			}

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
	osMessageQDef(appli_event,5,uint16_t);
	appli_event = osMessageCreate(osMessageQ(appli_event),NULL);

	/* Create User Button MessageQ */
	osMessageQDef(key_msg,5,uint8_t);
	key_msg = osMessageCreate(osMessageQ(key_msg),NULL);
	
	/* Create Keyboard Task*/
	osThreadDef(Keyboard, Keyboard, osPriorityRealtime, 1, configMINIMAL_STACK_SIZE);
	osThreadCreate (osThread(Keyboard), NULL);
	
	/* Create Main Task*/
	osThreadDef(Main, Main_Thread, osPriorityNormal, 1, configMINIMAL_STACK_SIZE * 7);
	osThreadCreate (osThread(Main), NULL);
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
	
	// Read Debug configuration
	config->debug_conf.debug					= ini_getbool	("Debug", "Debug",	false,	filename);
	if(config->debug_conf.debug)
	{
		config->debug_conf.save_proc_vars	= ini_getbool	("Debug", "save_proc_vars",	false,	filename);
		config->debug_conf.save_clb_vars	= ini_getbool	("Debug", "save_clb_vars",	false,	filename);
		config->debug_conf.save_dist			= ini_getbool	("Debug", "save_dist",	false,	filename);
		config->debug_conf.usb_buff_size	= (uint32_t)	ini_getl("Debug", "usb_buff_size", 128, filename);
	}
	else
	{
		config->debug_conf.save_proc_vars	= false;
		config->debug_conf.save_clb_vars	= false;
		config->debug_conf.save_dist			= false;
		config->debug_conf.usb_buff_size	= 0;
	}
	
	// Read Auido configuration
	config->capt_conf.frame_size			= (uint16_t)	ini_getl("AudioConf", "FRAME_SIZE",			AUDIO_FRAME_SIZE,				filename);
	config->capt_conf.ring_buff_size	= (uint16_t)	ini_getl("AudioConf", "RING_BUFF_SIZE",	AUDIO_RING_BUFF_SIZE,		filename);
	config->capt_conf.freq						= (uint16_t)	ini_getl("AudioConf", "FREQ",						AUDIO_IN_FREQ,					filename);
	config->capt_conf.bw_high					= (uint16_t) 	ini_getl("AudioConf", "BW_HIGH", 				AUDIO_IN_BW_HIGH,				filename);
	config->capt_conf.bw_low					= (uint16_t) 	ini_getl("AudioConf", "BW_LOW",					AUDIO_IN_BW_LOW,				filename);
	config->capt_conf.bit_resolution	= (uint8_t)		ini_getl("AudioConf", "BIT_RESOLUTION", AUDIO_IN_BIT_RESOLUTION,filename);
	config->capt_conf.channel_nbr			= (uint8_t)		ini_getl("AudioConf", "CHANNEL_NBR", 		AUDIO_IN_CHANNEL_NBR,		filename);
	config->capt_conf.volume					= (uint8_t)		ini_getl("AudioConf", "VOLUME",					AUDIO_IN_VOLUME,				filename);
	config->capt_conf.decimator				= (uint8_t)		ini_getl("AudioConf", "DECIMATOR", 			AUDIO_IN_DECIMATOR,			filename);
	

	// Read Speech Processing configuration
	config->proc_conf.numtaps				= (uint16_t)		ini_getl		("SPConf", "NUMTAPS",					NUMTAPS,					filename);
	config->proc_conf.alpha					= (float32_t)		ini_getf		("SPConf", "ALPHA",						ALPHA,						filename);
	config->proc_conf.frame_len			= (uint16_t)		ini_getl		("SPConf", "FRAME_LEN", 			FRAME_LEN,				filename);
	config->proc_conf.frame_overlap	= (uint16_t)		ini_getl		("SPConf", "FRAME_OVERLAP", 	FRAME_LEN,				filename);
	config->proc_conf.fft_len				= (uint16_t)		ini_getl		("SPConf", "FFT_LEN", 				FFT_LEN,					filename);
	config->proc_conf.mel_banks			= (uint16_t)		ini_getl		("SPConf", "MEL_BANKS",				MEL_BANKS,				filename);
	config->proc_conf.dct_len				= (uint16_t)		ini_getl		("SPConf", "DCT_LEN", 				DCT_LEN,					filename);
	config->proc_conf.lifter_length	= (uint16_t)		ini_getl		("SPConf", "LIFTER_LENGTH",		LIFTER_LENGTH,		filename);
	config->proc_conf.vad						= 							ini_getbool	("SPConf", "VAD",							VAD_ENABLE,				filename);
	
// Read Calibration configuration
	config->calib_conf.calib_time		= (uint16_t)	ini_getl("CalConf", "CALIB_TIME", 			CALIB_TIME, 			filename);
	config->calib_conf.thd_scl_eng	= (float32_t)	ini_getf("CalConf", "THD_Scale_ENERGY", THD_Scl_ENERGY,		filename);
	config->calib_conf.thd_min_fmax	= (uint32_t)	ini_getf("CalConf", "THD_min_FMAX",			THD_min_FMAX,			filename);
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
	(*Pat) = (Patterns *) malloc(sizeof(Patterns)*(*pat_num));
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
uint8_t loadPattern (Patterns *pat, uint32_t vector_length) {
	
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
			if( ( ptr = (struct VC *) malloc(file_size) ) != NULL )
				// Leo el archivo con los patrones
				if(f_read (&File, ptr, file_size, &bytesread) == FR_OK)
				{
					output = 1;
					arm_mat_init_f32 (&pat->pattern_mtx, VC_length / vector_length , vector_length, (float32_t *) ptr);
					
					//TODO: se reemplazo appconf.proc_conf.lifter_length por vector_length
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
//uint32_t tick_start ,elapsed_time;
//if(f_open(&log_file,"log.txt",FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();		//TODO de prueba						
//tick_start = osKernelSysTick();
//elapsed_time = osKernelSysTick() - tick_start;
//f_printf(&log_file, "elapsed_time: %d\n", elapsed_time  );
//f_close(&log_file);

//FIL nosirve;
//f_open(&nosirve,"noSirve.txt", FA_CREATE_ALWAYS | FA_WRITE);
//f_printf(&nosirve, "Esto es una mierda");
//f_close(&nosirve);	//TODO de prueba

//	osMessagePut(calibration_msg, END_CAPTURE, osWaitForever);		// BORRAR ESTA LINEA DE ABAJO


//						// Create Audio Save Task for copy
//						audio_save.usb_buff_size = args->debug_conf->usb_buff_size;
//						audio_save.buff	= &audio_read_buff;
//						audio_save.capt_conf = *args->capt_conf;
//						audio_save.file_name = "PTRN_00/copy.wav";
//						for(; f_stat(audio_save.file_name ,NULL)!= FR_NO_FILE; updateFilename(audio_save.file_name));
//						osThreadCreate (osThread(AudioSave), &audio_save);

