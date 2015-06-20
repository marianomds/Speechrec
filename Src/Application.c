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
osMessageQId audio_capture_msg;
osMessageQId audio_process_msg;
osMessageQId audio_save_msg;
osMessageQId calibration_msg;
osMessageQId recognition_msg;
osMessageQId leds_msg;

//For leds
App_states app_state = APP_STOP;
Capture_states cap_state = NOT_CAPTURING;
Proc_states proc_state = NOTHING;

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

//---------------------------------

AppConfig appconf;

//--------------------------------
// 					Task Defines
//--------------------------------

osThreadDef(Main, Main_Thread, osPriorityNormal, 1, 32*25);

osThreadDef(AudioCapture, AudioCapture, osPriorityHigh,				1, 32*12);
osThreadDef(AudioSave, 		AudioSave, 		osPriorityAboveNormal,1, 32*30);
osThreadDef(AudioProcess,	AudioProcess, osPriorityNormal,			1, 32*50);
osThreadDef(AudioCalib, 	AudioCalib, 	osPriorityNormal,			1, 32*20);
osThreadDef(AudioRead, 		AudioRead, 		osPriorityBelowNormal,1, 32*20);

osThreadDef(FeatureExtraction,	featureExtraction,	osPriorityAboveNormal,	1, 32*120);
osThreadDef(Calibration, 				calibration, 				osPriorityAboveNormal,	1, 32*80);
osThreadDef(Recognition,				Recognition,				osPriorityAboveNormal, 	1, 32*40);

osThreadDef(Keyboard, Keyboard, osPriorityRealtime, 1, configMINIMAL_STACK_SIZE);
osThreadDef(Leds, 		Leds, 		osPriorityHigh, 1, configMINIMAL_STACK_SIZE);

//---------------------------------------
//						APPLICATIONS TASKS
//---------------------------------------

void Main_Thread (void const *pvParameters)
{
	// Message variables
	osEvent event;
	osMessageQId msgID = NULL;
	
	// Task variables
	ringBuf audio_ring_buffer = {NULL};
	
	// Task arguments
	Audio_Capture_args audio_cap_args = {NULL};
	Audio_Calibration_args calibration_args = {NULL};
	Audio_Process_args pat_stor_args = {NULL};
	
	for(;;) {
		
		event = osMessageGet(appli_event, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v) {
				
				case APPLICATION_READY:
				{
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
					osThreadCreate (osThread(AudioCapture), &audio_cap_args);
				
					/* If VAD was configured*/
					if(appconf.proc_conf.vad)
					{
						app_state = APP_CALIBRATION;
						
						/* Create AudioCalib Task */
						calibration_args.msg_q 			  = &msgID;
						calibration_args.proc_conf    = &appconf.proc_conf;
						calibration_args.calib_conf   = &appconf.calib_conf;
						calibration_args.debug_conf   = &appconf.debug_conf;
						calibration_args.capt_conf    = &appconf.capt_conf;
						calibration_args.buff				  = &audio_ring_buffer;
						calibration_args.src_msg_id   = appli_event;
						calibration_args.src_msg_val	= FINISH_CALIBRATION;
						osThreadCreate (osThread(AudioCalib), &calibration_args);
					}
					else
					{
						osMessagePut(appli_event,FINISH_CALIBRATION,osWaitForever);
					}
						
					break;
				}
					
				case APPLICATION_DISCONNECT:
				{
					app_state = APP_STOP;
					
					// Send KILL message to running task
					if (msgID != NULL)
						osMessagePut(msgID,KILL_THREAD,0);

					// Send KILL message to Auido Capture Task
					osMessagePut (audio_capture_msg, KILL_CAPTURE, 0);
										
					// Desmonto el FileSystem del USB
					f_mount(NULL, (TCHAR const*)"", 0);
					
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
				case FINISH_CALIBRATION:
				{
					// Send kill to running task if necesary
					if (msgID != NULL)
						osMessagePut(msgID,KILL_THREAD,osWaitForever);
					
					app_state = appconf.maintask;
					
					/* Create Audio_Process Task */
					pat_stor_args.msg_q = &msgID;
					pat_stor_args.buff = &audio_ring_buffer;
					pat_stor_args.debug_conf = &appconf.debug_conf;
					pat_stor_args.capt_conf = &appconf.capt_conf;
					pat_stor_args.proc_conf = &appconf.proc_conf;
					pat_stor_args.recognize = appconf.maintask == APP_RECOGNITION;
					
					/* Create Tasks*/
					osThreadCreate (osThread(AudioProcess), &pat_stor_args);

				break;
				}
				default:
					break;
			}
		}
	}
}

