///**
//  ******************************************************************************
//  * @file    audio_processing.c
//  * @author  Alejandro Alvarez
//  * @version V1.0.0
//  * @date    28-Enero-2014
//  * @brief   Audio Functions for ASR_DTW
//  ******************************************************************************
//  * @attention
//  *
//  ******************************************************************************
//	*/

///* Includes ------------------------------------------------------------------*/
//#include "audio_processing.h"
//#include "misc.h"
//#include "error_handler.h"
//#include "ale_dct2_f32.h"


//osMessageQId audio_processing_msg;

////---------------------------------------
////				CONFIGURATION VARIALBES
////---------------------------------------
//	ProcConf proc_conf;				// Processing configurartion
//	CalibConf calib_conf;			// Calibration configurartion
//	bool use_vad = true;
//	Proc_var *vars_buffers = NULL;
//	uint32_t frame_num = 0;
////---------------------------------------
////				SPEECH PROCESSING VARIALBES
////---------------------------------------

//	uint16_t  *new_frame = NULL;					// [proc_conf.frame_net];
//	float32_t *pState = NULL;							// [proc_conf.numtaps + proc_conf.frame_net - 1]
//	float32_t *Pre_enfasis_Coeef = NULL;	// [proc_conf.numtaps]

//	float32_t *frame_block = NULL;				// [ proc_conf.frame_len ];
//	float32_t *aux1 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
//	float32_t *aux2 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
//	float32_t *MFCC_buff = NULL;					// [proc_conf.lifterlength];
//	
//	
//	float32_t *HamWin = NULL; 						// [proc_conf.frame_len];
//	float32_t *CepWeight = NULL; 					// [LIFTER_LEGNTH];
//	extern float32_t MelBank [256*20];
//	extern arm_matrix_instance_f32	MelFilt;
//	
//	arm_matrix_instance_f32 				MagFFTMtx, MelWinMtx;
//	arm_fir_instance_f32 						FirInst;	
//	arm_rfft_fast_instance_f32 			RFFTinst, DCTinst_rfft;
//	ale_dct2_instance_f32						DCTinst;

////---------------------------------------
////							VAD VARIALBES
////---------------------------------------
//	__align(8)
//		float32_t GM,AM,aux;	// Gemotric Mean & Arithmetic Mean

//		float32_t Energy;
//		uint32_t  Frecmax;
//		float32_t SpFlat;
//	//	float32_t SilZeroCross;

////---------------------------------------
////				CALIBRATION VARIABLES
////---------------------------------------
//		float32_t *SilEnergy  = NULL;
//		uint32_t  *SilFrecmax = NULL;
//		float32_t *SilSpFlat  = NULL;
//		
//		float32_t SilEnergyMean;
//		uint32_t	SilFmaxMean;
//		float32_t SilSpFlatMean;
//	//	float32_t SilZeroCrossMean;

//		float32_t SilEnergyDev;
//		uint32_t	SilFmaxDev;
//		float32_t SilSpFlatDev;
//	//	float32_t SilZeroCrossDev;

//		float32_t THD_E;
//		uint32_t	THD_FMX;
//		float32_t THD_SF;

//	

////
////--------	PROCESSING FUNCTIONS --------
////
//void initProcessing (float32_t **MFCC, uint32_t *MFCC_size, ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers){
//	
//	initBasics (configuration, vad, ptr_vars_buffers);
//	
//	// Get memory for variables
//	if( (MFCC_buff = pvPortMalloc(proc_conf.lifter_legnth * sizeof(*MFCC_buff))) == NULL)
//		Error_Handler();
//	
//	// Copy it to variables
//	*MFCC = MFCC_buff;
//	*MFCC_size = proc_conf.lifter_legnth * sizeof(*MFCC_buff);
//	
//	// Creo el Lifter (filtro de Cepstrum)
//	if( (CepWeight = pvPortMalloc(proc_conf.lifter_legnth * sizeof(*CepWeight))) == NULL)
//		Error_Handler();
//	Lifter_float (CepWeight,proc_conf.lifter_legnth);

