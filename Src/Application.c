/**
  ******************************************************************************
  * @file    Aplication.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    05-Agosto-2014
  * @brief   Application Tasks for ASR_DTW
  ******************************************************************************
  * @version V2.0.0
  * @author  Mariano Marufo da Silva
	* @email 	 mmarufodasilva@est.frba.utn.edu.ar
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
	Audio_Process_args audio_proc_args = {NULL};
	
	/* Initialize LCD */
	LCD_Init();
	
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
					if(appconf.vad_conf.vad)
					{
						app_state = APP_CALIBRATION;
						
						/* Create AudioCalib Task */
						calibration_args.msg_q 			  = &msgID;
						calibration_args.proc_conf    = &appconf.proc_conf;
						calibration_args.vad_conf    	= &appconf.vad_conf;
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
					audio_proc_args.msg_q = &msgID;
					audio_proc_args.buff = &audio_ring_buffer;
					audio_proc_args.debug_conf = &appconf.debug_conf;
					audio_proc_args.capt_conf = &appconf.capt_conf;
					audio_proc_args.proc_conf = &appconf.proc_conf;
					audio_proc_args.vad_conf =  &appconf.vad_conf;
					audio_proc_args.recognize = appconf.maintask == APP_RECOGNITION;
					osThreadCreate (osThread(AudioProcess), &audio_proc_args);

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
	feature_extract.vad_conf = args->vad_conf;
	feature_extract.save_to_files = args->debug_conf->save_proc_vars;
	feature_extract.src_msg_id =  audio_process_msg;
	feature_extract.src_msg_val = FINISH_PROCESSING;
	feature_extract.path = file_path;
					
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
						reco_args.proc_conf = args->proc_conf;
						reco_args.utterance_path = file_path;
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
	
	// Init calibration arguments
	calib_args.proc_conf = &appconf.proc_conf;
	calib_args.vad_conf  = &appconf.vad_conf;
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

		// Set file_name "CLB_xx/CLB_xx.WAV"
		strcpy(file_name, file_path);
		strcat(file_name, "/");
		strcat(file_name, file_path);
		strcat(file_name, AUDIO_FILE_EXTENSION);
			
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
					// Si trabajo offline ==> creo una tarea para leer el archivo y lo proceso
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
					else
						// Si trabajo online ==> Aviso que tiene que terminar
						osMessagePut(calib_args.calib_msg_id, CALIB_FINISH, osWaitForever);

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
	uint16_t T;
	
	// Aux variables
	uint16_t i;
	char str_aux[20];
	
	//HMM variables
	float32_t loglik[11];
	float32_t loglikMAX;
	uint8_t loglikMAXind = 11;
	uint8_t loglik2MAXind = 11;
	float32_t loglik2MAX;
	float32_t var;
	float32_t loglik3MAX;
	float32_t dif1;
	float32_t dif2;
	char nivel[6];
	
	// Output info variables
	FIL result;
	
	// Get arguments
	args = (Recognition_args *) pvParameters;
	
					
	/* START TASK */
	for(;;)
	{	
		proc_state = RECOGNIZING;
		
			// Open utterance folder and stay here
			f_chdir(args->utterance_path);
			
			f_open(&utterance_file, "features.bin", FA_OPEN_EXISTING | FA_READ);		// Open File
			utterance_size = f_size (&utterance_file);														// Check data size
			utterance = malloc(utterance_size);																		// Allocate memory
			f_read (&utterance_file, utterance, utterance_size, &bytes_read);			// Read data
			f_close(&utterance_file);																							// Close file
			
			// Calculo el largo de la secuencia
			T = bytes_read/NCOEFS/sizeof(*utterance);
			
			// Calculo los log-likelihoods de la secuencia para los 11 HMM
			for (i = 0; i<11; i++)
			{
				loglik[i] = Tesis_loglik(utterance, T, *(transmat1 + i), *(transmat2 + i), *(*(mixmat + i)), *(*(*(mu + i))), *(*(logDenom + i)), *(*(*(invSigma + i))));
			}
			
			
			// Leave utterance folder
			f_chdir ("..");

			// Busco primero y segundo máximo de los Log_likelihoods
			loglikMAX = loglik[0];
			loglikMAXind = 0;
			loglik2MAX = loglik[1];
			loglik2MAXind = 1;
			if (loglik2MAX > loglikMAX)
			{
				var = loglik2MAX;
				loglik2MAX = loglikMAX;
				loglikMAX = var;
				loglikMAXind = 1;
				loglik2MAXind = 0;
			}
			for (i = 2; i < 11; i++)
			{
				if (loglik[i] > loglikMAX)
				{
					loglik2MAX = loglikMAX;
					loglikMAX = loglik[i];
					loglik2MAXind = loglikMAXind;
					loglikMAXind = i;
				}
				else if(loglik[i] > loglik2MAX)
				{
					loglik2MAX = loglik[i];
					loglik2MAXind = i;
				}
			}
			
			// Busco tercer máximo de los Log_likelihoods
			loglik3MAX = -INFINITY;
			for (i = 0; i < 11; i++)
			{
				if ((i!=loglikMAXind)&&(i!=loglik2MAXind))
				{
					if (loglik[i] > loglik3MAX)
					{
						loglik3MAX = loglik[i];
					}
				}
			}
			
			// Evaluación del nivel de reconocimiento
			dif1 = loglikMAX - loglik2MAX; // difenrencia entre el primero y el segundo
			dif2 = loglik2MAX - loglik3MAX; // difenrencia entre el segundo y el tercero
			if ((dif1 > 200) && (dif2 <= 80))
			{
				strcpy(nivel, "Alto");
			}
			else if ((dif1 > 200) && (dif2 > 80))
			{
				strcpy(nivel, "Medio");
			}
			else
			{
				strcpy(nivel, "Bajo");
			}
			
			// Record in file wich pattern was spoken
			open_append (&result,"Spoken");																					// Load result file
			if (loglikMAXind == 11)
			{
				f_printf(&result, "Error - Likelihood para los 11 modelos: 0\n\n");
			}
			else
			{
				f_printf(&result, "Número reconocido: %d\nLog-Likelihood: %d\nSegundo puesto: %d\nLog-Likelihood (2° puesto): %d\nDiferencia entre primero y segundo: %d\nDiferencia entre segundo y tercero: %d\nNivel de reconocimiento: %s\n\n", loglikMAXind, (int)loglikMAX, loglik2MAXind, (int)loglik2MAX, (int)rintf(loglikMAX - loglik2MAX), (int)rintf(loglik2MAX - loglik3MAX), nivel);
				sprintf(str_aux, "%d", loglikMAXind);
				LCD_sendstring("  ", LCD_L2_B);
				LCD_sendstring(str_aux, LCD_L2_B);
				sprintf(str_aux, "%d", loglik2MAXind);
				LCD_sendstring("  ", LCD_L3_B);
				LCD_sendstring(str_aux, LCD_L3_B);
				sprintf(str_aux, "%s", nivel);
				LCD_sendstring("     ", LCD_L4_B);
				LCD_sendstring(str_aux, LCD_L4_B);
			}
			f_close(&result);
			

		/****************** FINISH RECOGNIZING *******************/
		proc_state = NOTHING;

		// Free memory
		free(utterance);				utterance = NULL;
		
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
	
	uint16_t aux_cont;
	
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
					aux_cont = 20;
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
					if (aux_cont==0) //Dejo algunos milisegundos al principio sin grabar, para saltearme el glitch inicial
					{
						// Copy frame to buffer
						ringBuf_write ( args->buff, (uint8_t*) audio_frame, args->capt_conf.frame_size * sizeof(*audio_frame) );
					}
					else
					{
						aux_cont--;
					}
					
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

	// Escalo el THD_min_FMAX a índices en el buffer
	appconf.calib_conf.thd_min_fmax = appconf.calib_conf.thd_min_fmax * appconf.proc_conf.fft_len / appconf.capt_conf.freq;
	appconf.calib_conf.thd_max_fmax = appconf.calib_conf.thd_max_fmax * appconf.proc_conf.fft_len / appconf.capt_conf.freq;
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
		config->debug_conf.usb_buff_size	= (uint32_t)	ini_getl("Debug", "usb_buff_size", 128, filename);
	}
	else
	{
		config->debug_conf.save_proc_vars	= false;
		config->debug_conf.save_clb_vars	= false;
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
	
	// Read Speech Processing configuration
	config->vad_conf.vad					= 							ini_getbool	("VADConf",	"VAD",					VAD_ENABLE,		filename);
	config->vad_conf.age_thd			= (uint16_t)		ini_getl		("VADConf", "AGE_THD", 			AGE_THD,			filename);
	config->vad_conf.timeout_thd	= (uint16_t)		ini_getl		("VADConf", "TIMEOUT_THD",	TIMEOUT_THD,	filename);
	
	// Read AudioCalib configuration
	config->calib_conf.calib_time		= (uint16_t)	ini_getl("CalConf", "CALIB_TIME", 			CALIB_TIME, 			filename);
	config->calib_conf.thd_scl_eng	= (float32_t)	ini_getf("CalConf", "THD_Scale_ENERGY", THD_Scl_ENERGY,		filename);
	config->calib_conf.thd_min_fmax	= (uint32_t)	ini_getf("CalConf", "THD_min_FMAX",			THD_min_FMAX,			filename);
	config->calib_conf.thd_max_fmax	= (uint32_t)	ini_getf("CalConf", "THD_max_FMAX",			THD_max_FMAX,			filename);
	config->calib_conf.thd_scl_sf		= (float32_t)	ini_getf("CalConf", "THD_Scale_SF",			THD_Scl_SF,				filename);
		

	return 1;
}

void LCD_Init(void)
{
	int16_t var;
	
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LCD_REG_SEL_Port, LCD_REG_SEL_Pin, GPIO_PIN_RESET);//Selected command register
	osDelay(2);//espera a que arranque el display
	
  var = LCD_DATOS_8bits->ODR;
	var = var & 0x1100;
	var = var | 0x0038; //Function set: 2 Line, 8-bit, 5x7 dots
	LCD_DATOS_8bits->ODR = var;
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
	osDelay(2);
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	osDelay(2);//espero al lcd para que procese el comando
	
  var = LCD_DATOS_8bits->ODR;
	var = var & 0x1100;
	var = var | 0x000C; //Display on, Cursor off blinking off
	LCD_DATOS_8bits->ODR = var;
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
	osDelay(2);
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	osDelay(2);//espero al lcd para que procese el comando
	
  var = LCD_DATOS_8bits->ODR;
	var = var & 0x1100;
	var = var | 0x0006; //Entry mode, auto increment with no shift
	LCD_DATOS_8bits->ODR = var;
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
	osDelay(2);
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	osDelay(2);//espero al lcd para que procese el comando

  var = LCD_DATOS_8bits->ODR;
	var = var & 0x1100;
	var = var | 0x0001; //Clear LCD
	LCD_DATOS_8bits->ODR = var;
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
	osDelay(2);
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	osDelay(2);//espero al lcd para que procese el comando


  LCD_sendstring("Reconocimiento Habla",LCD_L1);
	LCD_sendstring("Num. reconocido:    ",LCD_L2);
	LCD_sendstring("Segundo puesto:     ",LCD_L3);
	LCD_sendstring("Nivel de rec.:      ",LCD_L4);

}

