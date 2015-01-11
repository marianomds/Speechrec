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
#include "ALE_stm32f4_discovery.h"
#include "stdlib.h"
#include "usb_host.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "minIni.h"


/* Private Variables used within Applivation ---------------------------------*/

AppConfig appconf;	/* Configuration structure */
	
extern char USBH_Path[4];     /* USBH logical drive path */
FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */

osMessageQId AppliEvent;
osMessageQId CaptureState;
osMessageQId KeyMsg;
osMessageQId ProcessFile;

osMutexId AudioBuffMutex;

//---------------------------------------
//					AUDIO CAPTURE VARIABLES
//---------------------------------------
__align(8)
uint8_t  PDMBuf [PDM_BUFF_SIZE*EXTEND_BUFF*2];
uint16_t PCM    [PCM_BUFF_SIZE*EXTEND_BUFF];
PDMFilter_InitStruct Filter[AUDIO_IN_CHANNEL_NBR];

//---------------------------------------
//					CALIBRATION VARIALBES
//---------------------------------------

__align(8)
	float32_t SilEnergy  [CALIB_LEN];
	float32_t SilFrecmax [CALIB_LEN];
	float32_t SilSpFlat	 [CALIB_LEN];
	
	
//---------------------------------------
//							VAD VARIALBES
//---------------------------------------
	#if _VAD_
	__align(8)
		float32_t GM,AM,aux;	// Gemotric Mean & Arithmetic Mean

		float32_t Energy;
	//	float32_t Frecmax;
		uint32_t Frecmax;
		float32_t SpFlat;
	//	float32_t SilZeroCross;

		float32_t SilEnergyMean;
	//	float32_t SilFrecmaxMean;
		float32_t SilSpFlatMean;
	//	float32_t SilZeroCrossMean;

		float32_t SilEnergyDev;
	//	float32_t SilFrecmaxDev;
		float32_t SilSpFlatDev;
	//	float32_t SilZeroCrossDev;

		float32_t THD_E;
		float32_t THD_SF;
		const uint8_t	THD_FL = AUDIO_IN_FREQ/CALIB_THD_FRECLOW;
		const uint8_t	THD_FH = AUDIO_IN_FREQ/CALIB_THD_FRECHIGH;

	#endif

//---------------------------------------
//				SPEECH PROCESSING VARIALBES
//---------------------------------------

	#ifdef DEBUG
		__align(8)
		float32_t speech[FRAME_LEN];			// Señal de audio escalada
		float32_t FltSig[FRAME_LEN];			// Señal de audio pasada por el Filtro de Pre-Enfasis
		float32_t WinSig[FRAME_LEN];			// Señal de filtrada multiplicada por la ventana de Hamming
		float32_t STFTWin[FFT_LEN];				// Transformada de Fourier en Tiempo Corto
		float32_t MagFFT[FFT_LEN/2];			// Módulo del espectro
		float32_t MelWin[MEL_BANKS];			// Espectro pasado por los filtros de Mel
		float32_t LogWin[IFFT_LEN];				// Logaritmo del espectro filtrado
		float32_t CepWin[IFFT_LEN];				// Señal cepstral
	#endif
	
	__align(8)
	float32_t var1 [FRAME_LEN];
	float32_t var2 [FFT_LEN];
	float32_t var3 [FFT_LEN/2];
	uint16_t  data [FRAME_LEN];
	float32_t MFCC [LIFTER_LEGNTH];
	
	float32_t HamWin 	 [FRAME_LEN];
	float32_t CepWeight [LIFTER_LEGNTH];
	float32_t pState    [NUMTAPS+FRAME_LEN-1];
	float32_t Pre_enfasis_Coeef [NUMTAPS]={-ALPHA,1};
	
	extern float32_t MelBank [256*20];
	extern arm_matrix_instance_f32	MelFilt;
	
	arm_matrix_instance_f32 				STFTMtx, MelWinMtx;
	arm_fir_instance_f32 						FirInst;	
	arm_rfft_fast_instance_f32 			RFFTinst, DCTinst;	
	

//---------------------------------------
//						APPLICATIONS TASKS
//---------------------------------------
/**
  * @brief  
  * @param  
  * @retval
  */