//	// Instance DCT
//	if(dct2_init_f32(&DCTinst, &DCTinst_rfft, proc_conf.dct_len, proc_conf.dct_len/2, sqrt(2.0f/proc_conf.dct_len)) == ARM_MATH_ARGUMENT_ERROR)
//		Error_Handler();
//}
//void finishProcessing(void){
//	// Termino
//	finishBasics();
//	
//	// Libero memoria
//	vPortFree(MFCC_buff);							MFCC_buff = NULL;
//	vPortFree(CepWeight);							CepWeight = NULL;
//}
//ProcStatus MFCC_float (uint16_t *frame, ProcStages *proc_stages, bool last_frame) {
//	
//	// Inicializo variables
//	ProcStatus output = NO_VOICE;
//	*proc_stages = No_Stage;
//	
//	// Copio el puntero del nuevo frame al que usa audio_processing
//	new_frame = frame;
//	
//	if (!last_frame)
//	{
//		// Ejecuto la primera parte del procesamiento
//		firstProcStage (vars_buffers);
//		frame_num++;
//		*proc_stages |= First_Stage;
//	}
//	else
//	{
//		if(vars_buffers != NULL)
//			arm_fill_f32 (0, vars_buffers->FltSig, proc_conf.frame_net);
//		else
//			arm_fill_f32 (0, aux2, proc_conf.frame_net);
//	}
//	
//	if (frame_num >1)
//	{
//		secondProcStage (use_vad, vars_buffers);
//		*proc_stages |= Second_Stage;
//		
//		/* Check if it is a Voiced Frame */
//		if( !use_vad || (Energy > THD_E   &&  Frecmax < THD_FMX) || SpFlat > THD_SF)
//		{
//			thirdProcStage (MFCC_buff, vars_buffers);
//			output = VOICE;
//			*proc_stages |= Third_Stage;
//		}
//		
//		if(vars_buffers != NULL)
//		{
//			if(output == NO_VOICE)
//			{
//				memset(vars_buffers->MelWin, 0, proc_conf.mel_banks * sizeof(*(vars_buffers->MelWin)));
//				memset(vars_buffers->LogWin, 0, proc_conf.dct_len  * sizeof(*(vars_buffers->LogWin)));
//				memset(vars_buffers->CepWin, 0, proc_conf.dct_len  * sizeof(*(vars_buffers->CepWin)));
//				vars_buffers->VAD = 0;
//			}
//			else
//				vars_buffers->VAD = 1;
//		}
//	}

//	
//	return output;
//}

//void audioProc (void const *pvParameters) {
//	
//	// Message variables
//	Audio_Proc_args *args;
//	osEvent event;
//	
//	// Task variables
//	UINT byteswritten;
//	FIL MFCCFile;
//	bool finish = false;
//	uint32_t frameNum=0;
//	float32_t *MFCC;
//	uint32_t MFCC_size;
//	ProcStages stages;
//	
//	// Create Process Task State MessageQ
//	osMessageQDef(audio_processing_msg,10,uint32_t);
//	audio_processing_msg = osMessageCreate(osMessageQ(audio_processing_msg),NULL);	
//	
//	// Get arguments
//	args = (Audio_Proc_args*) pvParameters;

//	// Init processing
//	initProcessing(&MFCC, &MFCC_size, args->proc_conf, args->vad, NULL);

//	//---------------------------- START TASK ----------------------------
//	for(;;)
//	{
//		event = osMessageGet(audio_processing_msg,osWaitForever);
//		if(event.status == osEventMessage)
//		{
//			switch (event.value.v)
//			{
//				case FINISH:
//				{
//					// Cierro los archivos
//					f_close(&MFCCFile);

//					// De-Inicializo el proceso
//					finishProcessing();
//					
//					// Send message to parent task telling it's finish
//					osMessagePut(args->src_msg_id,FINISH_PROCESSING,0);