void LCD_senddata(char var, char pos)
{
	int16_t aux;
	
	if(pos)					 //si hay posicion la mando sino pongo el char donde esta
	{
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LCD_REG_SEL_Port, LCD_REG_SEL_Pin, GPIO_PIN_RESET);//Selected command register
	  aux = LCD_DATOS_8bits->ODR;
		aux = aux & 0x1100;
		aux = aux | pos; // Ingreso el caracter
		LCD_DATOS_8bits->ODR = aux;
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
		osDelay(2);
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
		osDelay(2);//espero al lcd para que procese el comando
	}

	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LCD_REG_SEL_Port, LCD_REG_SEL_Pin, GPIO_PIN_SET);//Selected data register
	aux = LCD_DATOS_8bits->ODR;
	aux = aux & 0x1100;
	aux = aux | var; // Ingreso el caracter
	LCD_DATOS_8bits->ODR = aux;
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
	osDelay(2);
	HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
	osDelay(2);//espero al lcd para que procese el comando

}

void LCD_sendstring(char *var, char pos)
{
	int16_t aux;
	
	if(pos)					 //si hay posicion la mando sino pongo el char donde esta
	{
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LCD_REG_SEL_Port, LCD_REG_SEL_Pin, GPIO_PIN_RESET);//Selected command register
	  aux = LCD_DATOS_8bits->ODR;
		aux = aux & 0x1100;
		aux = aux | pos; // Ingreso el caracter
		LCD_DATOS_8bits->ODR = aux;
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_SET);//habilito el dato
		osDelay(2);
		HAL_GPIO_WritePin(LCD_ENABLE_Port, LCD_ENABLE_Pin, GPIO_PIN_RESET);
		osDelay(2);//espero al lcd para que procese el comando
	}

	while(*var)              //mientras no sea el final del string
    	LCD_senddata(*var++, LCD_NOL);  //mando caracteres uno por uno
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