void Main_Thread (void const *pvParameters) {
	osEvent event;

	osThreadId CalibrationTaskID;
	osThreadId ProcessTaskID;
	osThreadId RecordTaskID;
	
	LED_On(OLED);
	
	for(;;) {
		
		event = osMessageGet(AppliEvent, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v) {
				
				case APPLICATION_START:
					{
						// Apago el LED Naranja
						LED_Off(OLED);
					
						/* Register the file system object to the FatFs module */
						if(f_mount(&USBDISKFatFs, (TCHAR const*)USBH_Path, 0) != FR_OK)
							Error_Handler();	/* FatFs Initialization Error */

						/* Read Configuration File */
						readConfigFile(CONFIG_FILE_NAME,&appconf);

						/* Set Environment variables */
						setEnvVar();
		
						/* Create Calibration Task*/
						#ifdef _VAD_
						// Tomo el Mutex para la tarea Calibration, antes de terminar libera el mutex
						osMutexWait (AudioBuffMutex, osWaitForever);
						osThreadDef(CalibrationTask, 	Calibration, 			osPriorityHigh, 		0, configMINIMAL_STACK_SIZE * 10);
						CalibrationTaskID = osThreadCreate (osThread(CalibrationTask), NULL);
						#endif

						/* Create Process Audio Task*/
						osThreadDef(ProcessTask, ProcessAudioFile, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 22);
						ProcessTaskID = osThreadCreate (osThread(ProcessTask), NULL);
						
						/* Create Record Audio Task*/
						osThreadDef(RecordTask, AudioRecord, osPriorityHigh, 0, configMINIMAL_STACK_SIZE * 10);
						RecordTaskID = osThreadCreate (osThread(RecordTask),  NULL);
						
						// Si cualquiera de los dos es NULL es un error
						if(!ProcessTaskID || !RecordTaskID) Error_Handler();
						break;
						}
				
				case APPLICATION_DISCONNECT:
					{
						// Apago el LED Naranja
						LED_On(OLED);
						
						// Elimino las tareas
						osThreadTerminate(CalibrationTaskID);
						osThreadTerminate(ProcessTaskID);
						osThreadTerminate(RecordTaskID);
						
						// Desmonto el FileSystem del USB
						f_mount(NULL, (TCHAR const*)"", 0);
						break;
					}
				
				default:
					break;
			}
		}
	
	}
}
#ifdef _VAD_
/**
* @brief  Calibration
* @param  
* @retval 
*/
void Calibration (void const * pvParameters) {
	osEvent event;
	uint32_t i=0;
	
	#ifdef DEBUG
		FIL WavFile;                   					/* File object */
		uint32_t byteswritten;     							/* File write/read counts */
		uint8_t pHeader [44];										/* Header del Wave File */
		uint32_t filesize = 0;									/* Size of the Wave File */	
		WAVE_FormatTypeDef WaveFormat;
		char Filename[]="SIL_00.wav";
	#endif
	
	/***********************************************
	 * ASSUME THAT ALL PERIPHERALS ARE INITIALIZED * 
	 ***********************************************/

	#ifdef DEBUG
		/* Create New Wave File */
		New_Wave_File(Filename,&WaveFormat,&WavFile,pHeader,&filesize);
	#endif
	
	/* Initialize PDM Decoder */
	PDMDecoder_Init(AUDIO_IN_FREQ,AUDIO_IN_CHANNEL_NBR,Filter);

	/* Starts Capturing Audio Process */
	if(AUDIO_IN_Record((uint16_t *)PDMBuf, PDM_BUFF_SIZE*EXTEND_BUFF) == AUDIO_ERROR)
		Error_Handler();
	
	// Prendo el LED Verde
	LED_On(GLED);
	
	for(;;)
	{
		event = osMessageGet(CaptureState, osWaitForever);
		if(event.status == osEventMessage && event.value.v == BUFFER_READY && i < CALIB_LEN)
		{
			#ifdef DEBUG
				/* WriteAudio File */
				if(f_write(&WavFile, (uint8_t*)PCM, PCM_BUFF_SIZE*EXTEND_BUFF*2, (void*)&byteswritten) != FR_OK) {
					f_close(&WavFile);
					Error_Handler();
				}
				filesize += byteswritten;
			#endif

			Calib(&SilEnergy[i],&SilFrecmax[i],&SilSpFlat[i]);
			i++;
		}
		if (i == CALIB_LEN)
		{
			// Stop Capture
			AUDIO_IN_Stop();
			
			#ifdef DEBUG
				// Update Wav File Header and close it
				f_lseek(&WavFile, 0);				
				WaveProcess_HeaderUpdate(pHeader, &WaveFormat, filesize);
				f_write(&WavFile, pHeader, 44, (void*)&byteswritten);
				f_close(&WavFile);
			#endif
			
			// Calculate Mean of Features
			arm_mean_f32 (SilEnergy,   CALIB_LEN, &SilEnergyMean);
//			arm_mean_f32 (SilFrecmax,  CALIB_LEN, &SilFrecmaxMean);
			arm_mean_f32 (SilSpFlat, CALIB_LEN, &SilSpFlatMean);

			// Calculate Deviation of Features
			arm_std_f32 (SilEnergy,   CALIB_LEN, &SilEnergyDev);
//			arm_std_f32 (SilFrecmax,  CALIB_LEN, &SilFrecmaxDev);
			arm_std_f32 (SilSpFlat, CALIB_LEN, &SilSpFlatDev);
			
			THD_E  = (float32_t) SilEnergyMean * CALIB_THD_ENERGY * SilEnergyDev;
			THD_SF = (float32_t) abs(SilSpFlatMean) * CALIB_THD_SF     * SilSpFlatDev;

			#ifdef DEBUG				
				FIL CalibFile;
				FRESULT res;
				char CalibFileName[]="Calib_00.bin";
				
				// Record values in file
				res = f_open(&CalibFile, CalibFileName, FA_CREATE_NEW | FA_WRITE);
				while (res == FR_EXIST)
					res = f_open(&CalibFile, updateFilename(CalibFileName), FA_CREATE_NEW | FA_WRITE);
				if (res !=FR_OK)	Error_Handler();
				
				// Guardo los vectores
				if(f_write(&CalibFile, &SilEnergy, 	sizeof(SilEnergy),	(void*)&byteswritten) != FR_OK) Error_Handler();
				if(f_write(&CalibFile, &SilFrecmax, sizeof(SilFrecmax),	(void*)&byteswritten) != FR_OK) Error_Handler();
				if(f_write(&CalibFile, &SilSpFlat,	sizeof(SilSpFlat),	(void*)&byteswritten) != FR_OK) Error_Handler();
				
				// Guardo las medias
				if(f_write(&CalibFile, &SilEnergyMean, 	sizeof(SilEnergyMean), 	(void*)&byteswritten) != FR_OK) Error_Handler();
//				if(f_write(&CalibFile, &SilFrecmaxMean, sizeof(SilFrecmaxMean),	(void*)&byteswritten) != FR_OK) Error_Handler();
				if(f_write(&CalibFile, &SilSpFlatMean,	sizeof(SilSpFlatMean),	(void*)&byteswritten) != FR_OK) Error_Handler();
				
				// Guardo las varianzas
				if(f_write(&CalibFile, &SilEnergyDev, 	sizeof(SilEnergyDev), 	(void*)&byteswritten) != FR_OK) Error_Handler();
//				if(f_write(&CalibFile, &SilFrecmaxDev,	sizeof(SilFrecmaxDev), 	(void*)&byteswritten) != FR_OK) Error_Handler();
				if(f_write(&CalibFile, &SilSpFlatDev,		sizeof(SilSpFlatDev),		(void*)&byteswritten) != FR_OK) Error_Handler();
				
				if(f_write(&CalibFile, &THD_FL,	sizeof(THD_FL), 	(void*)&byteswritten) != FR_OK) Error_Handler();
				if(f_write(&CalibFile, &THD_FH,	sizeof(THD_FH), 	(void*)&byteswritten) != FR_OK) Error_Handler();
				
				f_close(&CalibFile);
			#endif	
			
			// Apago el led
			LED_Off(GLED);
			
			// Libero el Mutex
			osMutexRelease(AudioBuffMutex);
			
			// Elimino la tarea
			osThreadTerminate(osThreadGetId());
		}
	}
}
#endif
/**
* @brief  Audio Capture
* @param  
* @retval 
*/
void AudioRecord (void const * pvParameters) {

	osEvent event;													/* Message Events */
	
	WAVE_FormatTypeDef WaveFormat;
	FIL WavFile;                   					/* File object */
  uint32_t byteswritten;     							/* File write/read counts */
  uint8_t pHeader [44];										/* Header del Wave File */
	uint32_t filesize = 0;									/* Size of the Wave File */
	FRESULT res;
	char Filename[] = "Aud_00.wav";
	char *message;
	bool fileisopen=0; 
	
	/***********************************************
	 * ASSUME THAT ALL PERIPHERALS ARE INITIALIZED * 
	 ***********************************************/

	/* Initialize PDM Decoder */
	PDMDecoder_Init(AUDIO_IN_FREQ,AUDIO_IN_CHANNEL_NBR,Filter);

	// Tomo el mutex y no lo libero más
	osMutexWait (AudioBuffMutex, osWaitForever);
	
	/* Start Task */
	for (;;) {
		event = osMessageGet(CaptureState, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v) {

				case BUFFER_READY:
					if(fileisopen)
					{
						/* write buffer in file */
						if(f_write(&WavFile, (uint8_t*)PCM, PCM_BUFF_SIZE*EXTEND_BUFF*2, (void*)&byteswritten) != FR_OK) {
							f_close(&WavFile);
							Error_Handler();
						}
						filesize += byteswritten;
					}
					break;

									
				case BUTTON_PRESS:
					/* Create New Wave File */
					res = New_Wave_File(Filename,&WaveFormat,&WavFile,pHeader,&filesize);
					if(res!=FR_OK)
						Error_Handler();						
						
					// File is open true
					fileisopen = true;
					
					// LEDs states
					LED_On(RLED);
				
					/* Starts Capturing Audio Process */
					if(AUDIO_IN_Record((uint16_t *)PDMBuf, PDM_BUFF_SIZE*EXTEND_BUFF) == AUDIO_ERROR)
						Error_Handler();				
				break;
				
							
				case BUTTON_RELEASE:
					
					// Pause Audio Record
					AUDIO_IN_Stop();

					// File is open true
					fileisopen = false;
				
					// LEDs states
					LED_Off(RLED);
				
					/* Update Wav File Header and close it*/
					WaveProcess_HeaderUpdate(pHeader, &WaveFormat, filesize);
					f_lseek(&WavFile, 0);				
					f_write(&WavFile, pHeader, 44, (void*)&byteswritten);
					f_close(&WavFile);
								
					/* Put Message to ProcessAudio Task */
					message = pvPortMalloc(strlen(Filename)+1);
					strcpy(message,Filename);
					osMessagePut(ProcessFile,(uint32_t) message,0);

				break;
								
				
				default:
				break;
			}
		}
	}
}

/**
	* @brief	Procesa el archivo de Audio que se pasa por eveneto y almacena en un nuevo archivo los MFCC indicando también a que Nº de frame corresponde
	* @param
	* @param
	*/