//					// Turn off Processing LED
//					LED_Off(BLED);
//					
//					// Destroy Message Que
//					audio_processing_msg = NULL;
//					
//					// Elimino la tarea
//					osThreadTerminate(osThreadGetId());
//				}
//				case NEW_FRAME:
//				{
//					// while (read audio from buffer)
//					//{
//						// Process signal
////							firstProcStage 	(filt_signal, audio, Proc_var *saving_var);
//						// write filt_signal in buffer
//						
//						// Check if read filt_buff is posible
////						while (read(filt_buff))
////						{
//								// Second stage processing
//								// VAD processing
//								// Third stage processing
//						
//								// write output in mfcc_buff (energy also)
////						}
//						
//						// Check if read mfcc_buff is posible
////						while (read(mfcc_buff))
////						{
//								// write output in delta_buff
////						}

//						// Check if read delta_buff is posible
////						while (read(delta_buff))
////						{
//								// package mfcc, delta & delta2 and save in a buff
////						}
////							}
//					
//					break;
//				}
//				default:
//					break;
//			}
//		}
//	}
//}

////
////-------- CALIBRATION FUNCTIONS --------
////
//CalibStatus initCalibration	(uint32_t *num_frames,CalibConf *calib_config, ProcConf *configuration, Proc_var *ptr_vars_buffers){
//	// Initialized basic configuration
//	initBasics (configuration, true, ptr_vars_buffers);
//	
//	// Copy Calibration configuration
//	calib_conf = *calib_config;
//	
//	// Allocate space for variables
//	SilEnergy  = pvPortMalloc(calib_conf.calib_len * sizeof(*SilEnergy));
//	SilFrecmax = pvPortMalloc(calib_conf.calib_len * sizeof(*SilFrecmax));
//	SilSpFlat  = pvPortMalloc(calib_conf.calib_len * sizeof(*SilSpFlat));
//	if(SilEnergy == NULL || SilFrecmax == NULL || SilSpFlat == NULL )
//		Error_Handler();
//	
//	// Initialize frame count
//	*num_frames = calib_conf.calib_len;
//	
//	return CALIB_INITIATED;
//}
//CalibStatus Calibrate (uint16_t *frame, uint32_t frame_num, ProcStages *stages) {
//	//TODO: ARREGLAR CALIBRATION QUE ESTA MAL
//	if ( frame_num < calib_conf.calib_len)
//	{
//		// Copio el puntero del nuevo frame al que usa audio_processing
//		new_frame = frame;
//	
//		// Ejecuto la primera parte del procesamiento
//		firstProcStage (vars_buffers);
//		frame_num++;
//		*stages = First_Stage;
//		
//		if (frame_num >1)
//		{
//			secondProcStage (true, vars_buffers);
//			*stages |= Second_Stage;
//			
//			/* Check if it is a Voiced Frame */
//			SilEnergy[frame_num] = Energy;
//			SilFrecmax[frame_num] = Frecmax;
//			SilSpFlat[frame_num] = SpFlat;
//		}
//	}
//	
//	if ( frame_num < calib_conf.calib_len-1)
//		return CALIB_IN_PROCESS;
//	else
//		return CALIB_FINISH;
//	
//}
//CalibStatus endCalibration	(const bool save_calib_vars) {

//	// Calculate Mean of Features
//	arm_mean_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyMean);
//	arm_mean_q31 ((q31_t*)SilFrecmax, calib_conf.calib_len,(q31_t*) &SilFmaxMean);
//	arm_mean_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatMean);

//	// Calculate Deviation of Features
//	arm_std_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyDev);
//	arm_std_q31 ((q31_t*)SilFrecmax, calib_conf.calib_len,(q31_t*) &SilFmaxDev);
//	arm_std_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatDev);