void AudioProcess (void const *pvParameters)
{
	// Message variables
	osEvent event;
	Audio_Process_args *args;
	Audio_Save_args audio_save = {NULL};
	Audio_Read_args audio_read = {NULL};
	Feature_Extraction_args feature_extract = {NULL};
	Recognition_args reco_args = {NULL};

	// Task Variables
	ringBuf audio_read_buff = {NULL}, features_buff = {NULL};
	uint8_t features_buff_client_num;
	char file_path[] = {"REC_00"};
	char file_name[ (strlen(file_path)+1)*2 + strlen(AUDIO_FILE_EXTENSION) ];
	bool processing = false;					// If Audio Processing Task is still processing then true
	bool recording = false;
	bool recognizing = false;
	
	// Para salvar los features
	float32_t *features;
	FIL features_file;
	UINT bytes_written;
	
	// Get arguments
	args = (Audio_Process_args *) pvParameters;
	
	// Create Audio Process Sotring Message Queue
	osMessageQDef(audio_process_msg,10,uint32_t);
	audio_process_msg = osMessageCreate(osMessageQ(audio_process_msg),NULL);
	*(args->msg_q) = audio_process_msg;
	
	// Alloco memoria para los features
	features = malloc( (1+args->proc_conf->lifter_length) * 3 * sizeof(*features));
	
	// Init Audio Processing args
	feature_extract.features_buff = &features_buff;
	feature_extract.proc_conf = args->proc_conf;
	feature_extract.save_to_files = args->debug_conf->save_proc_vars;
	feature_extract.src_msg_id =  audio_process_msg;
	feature_extract.src_msg_val = FINISH_PROCESSING;
	feature_extract.path = file_path;
	
	
//	// DE PRUEBA DESPUES BORRAR
//	strcpy(file_name, file_path);
//	strcat(file_name, "/");
//	strcat(file_name, file_path);
//	strcat(file_name, AUDIO_FILE_EXTENSION);

//// Open file for saving features
//	f_chdir(file_path);
//	f_open(&features_file, "features.bin", FA_CREATE_ALWAYS | FA_WRITE );
//	f_chdir("..");

//	osMessagePut(audio_process_msg,FINISH_SAVING,osWaitForever);
	
	/* START TASK */
	for (;;)
	{
		// Espero por mensaje
		event = osMessageGet(audio_process_msg, osWaitForever);
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

					// Set file_name "REC_xx/REC_xx.WAV"
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
						audio_save.src_msg = audio_process_msg;
						osThreadCreate (osThread(AudioSave), &audio_save);
					}
					
					if (!args->debug_conf->save_proc_vars)
					// Process Audio in Real Time
					{
						feature_extract.audio_buff = args->buff;
						osThreadCreate (osThread(FeatureExtraction), &feature_extract);

						// Espero a que este todo inicializado
						while(!is_ringBuf_init(&features_buff))
							osDelay(1);
						
						// Me registro como cliente en el buffer que crea audio_proc
						ringBuf_registClient ( &features_buff,	(1 + args->proc_conf->lifter_length) * 3 * sizeof(float32_t),
																		(1 + args->proc_conf->lifter_length) * 3 * sizeof(float32_t), audio_process_msg,
																		FRAME_READY,	&features_buff_client_num);
																		
						processing = true;
						proc_state = PROCESSING;
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
					
					if (!args->debug_conf->save_proc_vars)
						// Send Message to Auido Procesing Task
						osMessagePut(feature_extract.proc_msg_id, PROC_FINISH, osWaitForever);
					break;
				}
				case FINISH_SAVING:
				{
					// Process ofline
					if ( args->debug_conf->save_proc_vars )
					{
						// Create Read Audio Task
						audio_read.buff = &audio_read_buff;
						audio_read.capt_conf = *args->capt_conf;
						audio_read.file_name = file_name;
						audio_read.src_msg = audio_process_msg;
						osThreadCreate (osThread(AudioRead), &audio_read);
						
						// Espero a que este todo inicializado
						while(!is_ringBuf_init(&audio_read_buff))
							osDelay(1);
						
						// Start Feature Extraction process
						feature_extract.audio_buff = &audio_read_buff;
						osThreadCreate (osThread(FeatureExtraction), &feature_extract);
						
						// Espero a que este todo inicializado
						while(!is_ringBuf_init(&features_buff))
							osDelay(1);
						
						// Me registro como cliente en el buffer que crea audio_proc
						ringBuf_registClient ( &features_buff,	(1 + args->proc_conf->lifter_length) * 3 * sizeof(float32_t),
																		(1 + args->proc_conf->lifter_length) * 3 * sizeof(float32_t), audio_process_msg,
																		FRAME_READY,	&features_buff_client_num);

						processing = true;
						proc_state = PROCESSING;
					}

					break;
				}
				case FINISH_READING:
				{
					// Finish processing ofline
					osMessagePut(feature_extract.proc_msg_id, PROC_FINISH, osWaitForever);
					break;
				}
				case FRAME_READY:
				{
					while (ringBuf_read_const ( &features_buff, features_buff_client_num, (uint8_t*) features ) == BUFF_OK)
						f_write(&features_file, features, (1+args->proc_conf->lifter_length) * 3 * sizeof(*features), &bytes_written);
					break;
				}
				case FINISH_PROCESSING:
				{
					//Leo lo que me queda y lo guardo y cierro el archivo
					ringBuf_read_all ( &features_buff, features_buff_client_num, (uint8_t*) features, &bytes_written );
					f_write(&features_file, features, bytes_written, &bytes_written);
					f_close(&features_file);
					
					// Me desregistro del buffer
					ringBuf_unregistClient(&features_buff, features_buff_client_num);
					
					processing = false;
					proc_state = NOTHING;
					
					if(args->recognize)
					{
						reco_args.patterns_path = appconf.patpath;
						reco_args.proc_conf = args->proc_conf;
						reco_args.utterance_path = file_path;
						reco_args.save_dist = args->debug_conf->save_dist;
						reco_args.src_msg_id = audio_process_msg;
						reco_args.src_msg_val = FINISH_RECOGNIZING;
						osThreadCreate(osThread(Recognition),&reco_args);
					}
					break;
				}
				case FINISH_RECOGNIZING:
				{
					break;
				}
				case KILL_THREAD:
				{
					// Stop Recording Tasks
					if (recording)
					{
						osMessagePut(audio_capture_msg, STOP_CAPTURE, osWaitForever);
						
						if (args->debug_conf->debug)
							osMessagePut(audio_save_msg, FINISH, osWaitForever);
						
						recording = false;
					}
					
					// Stop Processing Task
					if(processing)
					{
						f_close(&features_file);
						ringBuf_unregistClient(&features_buff, features_buff_client_num);
						osMessagePut(feature_extract.proc_msg_id, KILL_THREAD, osWaitForever);
						
						//if (args->debug_conf->save_proc_vars);
						//TODO: KILL READING
						
						processing = false;
					}
					
					if(recognizing)
					{
						//TODO Completar
					}
					
					// Free memory
					free(features);
					
					//Kill message queue
					osMessageDelete(&audio_process_msg);
					
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
void AudioCalib (void const *pvParameters)
{
	// Message variables
	osEvent event;
	Audio_Calibration_args *args;
	Calib_args calib_args = {NULL};
	Audio_Save_args audio_save = {NULL};
	Audio_Read_args audio_read = {NULL};
	
	// Task Variables
	ringBuf audio_read_buff = {NULL};
	char file_path[] = {"CLB_00"};
	char file_name[ (strlen(file_path)+1)*2 + strlen(AUDIO_FILE_EXTENSION) ];
	bool calibrating = false;
	
	// Get arguments
	args = (Audio_Calibration_args *) pvParameters;
	
	// Create AudioCalib Message Queue
	osMessageQDef(calibration_msg,10,uint32_t);
	calibration_msg = osMessageCreate(osMessageQ(calibration_msg),NULL);
	*(args->msg_q) = calibration_msg;
	
	// Set file_name "CLB_xx/CLB_xx.WAV"
	strcpy(file_name, file_path);
	strcat(file_name, "/");
	strcat(file_name, file_path);
	strcat(file_name, AUDIO_FILE_EXTENSION);
	
	// Init calibration arguments
	calib_args.proc_conf = &appconf.proc_conf;
	calib_args.calib_conf = &appconf.calib_conf;
	calib_args.audio_freq = args->capt_conf->freq;
	calib_args.save_to_files = appconf.debug_conf.save_proc_vars;
	calib_args.save_calib_vars = appconf.debug_conf.save_clb_vars;
	calib_args.src_msg_id = calibration_msg;
	calib_args.src_msg_val = FINISH;
	calib_args.path = file_path;
		
	// Si estoy en debug ==> salvo el audio
	if (args->debug_conf->debug)
	{
		// Check if filename exist, otherwise update
		for(; f_stat(file_path,NULL)!= FR_NO_FILE; updateFilename(file_path));
	
		// Create subdirectory for saving files here
		f_mkdir (file_path);

		// Create Audio Save Task
		audio_save.usb_buff_size = args->debug_conf->usb_buff_size;
		audio_save.buff	= args->buff;
		audio_save.capt_conf = *args->capt_conf;
		audio_save.file_name = file_name;
		audio_save.src_msg = calibration_msg;
		osThreadCreate (osThread(AudioSave), &audio_save);
	}
	
	// Si no es necesario salvar las variables calibro online
	if(!args->debug_conf->save_proc_vars)
	{
		calib_args.audio_buff = args->buff;
		osThreadCreate (osThread(Calibration), &calib_args);
		calibrating = true;
		proc_state = CALIBRATING;
	}
	
	// Start capturing audio
	osMessagePut(audio_capture_msg,START_CAPTURE,osWaitForever);
	
	// Espero el tiempo indicado por la calibración (indicado en segundos)
	osDelay(args->calib_conf->calib_time * 1000);

	// Stop capturing audio
	osMessagePut(audio_capture_msg,STOP_CAPTURE,osWaitForever);
	
	
	if (args->debug_conf->debug)
		// Finish recording
		osMessagePut(audio_save_msg,FINISH,osWaitForever);
	else
		// Si trabajo online ==> tengo que terminar la calibración
		osMessagePut(calib_args.calib_msg_id, CALIB_FINISH, osWaitForever);
	
	/* START TASK */
	for (;;)
	{
		// Espero por mensaje
		event = osMessageGet(calibration_msg, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch(event.value.v)
			{	
				case FINISH_SAVING:
				{
					// Si trabajo ofline ==> creo una tarea para leer el archivo y lo proceso
					if(args->debug_conf->save_proc_vars)
					{
						// Create Read Audio Task
						audio_read.buff = &audio_read_buff;
						audio_read.capt_conf = *args->capt_conf;
						audio_read.file_name = file_name;
						audio_read.src_msg = calibration_msg;
						osThreadCreate (osThread(AudioRead), &audio_read);
						
						// Espero a que este todo inicializado
						while(!is_ringBuf_init(&audio_read_buff))
							osDelay(1);
						
						// Create AudioCalib Task
						calib_args.audio_buff = &audio_read_buff;
						osThreadCreate (osThread(Calibration), &calib_args);
						calibrating = true;
						proc_state = CALIBRATING;
					}

					break;
				}
				case FINISH_READING:
				{
					// Termino la calibración
					osMessagePut(calib_args.calib_msg_id, CALIB_FINISH, osWaitForever);
					break;
				}
				case FINISH:
				{
					osMessagePut(calibration_msg,KILL_THREAD,osWaitForever);
					calibrating = false;
					proc_state = NOTHING;
					break;
				}
				case KILL_THREAD:
				{				
					// Stop Recording Tasks
					if (calibrating)
						osMessagePut(calib_args.calib_msg_id, CALIB_KILL, osWaitForever);
					
					//Kill message queue
					osMessageDelete(&calibration_msg);
					
					// Send message telling that is finish
					osMessagePut(args->src_msg_id, args->src_msg_val, osWaitForever);
					
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
void Recognition (void const *pvParameters)
{
	// Message variables
	Recognition_args *args = NULL;
	
	// Utterance variables
	float32_t *utterance = NULL;
	FIL utterance_file;
	UINT bytes_read;
	size_t utterance_size;
	arm_matrix_instance_f32 utterance_mtx;
	
	// Patterns variables
	Patterns *pat = NULL;
	uint32_t pat_num = 0,pat_reco = 0;
	
	// Output info variables
	FIL result;
	float32_t *dist = NULL;
	
	// Get arguments
	args = (Recognition_args *) pvParameters;
	
					
	/* START TASK */
	for(;;)
	{	
		proc_state = RECOGNIZING;
		
		// --------------- Load spoken word ---------------
		// Read Patterns config file
		f_chdir(args->patterns_path);
		readPaternsConfigFile (PAT_CONF_FILE, &pat, &pat_num);
		f_chdir("..");

				
		if ( pat_num != 0)
		{
			// Allocate memroy for distance and initialize
			dist = malloc(pat_num * sizeof(*dist));
			for(int i=0; i < pat_num ; i++)
				dist[i] = FLT_MAX;
			
			// Open utterance folder and stay here
			f_chdir(args->utterance_path);
			
			f_open(&utterance_file, "features.bin", FA_OPEN_EXISTING | FA_READ);		// Open File
			utterance_size = f_size (&utterance_file);														// Check data size
			utterance = malloc(utterance_size);																		// Allocate memory
			f_read (&utterance_file, utterance, utterance_size, &bytes_read);			// Read data
			f_close(&utterance_file);																							// Close file
			
			arm_mat_init_f32 (&utterance_mtx, utterance_size / (sizeof(*utterance) * 3 * (1+appconf.proc_conf.lifter_length)), 3 * (1+appconf.proc_conf.lifter_length), utterance);
			
			// Search for minimun distance between patterns and utterance
			for(int i=0; i < pat_num ; i++)
			{
				// Load pattern
				if( !loadPattern (&pat[i], (1+args->proc_conf->lifter_length) * 3, args->patterns_path ) )
					continue;

				// Get distance
				dist[i] = dtw_reduce (&utterance_mtx, &pat[i].pattern_mtx, args->save_dist);

				// Check if distance is shorter
				if( i == 0 ||dist [i] < dist[pat_reco])
					pat_reco = i;
				
				// Free memory
				free(pat[i].pattern_mtx.pData);
				pat[i].pattern_mtx.pData = NULL;
				pat[i].pattern_mtx.numCols = 0;
				pat[i].pattern_mtx.numRows = 0;
			}
			
			// Save distances
			if(appconf.debug_conf.save_dist)
			{
				f_open (&result, "Dist.bin", FA_WRITE | FA_OPEN_ALWAYS);				// Load result file
				f_write(&result, dist, pat_num*sizeof(*dist),&bytes_read);
				f_close(&result);
			}

			// Leave utterance folder
			f_chdir ("..");

			// Record in file wich pattern was spoken
			open_append (&result,"Spoken");																					// Load result file
			if (dist[pat_reco] != FLT_MAX)
				f_printf(&result, "%s %s\n", "Patron:", pat[pat_reco].pat_name);			// Record pattern name
			else
				f_printf(&result, "%s %s\n", "Patron:", "No reconocido");							// If no pattern was found
			f_close(&result);

		}
		/****************** FINISH RECOGNIZING *******************/
		proc_state = NOTHING;

		// Free memory
		free(utterance);				utterance = NULL;
		free(pat);							pat = NULL;
		free(dist);							dist = NULL;
		
		// Send message telling I'm finish
		osMessagePut(args->src_msg_id, args->src_msg_val, osWaitForever);
		
		// Kill thread
		osThreadTerminate (osThreadGetId());
	}
}

void AudioCapture (void const * pvParameters)
{

	// Function variables
	uint16_t *audio_frame;
	Audio_Capture_args *args;
	osEvent event;
	
	// Create Message
	osMessageQDef(audio_capture_msg, 10, uint32_t);
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
					cap_state = CAPTURING;
					break;
				}
				
				case PAUSE_CAPTURE:
				{					
					// Pause Audio Record
					audioPause();
					cap_state = PAUSE;
					break;
				}
							
				case RESUME_CAPTURE:
				{
					// Resume Audio Record
					audioResume();
					cap_state = CAPTURING;
					break;
				}
				
				case STOP_CAPTURE:
				{
					// Pause Audio Record
					audioStop();
					cap_state = NOT_CAPTURING;
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
					
					// Elimino la tarea
					osThreadTerminate(osThreadGetId());

					break;
				}
				default:
					break;
			}
	}
}
void AudioSave (void const * pvParameters)
{
	
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
void AudioRead (void const * pvParameters)
{

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
				osThreadYield();
			}while (ring_status != BUFF_OK);
		}
		
		// Cierro el archivo
		f_close(&WaveFile);
		
		// Envío mensaje inidicando que termine
		osMessagePut(args->src_msg, FINISH_READING, osWaitForever);
		
		// Espero a que todos los clinetes terminen de leer el buffer
		while( has_ringBuf_clients ( args->buff ) )
			osThreadYield();

		// Elimino el buffer
		ringBuf_deinit(args->buff);
		
		// Release memory
		free(audio_frame);
		
		// Elimino la tarea
		osThreadTerminate(osThreadGetId());
	}
}
void Keyboard (void const * pvParameters)
{
	
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


void Leds (void const * pvParameters)
{
	while (!osKernelRunning());
	
	union
	{
		struct
		{
			int green;
			int orange;
			int red;
			int blue;
		} led;
		int vec[4];
	} count_leds;
	
	uint32_t count = 0;
	
	for(;;)
	{
		switch(cap_state)
		{
			case CAPTURING:
				count_leds.led.red = 0;
				break;
			case NOT_CAPTURING:
				count_leds.led.red = -1;
				break;
			case PAUSE:
				count_leds.led.red = 1;
				break;
			default :
				count_leds.led.red = -1;
				break;
		}
		
		switch(app_state)
		{
			case APP_STOP:
				count_leds.led.orange = 1;
				break;
			default:
				count_leds.led.orange = -1;
				break;
		}	
			
		switch(app_state)
		{
			case APP_CALIBRATION:
				count_leds.led.green = 1;
				break;
			case APP_PATTERN_STORING:
				count_leds.led.green = 3;
				break;
			case APP_RECOGNITION:
				count_leds.led.green = 5;
				break;
			default:
				count_leds.led.green = -1;
				break;
		}
		
		switch(proc_state)
		{
			case PROCESSING:
				count_leds.led.blue = 0;
				break;
			case CALIBRATING:
				count_leds.led.blue = 0;
				break;
			case RECOGNIZING:
				count_leds.led.blue = 5;
				break;
			default:
				count_leds.led.blue = -1;
				break;
		}
		
		// Led toggle
		osDelay(100);
		for (int i=0; i<4; i++)
		{
			if (count_leds.vec[i] < 0)
				LED_Off( (Led_TypeDef) i);
			else if (count_leds.vec[i] == 0)
				LED_On( (Led_TypeDef) i);
			else if ( (count++ % count_leds.vec[i]) == 0 )
				LED_Toggle( (Led_TypeDef) i);
			continue;
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
void Configure_Application (void)
{
	
	/* Create Application State MessageQ */
	osMessageQDef(appli_event,5,uint32_t);
	appli_event = osMessageCreate(osMessageQ(appli_event),NULL);

	/* Create User Button MessageQ */
	osMessageQDef(key_msg,5,uint32_t);
	key_msg = osMessageCreate(osMessageQ(key_msg),NULL);
	
	/* Create Keyboard Task*/
	osThreadCreate (osThread(Keyboard), NULL);
	
	/* Create Leds Task*/
	osThreadCreate (osThread(Leds), NULL);
	
	/* Create Main Task*/
	osThreadCreate (osThread(Main), NULL);
}
/**
  * @brief  
  * @param  
  * @retval
  */
void setEnvVar (void)
{

	// Calcula las variables que va a utilizar el sistema y dejar de usar los defines
	
//	uint32_t buff_n;
//	uint32_t padding;
//		
//	// Calculo la longitud necesaria del buffer (haciendo zero padding de ser necesario)
//	buff_n = ceil( log(appconf.proc_conf.frame_net + appconf.proc_conf.frame_overlap * 2) / log(2) );
//	appconf.proc_conf.frame_len = pow(2,buff_n);
//	padding = appconf.proc_conf.frame_len - (appconf.proc_conf.frame_net + appconf.proc_conf.frame_overlap * 2);
		
	// Escalo el THD_min_FMAX a índices en el buffer
	appconf.calib_conf.thd_min_fmax = appconf.calib_conf.thd_min_fmax * appconf.proc_conf.fft_len / appconf.capt_conf.freq;	
}
/**
  * @brief  Lee el archivo de configuración del sistema
	*
	* @param  filename  [in] 	Filename to read
	* @param  appconf [out]	Puntero a la estructura de configuración
  * @retval OK-->"1"  Error-->"0"
  */
uint8_t readConfigFile (const char *filename, AppConfig *config)
{
	
	// Read System configuration
	config->maintask	= (App_states) ini_getl	("System", "MAIN_TASK", APP_PATTERN_STORING,	filename);
	
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
	config->proc_conf.theta					= (uint8_t)			ini_getl		("SPConf", "THETA",						THETA,						filename);
	config->proc_conf.vad						= 							ini_getbool	("SPConf", "VAD",							VAD_ENABLE,				filename);
	
// Read AudioCalib configuration
	config->calib_conf.calib_time		= (uint16_t)	ini_getl("CalConf", "CALIB_TIME", 			CALIB_TIME, 			filename);
	config->calib_conf.thd_scl_eng	= (float32_t)	ini_getf("CalConf", "THD_Scale_ENERGY", THD_Scl_ENERGY,		filename);
	config->calib_conf.thd_min_fmax	= (uint32_t)	ini_getf("CalConf", "THD_min_FMAX",			THD_min_FMAX,			filename);
	config->calib_conf.thd_scl_sf		= (float32_t)	ini_getf("CalConf", "THD_Scale_SF",			THD_Scl_SF,				filename);
	
	// Read Patterns configuration
	ini_gets("PatConf", "PAT_DIR", 				PAT_DIR, 				config->patpath, 			sizeof(config->patpath), 			filename);

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
uint8_t readPaternsConfigFile (const char *filename, Patterns **Pat, uint32_t *pat_num)
{
	
	char section[30];
	uint32_t i;
	
	// Count how many Patterns there are
	for ((*pat_num) = 0; ini_getsection((*pat_num), section, sizeof(section), filename) > 0; (*pat_num)++);
	if(*pat_num == 0)
		return 0;
	
	// Allocate memory for the Patterns
	(*Pat) = (Patterns *) malloc(sizeof(Patterns)*(*pat_num));
	if(*Pat == NULL)
		return 0;

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
uint8_t loadPattern (Patterns *pat, uint32_t vector_length, char *pat_path)
{
	
	FIL File;
	UINT bytesread;
	uint8_t output = 0;
	float32_t *ptr;
	uint32_t file_size;
	char *pat_file_name;

	pat_file_name = malloc( strlen("0:/") + strlen(pat_path) + 1 + strlen(pat->pat_name) + 1 + strlen("features.bin") + 1 );
	sprintf(pat_file_name,"0:/%s/%s/features.bin", pat_path, pat->pat_name);

	//Abro el archivo
	if(f_open(&File, pat_file_name, FA_READ) == FR_OK)		// Abro el archivo
	{
		// Me fijo el tamaño del archivo
		file_size = f_size (&File);
		
		// Aloco la memoria necesaria para almacenar en memoria todos los Vectores de Características
		if( ( ptr = malloc(file_size) ) != NULL )
			
			// Leo el archivo con los patrones
			if(f_read (&File, ptr, file_size, &bytesread) == FR_OK)
			{
				output = 1;
				arm_mat_init_f32 (&pat->pattern_mtx, file_size / vector_length / sizeof(float32_t) , vector_length, (float32_t *) ptr);
			}
		
		// Cierro el archivo
		f_close(&File);
	}

	// Free memory
	free (pat_file_name);
	
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
void User_Button_EXTI (void)
{
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






//void Recognition (void const *pvParameters)
//{
//	// Message variables
////	osEvent event;
//	Recognition_args *args = NULL;
////	bool recognising = true;
//	
//	// Utterance variables
////	float32_t *utterance = NULL;
//	FIL utterance_file, pat_file;
//	UINT bytes_read;
////	size_t utterance_size;
////	arm_matrix_instance_f32 utterance_mtx;
//	
//	// Patterns variables
//	Patterns *pat = NULL;
//	uint32_t pat_num = 0,pat_reco = 0;
//	char *pat_file_name = NULL;
//	
//	// Output info variables
//	FIL result;
//	float32_t *dist = NULL;
//	
//	// Get arguments
//	args = (Recognition_args *) pvParameters;
//	
//					
//	/* START TASK */
//	for(;;)
//	{	
//		proc_state = RECOGNIZING;
//		
//		// --------------- Load spoken word ---------------
//		// Read Patterns config file
//		f_chdir(args->patterns_path);
//		readPaternsConfigFile (PAT_CONF_FILE, &pat, &pat_num);
//		f_chdir("..");

//				
//		if ( pat_num != 0)
//		{
//			// Allocate memroy for distance and initialize
//			dist = malloc(pat_num * sizeof(*dist));
////			for(int i=0; i < pat_num ; i++)
////				dist[i] = FLT_MAX;
//			
//			// Open utterance folder and stay here
//			f_chdir(args->utterance_path);
//			
//			f_open(&utterance_file, "features.bin", FA_OPEN_EXISTING | FA_READ);		// Open File
////			utterance_size = f_size (&utterance_file);														// Check data size
////			utterance = malloc(utterance_size);																		// Allocate memory
////			f_read (&utterance_file, utterance, utterance_size, &bytes_read);			// Read data
////			f_close(&utterance_file);																							// Close file
////			f_chdir ("..");																												// Leave from utterance file
////			
////			arm_mat_init_f32 (&utterance_mtx, (utterance_size / sizeof(*utterance)) / appconf.proc_conf.lifter_length , appconf.proc_conf.lifter_length, utterance);
//			
//			// Search for minimun distance between patterns and utterance
//			for(int i=0; i < pat_num ; i++)
//			{
//	//			// Load pattern
//	//			if( !loadPattern (&pat[i], (1+args->proc_conf->lifter_length) * 3) )
//	//				continue;

//				// Abro el archivo del patron
//					// Set complete pattern file_name
//				pat_file_name = malloc( strlen("0:/") + strlen(args->patterns_path) + 1 + strlen(pat[i].pat_name) + 1 + strlen("features.bin") + 1 );
//				sprintf(pat_file_name,"0:/%s/%s/features.bin",args->patterns_path,pat[i].pat_name);
//				if(f_open(&pat_file, pat_file_name, FA_READ) != FR_OK)		// Open pattern file_name
//					continue;
//				free (pat_file_name);			pat_file_name = NULL; 					// Free memory
//				
//				
//				// Get distance
//	//			dist[i] = dtw_reduce (&utterance_mtx, &pat[i].pattern_mtx, appconf.debug_conf.save_dist);		
//						// Abro un archivo para salvar la matriz de distancia
////				dist[i] = dtw_files (&utterance_file, &pat_file, 3 * (args->1+proc_conf->lifter_length), args->save_dist);
//				dist[i] = dtw_files_reduce (&utterance_file, &pat_file, 3 * (args->1+proc_conf->lifter_length), args->save_dist);
//				
//				// Close Pattern file
//				f_close(&pat_file);

//				// Check if distance is shorter
//				if( i == 0 ||dist [i] < dist[pat_reco])
//					pat_reco = i;
//				
//				// Free memory
//	//			free(pat[i].pattern_mtx.pData);
//	//			pat[i].pattern_mtx.pData = NULL;
//	//			pat[i].pattern_mtx.numCols = 0;
//	//			pat[i].pattern_mtx.numRows = 0;
//			}
//			
//			// Close utterance file
//			f_close(&utterance_file);
//											
//			// Save distances
//			if(appconf.debug_conf.save_dist)
//			{
//				f_open (&result, "Dist.bin", FA_WRITE | FA_OPEN_ALWAYS);				// Load result file
//				f_write(&result, dist, pat_num*sizeof(*dist),&bytes_read);
//				f_close(&result);
//			}
//			free(dist);							dist = NULL;

//			// Leave utterance folder
//			f_chdir ("..");

//			// Record in file wich pattern was spoken
//			open_append (&result,"Spoken");																					// Load result file
//			if (dist[pat_reco] != FLT_MAX)
//				f_printf(&result, "%s %s\n", "Patron:", pat[pat_reco].pat_name);			// Record pattern name
//			else
//				f_printf(&result, "%s %s\n", "Patron:", "No reconocido");							// If no pattern was found
//			f_close(&result);

//		}
//		/****************** FINISH RECOGNIZING *******************/
//		//		recognising = false;
//		proc_state = NOTHING;

//		// Free memory
////		free(utterance);				utterance = NULL;
//		free(pat);							pat = NULL;
////		free(dist);							dist = NULL;
//		
//		// Send message telling I'm finish
//		osMessagePut(args->src_msg_id, args->src_msg_val, osWaitForever);
//		
//		// Kill thread
//		osThreadTerminate (osThreadGetId());
//	}
//}