void ProcessAudioFile (void const *pvParameters) {
	
	FIL WaveFile, MFCCFile;
	UINT bytesread, byteswritten;
	osEvent event;
	uint32_t audiosize, frameNum=0;

	#ifdef DEBUG
		char *DirName;
		FIL HamWinFile, CepWeiFile, SpeechFile, FltSigFile, WinSigFile, STFTWinFile, \
				MagFFTFile, MelWinFile, LogWinFile, CepWinFile, nosirve;
		#if _VAD_
			FIL VADFile, EnerFile, FrecFile, SFFile;
			uint8_t vadoutput;
		#endif
	#else
		char MFCCFilename[] = "MFCC_00.bin";
	#endif
	
	
	/* START TASK */
	for(;;) {
		
		event = osMessageGet(ProcessFile,osWaitForever);
		if(event.status == osEventMessage) {		
			// Aviso mediante el LED Azul que comienzo a procesar el archivo
			LED_On(BLED);

			// Abro el archivo de audio
			if(f_open(&WaveFile,(char *)event.value.p,FA_READ) != FR_OK)				Error_Handler();
			
			#ifdef DEBUG
				// Creo un directorio con el nombre del archivo
				DirName = pvPortMalloc(strlen(event.value.p)-strlen(".wav")+1);					// Le saco la extensión ".wav"
				strlcpy(DirName,event.value.p,strlen(event.value.p)-strlen(".wav")+1);		// Creo un string con el nombre del archivo y sin la extensión
				if (f_mkdir (DirName) != FR_OK)	Error_Handler();
				if (f_chdir (DirName) != FR_OK)	Error_Handler();
				vPortFree(DirName);
			
				//Abro los archivos
				if(f_open(&HamWinFile, "HamWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&CepWeiFile, "CepWei.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&SpeechFile, "Speech.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&FltSigFile, "FltSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&WinSigFile, "WinSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&STFTWinFile,"FFTWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&MagFFTFile, "MagFFT.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&MelWinFile, "MelWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&LogWinFile, "LogWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&CepWinFile, "CepWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				if(f_open(&MFCCFile, 	 "MFCC.bin",   FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				
				#if _VAD_
					if(f_open(&VADFile,		"VAD.bin",    	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
					if(f_open(&EnerFile,	"Energy.bin",   FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
					if(f_open(&FrecFile,  "FrecMax.bin",  FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
					if(f_open(&SFFile,		"SpecFlat.bin",	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				#endif

				if(f_open(&nosirve, "nosirve.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
				
				// Libero la memoria del string alocada en AudioRecord
				vPortFree(event.value.p);

				// Vuelvo el directorio atras
				if (f_chdir ("..") != FR_OK)	Error_Handler();
				
				// Escribo un archivo con los coeficientes de la ventana de Hamming			
				if(f_write(&HamWinFile, HamWin, sizeof(HamWin), &byteswritten) != FR_OK)			Error_Handler();
				f_close(&HamWinFile);			
				
				// Escribo un archivo con los coeficientes del Filtro Cepstral
				if(f_write(&CepWeiFile, CepWeight, sizeof(CepWeight), &byteswritten) != FR_OK)	Error_Handler();
				f_close(&CepWeiFile);
			#else
				// Creo un archivo relacionado con el audio para escribir los MFCC
				updateFilename(MFCCFilename);
				if(f_open(&MFCCFile, 	MFCCFilename, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
			#endif			
			
			// Leo el tamaño del audio (almacenado en 4 bytes) y queda listo para leer el audio
			if(f_lseek(&WaveFile,40) != FR_OK)														Error_Handler();									// Voy a la parte donde me indica el tamaño del audio
			if(f_read (&WaveFile, &audiosize, sizeof(audiosize), &bytesread) != FR_OK)		Error_Handler();	// Leo el tamaño del archivo de audio
			audiosize /= 2;																																									// lo divido por dos porque viene en cantidad de bytes
			
			// Proceso todos los datos hasta leer todo el archivo
			do
			{
				// Leo un Frame del archivo
				if(f_read (&WaveFile, data, sizeof(data), &bytesread) != FR_OK)
					Error_Handler();
				
				// Si estoy en el último Frame relleno el final del Frame con ceros
				if(audiosize < FRAME_LEN)
					arm_fill_q15 (0,(q15_t *) &data[audiosize], FRAME_LEN-audiosize);
				
				#if _VAD_
					// Si es un Frame donde hay voz, obtengo los MFCC 
					vadoutput = VAD_MFCC_float (MFCC, data, HamWin, CepWeight);
					if(vadoutput)
				#else
					if(MFCC_float (MFCC, data, HamWin, CepWeight))
				#endif
				{
					// Escribo los valores intermedios en un archivo
					if(f_write(&MFCCFile,   MFCC,   sizeof(MFCC),&byteswritten) != FR_OK)	Error_Handler();	
					
					#ifdef DEBUG
						//Escribo los valores intermedios en un archivo
						if(f_write(&SpeechFile,  speech,  	sizeof(speech),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&FltSigFile,  FltSig,  	sizeof(FltSig),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&WinSigFile,  WinSig,  	sizeof(WinSig),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&STFTWinFile, STFTWin, 	sizeof(STFTWin), 	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&MagFFTFile,  MagFFT,  	sizeof(MagFFT),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&MelWinFile,  MelWin,  	sizeof(MelWin),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&LogWinFile,  LogWin,  	sizeof(LogWin),  	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&CepWinFile,  CepWin,  	sizeof(CepWin),  	&byteswritten) != FR_OK)	Error_Handler();

						#if _VAD_
							if(f_write(&VADFile,	&vadoutput,	sizeof(vadoutput),&byteswritten) != FR_OK)	Error_Handler();
							if(f_write(&EnerFile,	&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler();
							if(f_write(&FrecFile,	&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler();
							if(f_write(&SFFile,		&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler();
						#endif
					
						if(f_write(&nosirve,  &frameNum,  	sizeof(frameNum),  	&byteswritten) != FR_OK)	Error_Handler();

					#endif
				}
				#ifdef DEBUG
				else
				{
					arm_fill_f32 	(0,var1,FRAME_LEN);
					
					//Escribo todo en 0
					if(f_write(&SpeechFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&FltSigFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&WinSigFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&STFTWinFile, var1, 		sizeof(var1), 	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&MagFFTFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&MelWinFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&LogWinFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();
					if(f_write(&CepWinFile,  var1,  	sizeof(var1),  	&byteswritten) != FR_OK)	Error_Handler();

					#if _VAD_
						if(f_write(&VADFile,	&vadoutput,	sizeof(vadoutput),&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&EnerFile,	&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&FrecFile,	&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler();
						if(f_write(&SFFile,		&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler();
					#endif
				
					if(f_write(&nosirve,  &frameNum,  	sizeof(frameNum),  	&byteswritten) != FR_OK)	Error_Handler();

				}
				#endif
			
				
				//TODO: EN VEZ DE LEER TODO UN FRAME DEVUELTA ME CONVIENE HACER UNA COLA CIRCULAR DONDE VOY DESPLAZANDO LOS DATOS
				// Vuelvo atras en el archivo por el overlap que hay en las señales
				if(f_lseek(&WaveFile,f_tell(&WaveFile)-FRAME_OVERLAP*2) != FR_OK)
					Error_Handler();
				
				// Actualizo lo que me queda por leer
				audiosize -= FRAME_NET;
				
				// Incremento el Nº de Frame
				frameNum++;
				
			} while(bytesread >= sizeof(data));	// Como los datos leidos son menores ==> EOF

			// Cierro los archivos
			f_close(&WaveFile);
			f_close(&MFCCFile);
			
			#ifdef DEBUG
				f_close(&SpeechFile);
				f_close(&FltSigFile);
				f_close(&WinSigFile);
				f_close(&STFTWinFile);
				f_close(&MagFFTFile);
				f_close(&MelWinFile);
				f_close(&LogWinFile);
				f_close(&CepWinFile);
				f_close(&nosirve);
				#if _VAD_	
					f_close(&VADFile);
					f_close(&EnerFile);
					f_close(&FrecFile);
					f_close(&SFFile);
				#endif
			#endif

			// Aviso mediante el LED Azul que termine de Procesar el Audio
			LED_Off(BLED);
		}
	}
}
/**
  * @brief  KeyBoard Handler Task
	* @param  
  * @retval 
  */
void Keyboard (void const * pvParameters) {
	osEvent event;
	static uint8_t state=0;
	uint8_t var;
	for(;;) {
		event = osMessageGet(KeyMsg,osWaitForever);
		if(event.status == osEventMessage) {
			
			osDelay(300);													// Waint 300msec for establishment time
			var = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0);
			
			// Pregunto para eliminiar falsos
			if(var != state)
			{
				state = var;
				if(var)
					osMessagePut(CaptureState,BUTTON_PRESS,0);
				else
					osMessagePut(CaptureState,BUTTON_RELEASE,0);
			}
//				
//			event = osMessageGet(KeyMsg,1000);
//			switch(event.status) {
//				case osEventMessage:
//					osMessagePut(CaptureState,BUTTON_PRESS,500);
//					break;
//				case osEventTimeout:
//					osMessagePut(CaptureState,BUTTON_RETAIN_1s,500);
//					break;
//				default:
//					break;
//			}
			
			HAL_NVIC_EnableIRQ(EXTI0_IRQn);				// Enable once more EXTI0 IRQ
		}
	}
}
/**
  * @brief  Recognized Task
	* @param  
  * @retval 
  */
void Recognize (void const *pvParameters) {

	osEvent event;
	FIL MFCCFile;
	UINT byteswritten;
	static uint16_t data[FRAME_LEN];
	static float32_t MFCC[LIFTER_LEGNTH];
	uint8_t	reconocer = 0;
	char filename[] = "TmpMFCC.txt";
	Patterns *Pat;

	/***********************************************
	 * ASSUME THAT ALL PERIPHERALS ARE INITIALIZED * 
	 ***********************************************/
	
	// Load Patterns
	readPaternsConfigFile (appconf.patfilename,Pat);
	
	/* Initialize PDM Decoder */
	PDMDecoder_Init(AUDIO_IN_FREQ,AUDIO_IN_CHANNEL_NBR,Filter);

	/* Starts Capturing Audio Process */
	if(AUDIO_IN_Record((uint16_t *)PDMBuf, PDM_BUFF_SIZE*EXTEND_BUFF) == AUDIO_ERROR)
		Error_Handler();

	// Abro un archivo para los MFCC temporal
	if(f_open(&MFCCFile,"TmpMFCC.txt",FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK)
		Error_Handler();

	/* Prendo el Led Azul indicando que estoy reconociendo */
	LED_On(OLED);

	/* START TASK */
	for (;;)
	{
		event = osMessageGet(CaptureState, osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v) {
				
				case BUFFER_READY:
					{
						// Si es voz, calculo los MFCC y grabo en un archivo temporal
						#ifdef _VAD_
						if(VAD_MFCC_float(MFCC, data, HamWin, CepWeight))
						#else
						if(MFCC_float(MFCC, data, HamWin, CepWeight))
						#endif
						{
							f_write(&MFCCFile,MFCC,sizeof(MFCC)*LIFTER_LEGNTH,&byteswritten);
							reconocer=1;
						}
						else {
							// Si no es un frame de voz, pregunto si debo reconocer
							if(reconocer)
							{
								// Paro de recibir audio
								AUDIO_IN_Pause();
								
								// Voy al inicio del archivo
								f_lseek(&MFCCFile,0);

								// Comparo con los patrones
									// Llamo a la función DTW
								
								// Envío un mensaje con el patrón reconocido
								
								// Activo el audio nuevamente
								AUDIO_IN_Resume();
								
								// Flusheo el archio y voy al inicio
								f_sync(&MFCCFile);
								f_lseek(&MFCCFile,0);
								
								// Reseteo reconocer
								reconocer = 0;
							}
						}
						break;
					}
				case BUTTON_RETAIN_1s:
					{					
						// Stop Audio Record
						AUDIO_IN_Stop();
					
						// Cierro el archivo y lo borro
						f_close(&MFCCFile);
						f_unlink(filename);
						// Apago el LED Naranja
						LED_Off(OLED);
					
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
	osMessageQDef(AppliEvent,10,uint16_t);
	AppliEvent = osMessageCreate(osMessageQ(AppliEvent),NULL);

	/* Create User Button MessageQ */
	osMessageQDef(KeyMsg,10,uint8_t);
	KeyMsg = osMessageCreate(osMessageQ(KeyMsg),NULL);
	
	/* Create Capture Task State MessageQ */
	osMessageQDef(CaptureState,10,uint32_t);
	CaptureState = osMessageCreate(osMessageQ(CaptureState),NULL);
	
	/* Create Process Task State MessageQ */
	osMessageQDef(ProcessFile,10,char *);
	ProcessFile = osMessageCreate(osMessageQ(ProcessFile),NULL);	
	
	/* Create Audio Buffer Mutex */
	osMutexDef (AudioBuff);
	AudioBuffMutex = osMutexCreate (osMutex(AudioBuff));
	
	/* Create Keyboard Task*/
	osThreadDef(KeyboardTask, Keyboard, osPriorityRealtime, 0, configMINIMAL_STACK_SIZE);
	osThreadCreate (osThread(KeyboardTask), NULL);
	
	/* Create Main Task*/
	osThreadDef(MainTask, Main_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE*10);
	osThreadCreate (osThread(MainTask), NULL);


	/* Hammin Window */
	Hamming_float(HamWin,FRAME_LEN);
	
	/* Cepstral Weigths */
	Lifter_float(CepWeight,LIFTER_LEGNTH);
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
uint8_t readConfigFile (const char *filename, AppConfig *Conf) {
	
	// Read Speech Processing configuration
	Conf->time_window		= (uint16_t) 	ini_getl("SPConf", "TIME_WINDOW", 	TIME_WINDOW,		filename);
	Conf->time_overlap	= (uint16_t) 	ini_getl("SPConf", "TIME_OVERLAP",	TIME_OVERLAP,		filename);
	Conf->numtaps				= (uint8_t)		ini_getl("SPConf", "NUMTAPS",				NUMTAPS,				filename);
	Conf->alpha					= (float32_t)	ini_getl("SPConf", "ALPHA", 	(long)ALPHA,					filename);
	Conf->fft_len				= (uint8_t)		ini_getl("SPConf", "FFT_LEN", 			FFT_LEN,				filename);
	Conf->mel_banks			= (uint8_t)		ini_getl("SPConf", "MEL_BANKS",			MEL_BANKS,			filename);
	Conf->ifft_len			= (uint8_t)		ini_getl("SPConf", "IFFT_LEN", 			IFFT_LEN,				filename);
	Conf->lifter_legnth	= (uint8_t)		ini_getl("SPConf", "LIFTER_LEGNTH",	LIFTER_LEGNTH,	filename);
	
	// Read Calibration configuration
	Conf->calib_time				= (uint8_t)		ini_getl("CalConf", "CALIB_TIME", 				CALIB_TIME,					filename);
	Conf->calib_len					= (uint8_t)		ini_getl("CalConf", "CALIB_LEN",					CALIB_LEN,					filename);
	Conf->calib_thd_energy	= (uint8_t)		ini_getl("CalConf", "CALIB_THD_ENERGY", 	CALIB_THD_ENERGY,		filename);
	Conf->calib_thd_freclow	= (uint8_t)		ini_getl("CalConf", "CALIB_THD_FRECLOW", 	CALIB_THD_FRECLOW,	filename);
	Conf->calib_thd_frechigh= (uint8_t)		ini_getl("CalConf", "CALIB_THD_FRECHIGH", CALIB_THD_FRECHIGH,	filename);
	Conf->calib_thd_sf			= (float32_t)	ini_getl("CalConf", "CALIB_THD_SF", (long)CALIB_THD_SF,				filename);

	// Read Patterns configuration
	ini_gets("PatConf", "PAT_DIR", 				PAT_DIR, 				Conf->patdir, 			sizeof(Conf->patdir), 			filename);
	ini_gets("PatConf", "PAT_CONF_FILE",	PAT_CONF_FILE,	Conf->patfilename,	sizeof(Conf->patfilename),	filename);

	return 1;
}
/**
  * @brief  Carga los patrones
	* 				Carga el archivo de configuración de los patrones y también los patrones en si.
	*
	* @param [in] 	Filename to read
	* @param [out]	Puntero a la estructura de patrones 
  * @retval 			OK-->"1"  Error-->"0"
  */
uint8_t readPaternsConfigFile (const char *filename, Patterns *Pat) {
	
	char section[50];
	uint32_t i;
	uint32_t PatNum;
	
	// Count how many Patterns there are
	for (PatNum = 0; ini_getsection(PatNum, section, sizeof(section), filename) > 0; PatNum++);
	
	// Allocate memory for the Patterns
	Pat = (Patterns *) pvPortMalloc(sizeof(Patterns)*PatNum);
	if(!Pat)	return 0;

	// Loop for all sections ==> All Patterns
	for (i = 0; ini_getsection(i, section, sizeof(section), filename) > 0; i++)
	{
		// Get Pattern Name
		ini_gets(section, "PAT_NAME", "", Pat[i].PatName, sizeof(Pat->PatName), filename);
		
		// Get Pattern Activation Number
		Pat[i].PatActvNum	= (uint16_t) 	ini_getl(section, "PAT_NUM", 0, filename);
	}
		
	return 1;
}
/**
  * @brief  Read Atributes from Pattern File
	* @param  Filename to read
  * @retval OK-->"1"  Error-->"0"
  */
uint8_t LoadPatterns (const char *PatName, struct VC* ptr) {

	DIR PatternsDir;
	FIL File;
	UINT bytesread;
	uint32_t filesize;
	char Filename[13];

	// Abro la carpeta donde estan los patrones
	if(f_opendir(&PatternsDir, PAT_DIR))
		return 0;
	
	//Abro el archivo
	strlcpy(Filename, PatName, sizeof(Filename));	// Copio el nombre a una variable temporal
	strlcat(Filename, ".bin", sizeof(Filename));	// Agrego la extensión del archivo
	if(f_open(&File, Filename, FA_READ))					// Abro el archivo
		return 0;
	
	// Me fijo el tamaño del archivo
	filesize = f_size (&File);
	
	// Aloco la memoria necesaria para almacenar en memoria todos los Vectores de Características
	ptr = (struct VC *) pvPortMalloc(filesize);
	if(ptr)	return 0;
	
	// Leo el archivo con los patrones
	if(f_read (&File, &ptr, filesize, &bytesread) != FR_OK)
		return 0;
	
	// Cierro el archivo
	f_close(&File);

	// Cierro la carpeta donde estan los patrones
	f_closedir(&PatternsDir);
	
	return 1;
}
/**
  * @brief  Error Handler for general Errors
	* @param
  * @retval 
  */
void Error_Handler(void) {
  while(1){
		LED_Toggle(RLED);
		LED_Toggle(GLED);
		LED_Toggle(BLED);
		LED_Toggle(OLED);
			
		osDelay(100);
	}
}
//---------------------------------------
//						AUDIO FUNCTIONS
//---------------------------------------
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void Hamming_float (float32_t *Hamming, uint32_t Length) {
	uint32_t i;
	const float32_t a=0.54 , b=0.46;
	
	for(i=0; i < Length; i++) {
		Hamming[i] = a - b * arm_cos_f32 ((float32_t)2*PI*i/Length);
	}
}


/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void Lifter_float (float32_t *Lifter, uint32_t Length) {
	
	float32_t theta;
	uint32_t n;
	
	for(n=0; n<Length; n++) {
		theta = PI*n/Length;
		Lifter [n] = 1 + arm_sin_f32(theta) * Length/2;
	}
}


#ifdef _VAD_
/**
* @brief  Calibration
* @param  
* @retval 
*/
void Calib (float32_t *Ener, float32_t *Fmax, float32_t *SpFt) {

	uint32_t Index;
	
	/* Convierto a Float y escalo */
	arm_q15_to_float((q15_t*)PCM, var1, FRAME_LEN);

	/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
	arm_fir_init_f32 (&FirInst, NUMTAPS, Pre_enfasis_Coeef, pState, FRAME_LEN);
	arm_fir_f32 (&FirInst, var1, var2, FRAME_LEN);

	/* Se le aplica la ventana de Hamming al segmento obtenido */
	arm_mult_f32 (var2, HamWin, var1, FRAME_LEN);

	/* Calculo la energía */
	arm_power_f32 (var1, FRAME_LEN, Ener);
	*Ener = *Ener/FRAME_LEN;

	/* Se calcula la STFT */
	if(arm_rfft_fast_init_f32 (&RFFTinst, FFT_LEN))		Error();
	arm_rfft_fast_f32(&RFFTinst,var1,var2,0);
	var2[1]=0;																									//Borro la componente de máxima frec

	/* Calculo el módulo de la FFT */
	arm_cmplx_mag_squared_f32  (var2, var1, FFT_LEN/2);

	/* Obtengo la Fmax */
	arm_max_f32 (var1, FFT_LEN/2, &aux, &Index);									// no me interesa el valor máximo, sino en que lugar se encuentra
	*Fmax = Index;

	/* Calculo el Spectral Flatness */
	arm_mean_f32 (var1, FFT_LEN/2, &AM);	// Arithmetic Mean
	sumlog(var1, &GM, FFT_LEN/2);
	GM = expf(GM/(FFT_LEN/2));							// Geometric Mean
	*SpFt = 10*log10f(GM/AM);
	
//	arm_mean_f32 (var1, FFT_LEN/2, &AM);	// Arithmetic Mean
//	arm_scale_f32 (var1, (1/AM), var2, FFT_LEN/2);
//	sumlog(var2, &GM, FFT_LEN/2);
//	GM = expf(GM/(FFT_LEN/2));							// Geometric Mean
//	*SpFt = 10*log10f(GM/AM);
}
/**
  * @brief  Execute VAD Algorithm and if it's a voiced frame it calculates the Mel-Frequency Cepstrum Coefficients
	* @param  SpeechFrame: Address of the vector with the Speech signal of the Frame to be processed
	* @param  SigSize: Length of the Frame
	* @param  HamWin: Address of the vector with the Window to apply
	* @param  Lifter: Address of the vector of the Lifter Coefficents to apply
	* @param  alpha: Coefficient of the Pre-emphasis Filter
	* @param  CoefNum: 
	* @param  L: 
	* @retval VADout: Returns if it is a valid Vocied frame or not
  */
uint8_t VAD_MFCC_float (float32_t *MFCC, uint16_t *Frame, float32_t *HamWin, float32_t *Lifter) {

	uint32_t Index,i;

	/* ========= START ========= */
	
	//Convierto a Float y escalo
	arm_q15_to_float((q15_t*)Frame, var1, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var1,speech,FRAME_LEN);	// Copio la señal filtrada para cuestiones de DEBUG
	#endif
	
	
	/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
	arm_fir_init_f32 (&FirInst, NUMTAPS, Pre_enfasis_Coeef, pState, FRAME_LEN);
	arm_fir_f32 (&FirInst, var1, var2, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var2,FltSig,FRAME_LEN);	// Copio la señal filtrada
	#endif
	
	
	/* Se le aplica la ventana de Hamming al segmento obtenido */
	arm_mult_f32 (var2, HamWin, var1, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var1,WinSig,FRAME_LEN);	// Copio la señal pasda por la ventana de Hamming
	#endif


	/* Calculo la energía */
	arm_power_f32 (var1, FRAME_LEN, &Energy);
	Energy = Energy/FRAME_LEN;
	
// REVISAR LO DE ABAJO, NO SE SI ESTA BIEN
//	/* Hago Zero-Padding si es necesario */
//	if(FFT_LEN != FRAME_LEN){
//		//Relleno con ceros en la parte izquierda
//		arm_fill_f32 	(0,STFTWin,(FFT_LEN-FRAME_LEN)/2);
//		//Relleno con ceros en la parte derecha
//		arm_fill_f32 	(0,&STFTWin[(FFT_LEN+FRAME_LEN)/2],(FFT_LEN-FRAME_LEN)/2);
//	}
	
	/* Se calcula la DFT */
	if(arm_rfft_fast_init_f32 (&RFFTinst, FFT_LEN) == ARM_MATH_ARGUMENT_ERROR)		Error();			// Si el tamaño de la FFT no es aceptado da Error
	arm_rfft_fast_f32(&RFFTinst,var1,var2,0);																										// Se calcula la FFT		
	var2[1]=0;																																									// Borro la componente de máxima frec
	#ifdef DEBUG
		arm_copy_f32 (var2,STFTWin,FFT_LEN);																											// Copio la Transformada de Fourier en Tiempo Corto
	#endif
	
	
	/* Calculo el módulo de la STFT */
	arm_cmplx_mag_squared_f32  (var2, var3, FFT_LEN/2);			// Se calcula el módulo de la STFT
	#ifdef DEBUG
		arm_copy_f32 (var3,MagFFT,FFT_LEN/2);										// Copio el módulo de la STFT
	#endif
	
	
	/* Obtengo la Fmax */
	arm_max_f32 (var3, FFT_LEN/2, &aux, &Index);
	Frecmax=Index;

	/* Calculo el Spectral Flatness */
	arm_mean_f32 (var3, FFT_LEN/2, &AM);	// Arithmetic Mean
	sumlog(var3,&GM,FFT_LEN/2);
	GM = expf(GM/(FFT_LEN/2));						// Geometric Mean
	SpFlat = abs(10*log10f(GM/AM));


	/* Check if it is a Voiced Frame */
	if( (Energy > THD_E   &&   THD_FL < Frecmax && Frecmax < THD_FH)   ||   SpFlat > THD_SF)
	{			
		/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
		arm_mat_init_f32 (&STFTMtx,   FFT_LEN/2, 1, var3);																				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
		arm_mat_init_f32 (&MelWinMtx, MEL_BANKS, 1, var2);																				// Se crea una matriz para almacenar el resultado
		if(arm_mat_mult_f32(&MelFilt, &STFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)	Error();	// MelFilt[MEL_BANKS,FFT_LEN] * MagFFTMtx[FFT_LEN,1] = MelWinMtx [MEL_BANKS,1]
		#ifdef DEBUG
			arm_copy_f32 (var2,MelWin,MEL_BANKS);																											// Copio el espectro luego de aplicado el banco de filtros Mel
		#endif
		
				
		/* Se obtienen los valores logaritmicos de los coeficientes y le hago zero-padding */
		arm_fill_f32 	(0,var1,IFFT_LEN);
		for (i=0; i<MEL_BANKS; i++)
			var1[i*2+2] = log10f(var2[i]);
		#ifdef DEBUG
			arm_copy_f32 (var1,LogWin,IFFT_LEN);										// Copio la señal filtrada para cuestiones de DEBUG
		#endif
		
		
		/* Se Anti-transforma aplicando la DCT-II ==> hago la anti-transformada real */
		if(arm_rfft_fast_init_f32 (&DCTinst, IFFT_LEN) == ARM_MATH_ARGUMENT_ERROR)	Error();
		arm_rfft_fast_f32(&DCTinst,var1,var2,1);
		#ifdef DEBUG
			arm_copy_f32 (var2,CepWin,IFFT_LEN);										// Copio la señal filtrada para cuestiones de DEBUG
		#endif
		
		
		/* Se pasa la señal por un filtro en el campo Cepstral */
		arm_mult_f32 (var2,Lifter,MFCC,LIFTER_LEGNTH);
		
		// Valid Voiced Frame
		return 1;
	}
	
	return 0;
}
#else
/**
  * @brief  Obtains the Mel-Frequency Cepstrum Coefficients
	* @param  SpeechFrame: Address of the vector with the Speech signal of the Frame to be processed
	* @param  SigSize: Length of the Frame
	* @param  HamWin: Address of the vector with the Window to apply
	* @param  Lifter: Address of the vector of the Lifter Coefficents to apply
	* @param  alpha: Coefficient of the Pre-emphasis Filter
	* @param  CoefNum: 
	* @param  L: 
  * @retval 
  */
uint8_t MFCC_float (float32_t *MFCC, uint16_t *Frame, float32_t *HamWin, float32_t *Lifter) {

	uint32_t i;

	/* ========= START ========= */
	
	//Convierto a Float y escalo
	arm_q15_to_float((q15_t*)Frame, var1, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var1,speech,FRAME_LEN);	// Copio la señal filtrada para cuestiones de DEBUG
	#endif
	
	
	/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
	arm_fir_init_f32 (&FirInst, NUMTAPS, Pre_enfasis_Coeef, pState, FRAME_LEN);
	arm_fir_f32 (&FirInst, var1, var2, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var2,FltSig,FRAME_LEN);	// Copio la señal filtrada
	#endif
	
	
	/* Se le aplica la ventana de Hamming al segmento obtenido */
	arm_mult_f32 (var2, HamWin, var1, FRAME_LEN);
	#ifdef DEBUG
		arm_copy_f32 (var1,WinSig,FRAME_LEN);	// Copio la señal pasda por la ventana de Hamming
	#endif

	
// REVISAR LO DE ABAJO, NO SE SI ESTA BIEN
//	/* Hago Zero-Padding si es necesario */
//	if(FFT_LEN != FRAME_LEN){
//		//Relleno con ceros en la parte izquierda
//		arm_fill_f32 	(0,STFTWin,(FFT_LEN-FRAME_LEN)/2);
//		//Relleno con ceros en la parte derecha
//		arm_fill_f32 	(0,&STFTWin[(FFT_LEN+FRAME_LEN)/2],(FFT_LEN-FRAME_LEN)/2);
//	}
	
	/* Se calcula la DFT */
	if(arm_rfft_fast_init_f32 (&RFFTinst, FFT_LEN) == ARM_MATH_ARGUMENT_ERROR)		Error();			// Si el tamaño de la FFT no es aceptado da Error
	arm_rfft_fast_f32(&RFFTinst,var1,var2,0);																										// Se calcula la FFT		
	var2[1]=0;																																									// Borro la componente de máxima frec
	#ifdef DEBUG
		arm_copy_f32 (var2,STFTWin,FFT_LEN);																											// Copio la Transformada de Fourier en Tiempo Corto
	#endif
	
	
	/* Calculo el módulo de la STFT */
	arm_cmplx_mag_squared_f32  (var2, var3, FFT_LEN/2);			// Se calcula el módulo de la STFT
	#ifdef DEBUG
		arm_copy_f32 (var3,MagFFT,FFT_LEN/2);										// Copio el módulo de la STFT
	#endif
	
			
	/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
	arm_mat_init_f32 (&STFTMtx,   FFT_LEN/2, 1, var3);																				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
	arm_mat_init_f32 (&MelWinMtx, MEL_BANKS, 1, var2);																				// Se crea una matriz para almacenar el resultado
	if(arm_mat_mult_f32(&MelFilt, &STFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)	Error();	// MelFilt[MEL_BANKS,FFT_LEN] * MagFFTMtx[FFT_LEN,1] = MelWinMtx [MEL_BANKS,1]
	#ifdef DEBUG
		arm_copy_f32 (var2,MelWin,MEL_BANKS);																											// Copio el espectro luego de aplicado el banco de filtros Mel
	#endif
	
	
			
	/* Se obtienen los valores logaritmicos de los coeficientes y le hago zero-padding */
	arm_fill_f32 	(0,var1,IFFT_LEN);
	for (i=0; i<MEL_BANKS; i++)
		var1[i*2+2] = log10f(var2[i]);
	#ifdef DEBUG
		arm_copy_f32 (var1,LogWin,IFFT_LEN);										// Copio la señal filtrada para cuestiones de DEBUG
	#endif
	
	
	
	/* Se Anti-transforma aplicando la DCT-II ==> hago la anti-transformada real */
	if(arm_rfft_fast_init_f32 (&DCTinst, IFFT_LEN) == ARM_MATH_ARGUMENT_ERROR)	Error();
	arm_rfft_fast_f32(&DCTinst,var1,var2,1);
	#ifdef DEBUG
		arm_copy_f32 (var2,CepWin,IFFT_LEN);										// Copio la señal filtrada para cuestiones de DEBUG
	#endif
	
	/* Se pasa la señal por un filtro en el campo Cepstral */
	arm_mult_f32 (var2,Lifter,MFCC,LIFTER_LEGNTH);

	return 1;
}
#endif
/**
  * @brief  Coefficients of the Hamming Window
	* @param  [in] a: Pointer to the matrix of one of the time series to compare
	* @param  [in] b: Pointer to the matrix of the other of the time series to compare
	* @param  [in] path: Pointer to the matrix where the path taken can be seen. Size of path must be length(a)+length(b). If it's not need pass path=NULL.
  * @retval 
  */
float32_t dtw (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path){
	/* 		PARAMETERS IN EACH MATRIX
	 *	Rows --> time movement
	 *	Cols --> Parameters
	 *
	 * b0 a0	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a1	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a2	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a3	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a4	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a5	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a6	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a7	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a8	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 * b0 a9	--> p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 ...
	 *
	 */
 /*			REPRESENTATION OF A POSIBLE PATH
	*
	* 		a0	a1	a2	a3	a4	a5	a6	a7	a8	a9 ...
	* b0
	* b1																		 _____|
	* b2																_____|		
	* b3													 _____|
	* b4											_____|
	* b5								 _____|
	* b6						_____|
	* b7			 _____|
	* b8	_____|
	* b9	|
	*/
	

	

	// Assert error
	if(a->numRows == b->numRows)
		Error_Handler();

	uint32_t i,j,idx;
	float32_t aux,past[3];
	float32_t cost;
	const uint16_t width=10;
	
	// Length of time series
	uint16_t costmtxcols = a->numCols;
	uint16_t costmtxrows = b->numCols;
	uint16_t params      = b->numRows;	// == a->numRows
	
	// Matrices used for calculation
	float32_t dtw_mtx[costmtxrows*costmtxcols];
	uint16_t	whole_path[costmtxrows*costmtxcols];
	
	dtw_mtx[0]=0;
	for(i=1;i<costmtxrows;i++)
		for(j=1;j<costmtxcols;j++)
			dtw_mtx[i*costmtxcols+j]=FLT_MAX;
	
	for(i=1;i<costmtxrows;i++){
		for(j=(uint32_t)fmax(0,i-width);j<(uint32_t)fmin(0,i-width);j++) {
			// Get distance
			cost = dist(&(a->pData[i*params]),&(b->pData[j*params]),params);
			
			// Calculate arrive to node through:
			past[LEFT] 	 = dtw_mtx[(i-1) * costmtxcols +   j  ];		// horizontal line
			past[MIDDLE] = dtw_mtx[(i-1) * costmtxcols + (j-1)];		// diagonal line
			past[BOTTOM] = dtw_mtx[  i   * costmtxcols + (j-1)];		// vertical line
			
			// Calculo el minimo de todos
			arm_min_f32 (past, sizeof(past), &aux, &idx);

			// Save path to that node
			if (path != NULL) {
				switch(idx){
					case LEFT:
						whole_path[i*costmtxcols + j] = (uint16_t) ((i-1)*costmtxcols + j);
						break;
					
					case MIDDLE:
						whole_path[i*costmtxcols + j] = (uint16_t) ((i-1)*costmtxcols + (j-1));
						break;
					
					case BOTTOM:
						whole_path[i*costmtxcols + j] = (uint16_t) (i*costmtxcols + (j-1));
						break;
					
					case DONTCARE:
						whole_path[i*costmtxcols + j] = (uint16_t) (i*costmtxcols + j);		// lo guardo con la posición propia
						break;
				}
			}
			
			// Calculo la distancia a ese punto
			dtw_mtx[i*costmtxcols + j] = cost + aux;
		}
	}		
	if(path!=NULL){
		path[0] = whole_path[costmtxrows*costmtxcols];
		for(i=0;i<costmtxrows+costmtxcols;i++)
			path[i] = whole_path[path[i]];
	}
	
	// Return distance value
	return dtw_mtx[costmtxrows*costmtxcols];
}
/**
  * @brief  Calcula la distancia entre dos elementos
  * @param	[in] points to the first input vector 
	* @param	[in] points to the second input vector 
	* @retval resultado
  */
float32_t dist (float32_t *pSrcA, float32_t *pSrcB, uint16_t blockSize){
	float32_t pDst[blockSize];
	float32_t pResult;
	
	// Calculo la distancia euclidiana
	arm_sub_f32 (pSrcA, pSrcB, pDst, blockSize);		// pDst[n] = pSrcA[n] - pSrcB[n]
	arm_rms_f32 (pDst, blockSize, &pResult);				// sqrt(pDst[1]^2 + pDst[2]^2 + ... + pDst[n]^2)
	
	return pResult;
}
/**
  * @brief  Calcula la derivada del vector. El vector destino debería tener tamaño blockSize-1
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void arm_diff_f32 (float32_t *pSrc, float32_t *pDst, uint32_t blockSize) {
	// pDst[n] = pSrc[n] - pSrc[n-1]
		arm_sub_f32 (pSrc, &pSrc[1], pDst, blockSize-1);
		arm_negate_f32 (pDst, pDst, blockSize-1);
}

/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void cumsum (float32_t *pSrc, float32_t *pDst, uint32_t blockSize){
	
	float32_t sum = 0.0f;                          /* accumulator */
  uint32_t blkCnt;                               /* loop counter */

  /*loop Unrolling */
  blkCnt = blockSize >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
   ** a second loop below computes the remaining 1 to 3 samples. */
  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* Compute the cumsum and then store the result in a temporary variable, sum. */
    sum += *pSrc++;
    sum += *pSrc++;
		sum += *pSrc++;
		sum += *pSrc++;
    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the blockSize is not a multiple of 4, compute any remaining output samples here.    
   ** No loop unrolling is used. */
  blkCnt = blockSize % 0x4u;

  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* compute the cumsum and then store the result in a temporary variable, sum. */
    sum += *pSrc++;

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* Store the result to the destination */
  *pDst = sum;
}
/**
  * @brief  Cum sum
  * @param	Puntero al vector de entrada
	* @param	Puntero al vector destino 
	* @param  Tamaño del vector de entrada
  */
void sumlog (float32_t *pSrc, float32_t *pDst, uint32_t blockSize){
	
	float32_t sum = 0.0f;                          /* accumulator */
  uint32_t blkCnt;                               /* loop counter */

  /*loop Unrolling */
  blkCnt = blockSize >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
   ** a second loop below computes the remaining 1 to 3 samples. */
  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* Compute the cumsum and then store the result in a temporary variable, sum. */
    sum += logf(*pSrc++);
    sum += logf(*pSrc++);
		sum += logf(*pSrc++);
		sum += logf(*pSrc++);
    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the blockSize is not a multiple of 4, compute any remaining output samples here.    
   ** No loop unrolling is used. */
  blkCnt = blockSize % 0x4u;

  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* compute the cumsum and then store the result in a temporary variable, sum. */
    sum += logf(*pSrc++);

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* Store the result to the destination */
  *pDst = sum;
}
//---------------------------------------
//				INTERRUPTION FUNCTIONS
//---------------------------------------
/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_HalfTransfer_CallBack(void) { 
	/* First Half of PDM Buffer */
	int i;
	
	for(i=0; i<EXTEND_BUFF; i++)		
		AUDIO_IN_PDM2PCM((uint16_t*)&PDMBuf[i*PDM_BUFF_SIZE],PDM_BUFF_SIZE,&PCM[i*PCM_BUFF_SIZE],AUDIO_IN_CHANNEL_NBR, AUDIO_IN_VOLUME, Filter);

	osMessagePut(CaptureState,BUFFER_READY,0);
	
}
/**
  * @brief  Manages the DMA Transfer Complete interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_TransferComplete_CallBack(void) {
	/* Second Half of PDM Buffer */
	int i;
	
	for(i=EXTEND_BUFF; i<EXTEND_BUFF*2; i++)
		AUDIO_IN_PDM2PCM((uint16_t*)&PDMBuf[i*PDM_BUFF_SIZE],PDM_BUFF_SIZE,&PCM[(i-EXTEND_BUFF)*PCM_BUFF_SIZE],AUDIO_IN_CHANNEL_NBR, AUDIO_IN_VOLUME, Filter);

	osMessagePut(CaptureState,BUFFER_READY,0);
	
}
/**
  * @brief  Manages the DMA Error interrupt.
  * @param  None
  * @retval None
  */
void AUDIO_IN_Error_CallBack	(void){
	
	Error_Handler();
	
}
/**
  * @brief  
  * @param  
  * @retval
  */
void User_Button_EXTI (void) {
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);			// Disable EXTI0 IRQ
	osMessagePut(KeyMsg,BUTTON_IRQ,0);
}
//---------------------------------------
//						WAVE FILE FUNCTIONS
//---------------------------------------
/**
  * @brief  Create a new Wave File
	* @param  WaveFormat: Pointer to a structure type WAVE_FormatTypeDef
  * @param  WavFile: 
	* @param	Filename:
  * @param  pHeader: Pointer to the Wave file header to be written.  
  * @retval pointer to the Filename string
  */
FRESULT New_Wave_File (char *Filename, WAVE_FormatTypeDef* WaveFormat, FIL *WavFile, uint8_t *pHeader,uint32_t *byteswritten) {
	
	FRESULT res;

	// Open File
	res = f_open(WavFile, Filename, FA_CREATE_NEW | FA_WRITE);
	while (res == FR_EXIST || res == FR_LOCKED ){
		updateFilename(Filename);		
		res = f_open(WavFile, Filename, FA_CREATE_NEW | FA_WRITE);
	}
	
	/* If Error */
	if(res != FR_OK)
		return res;
		
	/* Initialized Wave Header File */
	WaveProcess_EncInit(WaveFormat, pHeader);
	
	/* Write data to the file */
	res = f_write(WavFile, pHeader, sizeof(pHeader), (void *)byteswritten);
	
	/* Error when trying to write file*/
	if((byteswritten == 0) || (res != FR_OK))
		f_close(WavFile);
	
	return res;
}
/**
  * @brief  Encoder initialization.
	* @param  WaveFormat: Pointer to a structure type WAVE_FormatTypeDef
  * @param  Freq: Sampling frequency.
  * @param  pHeader: Pointer to the Wave file header to be written.  
  * @retval 0 if success, !0 else.
  */
uint32_t WaveProcess_EncInit(WAVE_FormatTypeDef* WaveFormat, uint8_t* pHeader) {  
	/* Initialize the encoder structure */
	WaveFormat->SampleRate = AUDIO_IN_FREQ;        					/* Audio sampling frequency */
	WaveFormat->NbrChannels = AUDIO_IN_CHANNEL_NBR;       	/* Number of channels: 1:Mono or 2:Stereo */
	WaveFormat->BitPerSample = AUDIO_IN_BIT_RESOLUTION;  	 	/* Number of bits per sample (16, 24 or 32) */
	WaveFormat->FileSize = 0x001D4C00;    									/* Total length of useful audio data (payload) */
	WaveFormat->SubChunk1Size = 44;       									/* The file header chunk size */
	WaveFormat->ByteRate = (WaveFormat->SampleRate * \
												(WaveFormat->BitPerSample/8) * \
												 WaveFormat->NbrChannels);        /* Number of bytes per second  (sample rate * block align)  */
	WaveFormat->BlockAlign = WaveFormat->NbrChannels * \
													(WaveFormat->BitPerSample/8);   /* channels * bits/sample / 8 */
	
	/* Parse the Wave file header and extract required information */
  if(WaveProcess_HeaderInit(pHeader, WaveFormat))
    return 1;
	
  return 0;
}

/**
  * @brief  Initialize the Wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the Wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
uint32_t WaveProcess_HeaderInit(uint8_t* pHeader, WAVE_FormatTypeDef* pWaveFormatStruct) {

/********* CHUNK DESCRIPTOR *********/	
	/* Write chunkID. Contains the letters "RIFF"	in ASCII form  ------------------------------------------*/
  pHeader[0] = 'R';
  pHeader[1] = 'I';
  pHeader[2] = 'F';
  pHeader[3] = 'F';

  /* Write the file length. This is the size of the entire file in bytes minus 8 bytes for the two
	fields not included in this count: ChunkID and ChunkSize. ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the recording opearation. 
	Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pHeader[4] = 0x00;
  pHeader[5] = 0x4C;
  pHeader[6] = 0x1D;
  pHeader[7] = 0x00;
	
  /* Write the file format, must be 'Wave'. Contains the letters "WaveE" -----------------------------------*/
  pHeader[8]  = 'W';
  pHeader[9]  = 'A';
  pHeader[10] = 'V';
  pHeader[11] = 'E';



/********* SUB-CHUNK DESCRIPTOR N°1 *********/	
  /* Write the format chunk, must be'fmt ' -----------------------------------*/
  pHeader[12]  = 'f';
  pHeader[13]  = 'm';
  pHeader[14]  = 't';
  pHeader[15]  = ' ';

  /* Write the length of the 'fmt' data (16 for PCM).
	This is the size of the rest of the Subchunk which follows this number. ------------------------*/
  pHeader[16]  = 0x10;
  pHeader[17]  = 0x00;
  pHeader[18]  = 0x00;
  pHeader[19]  = 0x00;

  /* Write the audio format. PCM = 1 ==> Linear quantization
	Values other than 1 indicate some form of compression. ------------------------------*/
  pHeader[20]  = 0x01;
  pHeader[21]  = 0x00;

  /* Write the number of channels (Mono = 1, Stereo = 2). ---------------------------*/
  pHeader[22]  = pWaveFormatStruct->NbrChannels;
  pHeader[23]  = 0x00;

  /* Write the Sample Rate in Hz ---------------------------------------------*/
  /* Write Little Endian ie. 8000 = 0x00001F40 => byte[24]=0x40, byte[27]=0x00*/
  pHeader[24]  = (uint8_t)((pWaveFormatStruct->SampleRate & 0xFF));
  pHeader[25]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 8) & 0xFF);
  pHeader[26]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 16) & 0xFF);
  pHeader[27]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 24) & 0xFF);

  /* Write the Byte Rate
	==> SampleRate * NumChannels * BitsPerSample/8	-----------------------------------------------------*/
  pHeader[28]  = (uint8_t)((pWaveFormatStruct->ByteRate & 0xFF));
  pHeader[29]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 8) & 0xFF);
  pHeader[30]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 16) & 0xFF);
  pHeader[31]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 24) & 0xFF);

  /* Write the block alignment 
	==> NumChannels * BitsPerSample/8 -----------------------------------------------*/
  pHeader[32]  = pWaveFormatStruct->BlockAlign;
  pHeader[33]  = 0x00;

  /* Write the number of bits per sample -------------------------------------*/
  pHeader[34]  = pWaveFormatStruct->BitPerSample;
  pHeader[35]  = 0x00;



/********* SUB-CHUNK DESCRIPTOR N°2 *********/	
  /* Write the Data chunk. Contains the letters "data" ------------------------------------*/
  pHeader[36]  = 'd';
  pHeader[37]  = 'a';
  pHeader[38]  = 't';
  pHeader[39]  = 'a';

  /* Write the number of sample data. This is the number of bytes in the data.
	==> NumSamples * NumChannels * BitsPerSample/8  -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pHeader[40]  = 0x00;
  pHeader[41]  = 0x4C;
  pHeader[42]  = 0x1D;
  pHeader[43]  = 0x00;
  
  /* Return 0 if all operations are OK */
  return 0;
}

/**
  * @brief  Initialize the Wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the Wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
uint32_t WaveProcess_HeaderUpdate(uint8_t* pHeader, WAVE_FormatTypeDef* pWaveFormatStruct, uint32_t filesize) {
  /* Write the file length ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the 
   recording opearation.  Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pHeader[4] = (uint8_t)(filesize);
  pHeader[5] = (uint8_t)(filesize >> 8);
  pHeader[6] = (uint8_t)(filesize >> 16);
  pHeader[7] = (uint8_t)(filesize >> 24);
  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  filesize -=44;
  pHeader[40] = (uint8_t)(filesize); 
  pHeader[41] = (uint8_t)(filesize >> 8);
  pHeader[42] = (uint8_t)(filesize >> 16);
  pHeader[43] = (uint8_t)(filesize >> 24); 
  /* Return 0 if all operations are OK */
  return 0;
}
/**
	* @brief
	* @param
	* @param
	*/
char *updateFilename (char *Filename) {
	uint32_t indx,start;

	// Busco el inicio del número
	for(indx=0; Filename[indx]<'0' || Filename[indx]>'9'; indx++)	{}
	start = indx;
	
	// Busco el final del número
	for(; Filename[indx]>='0' && Filename[indx]<='9'; indx++)	{}
	
	// Hago un update del número
	while(--indx>=start)
	{
		if(++Filename[indx] >'9')
			Filename[indx] = '0';
		else
			break;
	}
	return Filename;
}
/**
  * @brief  Error
  * @retval 
  */
void Error (void) {
	while(1) {
		LED_Toggle(RLED);
		LED_Toggle(BLED);
		HAL_Delay(100);
	}
}