//	// Free memory
//	vPortFree(SilEnergy);		SilEnergy = NULL;
//	vPortFree(SilFrecmax);	SilFrecmax = NULL;
//	vPortFree(SilSpFlat);		SilSpFlat = NULL;
//	
//	// Set Thresholds
//	THD_E   = SilEnergyMean + calib_conf.thd_scl_eng * SilEnergyDev;
//	THD_FMX = SilFmaxMean > calib_conf.thd_min_fmax ? SilFmaxMean : calib_conf.thd_min_fmax;
//	THD_SF  = fabs(SilSpFlatMean) + calib_conf.thd_scl_sf * SilSpFlatDev;

//	if(save_calib_vars)
//	{
//		FIL CalibFile;
//		uint32_t byteswritten;     							/* File write/read counts */	
//		
//		// Guardo la Energ�a
//		if(f_open(&CalibFile, "CLB_ENR.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilEnergyMean,sizeof(SilEnergyMean),(void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilEnergyDev,	sizeof(SilEnergyDev), (void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &THD_E, 				sizeof(THD_E), 				(void*)&byteswritten) != FR_OK) Error_Handler();
//		f_close(&CalibFile);
//			
//		// Guardo la Frecuencia M�xima
//		if(f_open(&CalibFile, "CLB_FMX.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilFmaxMean,sizeof(SilFmaxMean),(void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilFmaxDev, sizeof(SilFmaxDev),	(void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &THD_FMX,		sizeof(THD_FMX),		(void*)&byteswritten) != FR_OK) Error_Handler();
//		f_close(&CalibFile);
//		
//				// Guardo el Spectral Flatness
//		if(f_open(&CalibFile, "CLB_SF.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilSpFlatMean,sizeof(SilSpFlatMean),(void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &SilSpFlatDev,	sizeof(SilSpFlatDev),	(void*)&byteswritten) != FR_OK) Error_Handler();
//		if(f_write(&CalibFile, &THD_SF, 			sizeof(THD_SF),				(void*)&byteswritten) != FR_OK) Error_Handler();
//		f_close(&CalibFile);
//	}
//	
//	// Finish basics
//	finishBasics();	
//	
//	return CALIB_OK;
//}
////
////-----	INIT PROCESSING FUNCTIONS	-------
////
//void initBasics (ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers) {
//	
//	// Copio la configuraci�n de procesamiento
//	proc_conf = *configuration;
//	use_vad = vad;
//	frame_num = 0;
//	
//	// Armo el filtro de Pre�nfasis
//	// Los coeficientes tienen que estar almacenados en tiempo invertido (leer documentaci�n)
//	if( (Pre_enfasis_Coeef = pvPortMalloc( proc_conf.numtaps * sizeof(*Pre_enfasis_Coeef) ) ) == NULL )
//		Error_Handler();
//	Pre_enfasis_Coeef[0] = -proc_conf.alpha;
//	Pre_enfasis_Coeef[1]= 1;
//	
//	// Alloco memoria para el filtro FIR
//	if( (pState = pvPortMalloc( (proc_conf.numtaps + proc_conf.frame_net - 1) * sizeof(*pState) ) ) == NULL )
//		Error_Handler();
//	arm_fir_init_f32 (&FirInst, proc_conf.numtaps, Pre_enfasis_Coeef, pState, proc_conf.frame_net);
//	
//	// Alloco memoria para la variable frame_block y la inicializo en cero
//	if( (frame_block = pvPortMalloc(proc_conf.frame_len*sizeof(*frame_block))) == NULL)
//		Error_Handler();
//	memset(frame_block, 0, proc_conf.frame_len*sizeof(*frame_block));		// Inicializo en 0

//	// Creo la ventana de Hamming
//	if( (HamWin = pvPortMalloc(proc_conf.frame_len* sizeof(*HamWin))) == NULL)
//		Error_Handler();
//	Hamming_float(HamWin,proc_conf.frame_len);
//	
//	// Instance FFT
//	if(arm_rfft_fast_init_f32 (&RFFTinst, proc_conf.fft_len) == ARM_MATH_ARGUMENT_ERROR)
//		Error_Handler();
//	
//	// Alloco memoria para aux1 y aux2 que las uso como auxiliares
//	if( (aux1 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*aux1) ) ) == NULL )
//		Error_Handler();
//	if( (aux2 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*aux2) ) ) == NULL )
//			Error_Handler();
//	
//	// Allocate variables
//	if(ptr_vars_buffers != NULL)
//	{
//		allocateProcVariables (ptr_vars_buffers);
//		vars_buffers = ptr_vars_buffers;
//	}
//}
//void finishBasics	(void) {
//	// Libero memoria
//	vPortFree(Pre_enfasis_Coeef);			Pre_enfasis_Coeef = NULL;
//	vPortFree(pState);								pState = NULL;
//	vPortFree(frame_block);						frame_block = NULL;
//	vPortFree(HamWin);								HamWin = NULL;
//	vPortFree(aux1);									aux1 = NULL;
//	vPortFree(aux2);									aux2 = NULL;
//	
//	if(vars_buffers != NULL)
//	{
//		freeProcVariables(vars_buffers);
//		vars_buffers = NULL;
//	}
//}
//void Hamming_float (float32_t *Hamming, uint32_t length) {
//	uint32_t i;
//	const float32_t a=0.54 , b=0.46;
//	
//	for(i=0; i < length; i++) {
//		Hamming[i] = a - b * arm_cos_f32 ((float32_t)2*PI*i/length);
//	}
//}
//void Lifter_float (float32_t *Lifter, uint32_t length) {
//	
//	float32_t theta;
//	uint32_t n;
//	
//	for(n=0; n<length; n++) {
//		theta = PI*n/(length-1);
//		Lifter[n] = 1 + arm_sin_f32(theta) * length/2;
//	}
//}
////
////-----	PROCESSING FUNCTIONS	-------
////
//void firstProcStage 	(float32_t *filt_signal, uint16_t *audio, Proc_var *saving_var) {
//	
//	/* Convierto a Float y escalo */
//	arm_q15_to_float((q15_t*)audio, aux1, proc_conf.frame_net);

//	/* Se aplica un filtro de Pre-�nfais al segmento de se�al obtenida */	
//	arm_fir_f32 (&FirInst, aux1, filt_signal, proc_conf.frame_net);

//	// Copio las variables de ser necesario
//	if(saving_var != NULL)
//	{
//		arm_copy_f32 (aux1, saving_var->speech, proc_conf.frame_net);
//		arm_copy_f32 (filt_signal, saving_var->FltSig, proc_conf.frame_net);
//	}
//}

//void secondProcStage	(float32_t *MagFFT, float32_t *frame_block, Proc_var *saving_var) {
//	
//	/* Se le aplica la ventana de Hamming al segmento obtenido */
//	arm_mult_f32 (frame_block, HamWin, aux1, proc_conf.frame_len);
//	
//	// Copio las variables de ser necesario
//	if(saving_var != NULL)
//		arm_copy_f32 (aux1, saving_var->WinSig, proc_conf.frame_len);
//		
//	/* Calculo la energ�a */
//	arm_power_f32 (aux1, proc_conf.frame_len, &Energy);
//	Energy /= proc_conf.frame_len;
//	
//	/* Se calcula la STFT */
//	arm_rfft_fast_f32(&RFFTinst,aux1,aux2,0);
//	aux2[0] += aux2[1];				//Sumo las componentes reales

//	/* Calculo el m�dulo de la FFT */
//	arm_cmplx_mag_squared_f32  (aux2, MagFFT, proc_conf.fft_len/2);

//	// Copio las variables de ser necesario
//	if(saving_var != NULL)
//	{
//		arm_copy_f32 (aux2, saving_var->STFTWin, proc_conf.frame_len);
//		arm_copy_f32 (MagFFT, saving_var->MagFFT,  proc_conf.fft_len/2);
//	}
//}
//void thirdProcStage 	(float32_t *MFCC, float32_t *MagFFT, Proc_var *saving_var) {
//	
//	/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
//	arm_mat_init_f32 (&MagFFTMtx, proc_conf.fft_len/2, 1, MagFFT);																				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
//	arm_mat_init_f32 (&MelWinMtx, proc_conf.mel_banks, 1, aux1);																				// Se crea una matriz para almacenar el resultado
//	if(arm_mat_mult_f32(&MelFilt, &MagFFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)
//		Error_Handler();
//	// MelFilt[MEL_BANKS,proc_conf.fft_len] * MagFFTMtx[proc_conf.fft_len,1] = MelWinMtx [MEL_BANKS,1]
//	
//	/* Se obtienen los valores logaritmicos de los coeficientes */
//	arm_fill_f32 	(0, aux2, proc_conf.dct_len);
//	for (uint32_t i=0; i<proc_conf.mel_banks; i++)
//		aux2[i] = log10f(aux1[i]);
//	
//	// Copio las variables
//	if(saving_var != NULL)
//	{
//		arm_copy_f32 (aux1, saving_var->MelWin, proc_conf.mel_banks);
//		arm_copy_f32 (aux2, saving_var->LogWin, proc_conf.dct_len);
//	}
//	
//	/* Se Anti-transforma aplicando la DCT-II */
//	ale_dct2_f32(&DCTinst, aux1, aux2);
//	
//	/* Se pasa la se�al por un filtro en el campo Cepstral */
//	arm_mult_f32 (aux2, CepWeight, MFCC, proc_conf.lifter_legnth);

//	// Copio las variables
//	if(saving_var != NULL)
//	{
//		arm_copy_f32 (aux2, saving_var->CepWin, proc_conf.dct_len/2);
//		arm_copy_f32 (MFCC, saving_var->CepWin, proc_conf.lifter_legnth);
//	}
//	
//}
//void VADFeatures			(VAD_var *vad, float32_t *MagFFT, float32_t Energy) {
//	uint32_t Index;
//	float32_t AM,GM;
//	
//	/* Obtengo la Fmax */
//	arm_max_f32 (MagFFT, proc_conf.fft_len/2, &aux, &Index);									// no me interesa el valor m�ximo, sino en que lugar se encuentra
//	Frecmax = Index;
//	
//	/* Calculo el Spectral Flatness */
//	arm_mean_f32 (MagFFT, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
//	sumlog(MagFFT, &GM, proc_conf.fft_len/2);
//	GM = expf(GM/(proc_conf.fft_len/2));													// Geometric Mean
//	SpFlat = 10*log10f(GM/AM);
//	
//	vad->energy = Energy;
//	vad->fmax = Frecmax;
//	vad->energy = SpFlat;
//}
////
////----- MISCELLANEOUS FUNCTIONS -----
////
//void allocateProcVariables (Proc_var *var) {
//	var->speech		= pvPortMalloc(proc_conf.frame_net * sizeof(*(var->speech)));		// Se�al de audio escalada
//	var->FltSig		= pvPortMalloc(proc_conf.frame_net * sizeof(*(var->FltSig)));		// Se�al de audio pasada por el Filtro de Pre-Enfasis
//	
//	var->WinSig		= pvPortMalloc(proc_conf.frame_len * sizeof(*(var->WinSig)));		// Se�al de filtrada multiplicada por la ventana de Hamming
//	
//	var->STFTWin	= pvPortMalloc(proc_conf.fft_len   * sizeof(*(var->STFTWin)));	// Transformada de Fourier en Tiempo Corto
//	var->MagFFT		= pvPortMalloc(proc_conf.fft_len/2 * sizeof(*(var->MagFFT)));		// M�dulo del espectro
//	
//	var->MelWin		= pvPortMalloc(proc_conf.mel_banks * sizeof(*(var->MelWin)));		// Espectro pasado por los filtros de Mel
//	
//	var->LogWin		= pvPortMalloc(proc_conf.dct_len  * sizeof(*(var->LogWin)));		// Logaritmo del espectro filtrado
//	var->CepWin		= pvPortMalloc(proc_conf.dct_len/2* sizeof(*(var->CepWin)));		// Se�al cepstral
//	
//	if(var->speech == NULL || var->FltSig == NULL || var->WinSig == NULL || var->STFTWin == NULL || 
//		 var->MagFFT == NULL || var->MelWin == NULL || var->LogWin == NULL || var->CepWin == NULL)
//		Error_Handler();
//}
//void freeProcVariables (Proc_var *var) {
//	vPortFree(var->speech);				var->speech = NULL;
//	vPortFree(var->FltSig);				var->FltSig = NULL;
//	vPortFree(var->WinSig);				var->WinSig = NULL;
//	vPortFree(var->STFTWin);			var->STFTWin = NULL;
//	vPortFree(var->MagFFT);				var->MagFFT = NULL;
//	vPortFree(var->MelWin);				var->MelWin = NULL;
//	vPortFree(var->LogWin);				var->LogWin = NULL;
//	vPortFree(var->CepWin);				var->CepWin = NULL;
//}
//uint8_t Open_proc_files (Proc_files *files, const bool vad) {
//	
//	UINT byteswritten;
//	
//	// Escribo un archivo con los coeficientes de la ventana de Hamming			
//	if(f_open(&files->HamWinFile, "HamWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)Error_Handler();
//	if(f_write(&files->HamWinFile, HamWin, proc_conf.frame_len * sizeof(*HamWin), &byteswritten) != FR_OK)	Error_Handler();
//	f_close(&files->HamWinFile);

//	// Escribo un archivo con los coeficientes del Filtro Cepstral
//	if(f_open(&files->CepWeiFile, "CepWei.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_write(&files->CepWeiFile, CepWeight, proc_conf.lifter_legnth * sizeof(*CepWeight), &byteswritten) != FR_OK)	Error_Handler();
//	f_close(&files->CepWeiFile);
//					
//	//Abro los archivos
//	if(f_open(&files->SpeechFile, "Speech.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->FltSigFile, "FltSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->FrameFile, 	"Frames.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->WinSigFile, "WinSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->STFTWinFile,"FFTWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->MagFFTFile, "MagFFT.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->MelWinFile, "MelWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->LogWinFile, "LogWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	if(f_open(&files->CepWinFile, "CepWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

//	if (vad)
//	{
//		if(f_open(&files->VADFile,			 "VAD.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//		if(f_open(&files->EnerFile,		"Energy.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//		if(f_open(&files->FrecFile,  "FrecMax.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//		if(f_open(&files->SFFile,		"SpecFlat.bin",	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
//	}

////	if(f_open(&files->nosirve, "nosirve.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

//	return 0;
//}
//uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad, ProcStages stage) {
//	UINT byteswritten;
//	
//	//Escribo los valores intermedios en un archivo
//	
//	if (stage & First_Stage)
//	{
//		if(f_write(&files->SpeechFile,  data->speech, proc_conf.frame_net * sizeof(*(data->speech)),  &byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->FltSigFile,  data->FltSig, proc_conf.frame_net * sizeof(*(data->FltSig)),  &byteswritten) != FR_OK)	Error_Handler();
//	}
//	
//	if(stage & Second_Stage)
//	{
//		if(f_write(&files->FrameFile,  	frame_block,  proc_conf.frame_len * sizeof(*frame_block),  		&byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->WinSigFile,  data->WinSig, proc_conf.frame_len * sizeof(*(data->WinSig)),  &byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->STFTWinFile, data->STFTWin,proc_conf.fft_len		* sizeof(*(data->STFTWin)), &byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->MagFFTFile,  data->MagFFT, proc_conf.fft_len/2 * sizeof(*(data->MagFFT)),  &byteswritten) != FR_OK)	Error_Handler();
//	}
//	
//	if(stage & Third_Stage)
//	{
//		if(f_write(&files->MelWinFile,  data->MelWin, proc_conf.mel_banks * sizeof(*(data->MelWin)),  &byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->LogWinFile,  data->LogWin, proc_conf.dct_len   *	sizeof(*(data->LogWin)),  &byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->CepWinFile,  data->CepWin, proc_conf.dct_len/2 * sizeof(*(data->CepWin)),  &byteswritten) != FR_OK)	Error_Handler();
//	}
//	
//	if(vad)
//	{
//		if(f_write(&files->VADFile,	&data->VAD,	sizeof(data->VAD),&byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->EnerFile,&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->FrecFile,&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler();
//		if(f_write(&files->SFFile,	&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler();
//	}
//	return 1;
//}
//uint8_t Close_proc_files (Proc_files *files, const bool vad) {
//		
//	f_close(&files->SpeechFile);
//	f_close(&files->FltSigFile);
//	f_close(&files->FrameFile);
//	f_close(&files->WinSigFile);
//	f_close(&files->STFTWinFile);
//	f_close(&files->MagFFTFile);
//	f_close(&files->MelWinFile);
//	f_close(&files->LogWinFile);
//	f_close(&files->CepWinFile);

//	if(vad)
//	{
//		f_close(&files->VADFile);
//		f_close(&files->EnerFile);
//		f_close(&files->FrecFile);
//		f_close(&files->SFFile);
//	}
//	
////	f_close(&files->nosirve);
//	return 1;
//}



////	ale_dct2_instance_f32 			dct2_instance;
////	arm_rfft_fast_instance_f32 	rfft_instance;
////	float32_t pState[64];
////
//// Los valores fueron obtenidos mediante la equaci�n x = 50*cos((1:32)*2*pi/40);
////
////	float32_t pInlineBuffer[32]={		49.384417029756889f,		47.552825814757675f,		44.550326209418394f,		40.450849718747371f,
////		35.355339059327378f,		29.389262614623657f,		22.699524986977345f,		15.450849718747373f,		7.821723252011546f,
////		0.000000000000003f,		-7.821723252011529f,		-15.450849718747367f,		-22.699524986977337f,		-29.389262614623650f,
////		-35.355339059327370f,		-40.450849718747364f,		-44.550326209418387f,		-47.552825814757675f,		-49.384417029756882f,
////		-50.000000000000000f,		-49.384417029756889f,		-47.552825814757689f,		-44.550326209418394f,		-40.450849718747371f,
////		-35.355339059327385f,		-29.389262614623661f,		-22.699524986977345f,		-15.450849718747378f,		-7.821723252011552f,
////		-0.000000000000009f,		7.821723252011534f,		15.450849718747362f,
////	};
////	
////	dct2_init_f32(&dct2_instance, &rfft_instance, 32, 16, sqrt(2.0f/32.0f));
////	ale_dct2_f32(&dct2_instance, pState, pInlineBuffer);





//		// Shifteo la ventana FRAME_OVERLAP veces
//		// Para ello, shifteo lo viejo y copio el nuevo frame al final
////		memcpy(&frame_block[proc_conf.zero_padding_left], &frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap], (proc_conf.frame_net + proc_conf.frame_overlap) * sizeof(*frame_block));
////		memcpy(&frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap + proc_conf.frame_net], saving_var->FltSig, proc_conf.frame_net * sizeof(*saving_var->FltSig));
//// Shifteo la ventana FRAME_OVERLAP veces
//		// Para ello, shifteo lo viejo y copio el nuevo frame al final
////		memcpy(&frame_block[proc_conf.zero_padding_left], &frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap], (proc_conf.frame_net + proc_conf.frame_overlap) * sizeof(*frame_block));
////		memcpy(&frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap + proc_conf.frame_net], aux2, proc_conf.frame_net * sizeof(*aux2));
