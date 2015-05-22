/**
  ******************************************************************************
  * @file    audio_processing.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/

/* Includes ------------------------------------------------------------------*/
#include "audio_processing.h"
#include "misc.h"
#include "error_handler.h"


//---------------------------------------
//				CONFIGURATION VARIALBES
//---------------------------------------
	ProcConf proc_conf;				// Processing configurartion
	CalibConf calib_conf;			// Calibration configurartion
	bool use_vad = true;
	Proc_var *vars_buffers = NULL;
	uint32_t frame_num = 0;
//---------------------------------------
//				SPEECH PROCESSING VARIALBES
//---------------------------------------

	uint16_t  *new_frame = NULL;					// [proc_conf.frame_net];
	float32_t *pState = NULL;							// [proc_conf.numtaps + proc_conf.frame_net - 1]
	float32_t *Pre_enfasis_Coeef = NULL;	// [proc_conf.numtaps]

	float32_t *frame_block = NULL;				// [ proc_conf.frame_len ];
	float32_t *var1 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
	float32_t *var2 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
	float32_t *MFCC_buff = NULL;					// [proc_conf.lifterlength];
	
	
	float32_t *HamWin = NULL; 						// [proc_conf.frame_len];
	float32_t *CepWeight = NULL; 					// [LIFTER_LEGNTH];
	extern float32_t MelBank [256*20];
	extern arm_matrix_instance_f32	MelFilt;
	
	arm_matrix_instance_f32 				MagFFTMtx, MelWinMtx;
	arm_fir_instance_f32 						FirInst;	
	arm_rfft_fast_instance_f32 			RFFTinst, DCTinst;

//---------------------------------------
//							VAD VARIALBES
//---------------------------------------
	__align(8)
		float32_t GM,AM,aux;	// Gemotric Mean & Arithmetic Mean

		float32_t Energy;
		uint32_t  Frecmax;
		float32_t SpFlat;
	//	float32_t SilZeroCross;

//---------------------------------------
//				CALIBRATION VARIABLES
//---------------------------------------
		float32_t *SilEnergy  = NULL;
		uint32_t  *SilFrecmax = NULL;
		float32_t *SilSpFlat  = NULL;
		
		float32_t SilEnergyMean;
		uint32_t	SilFmaxMean;
		float32_t SilSpFlatMean;
	//	float32_t SilZeroCrossMean;

		float32_t SilEnergyDev;
		uint32_t	SilFmaxDev;
		float32_t SilSpFlatDev;
	//	float32_t SilZeroCrossDev;

		float32_t THD_E;
		uint32_t	THD_FMX;
		float32_t THD_SF;

	
//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------
//
//--------	PROCESSING FUNCTIONS --------
//
void initProcessing (float32_t **MFCC, uint32_t *MFCC_size, ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers){
	
	initBasics (configuration, vad, ptr_vars_buffers);
	
	// Get memory for variables
	if( (MFCC_buff = pvPortMalloc(proc_conf.lifter_legnth * sizeof(*MFCC_buff))) == NULL)
		Error_Handler();
	
	// Copy it to variables
	*MFCC = MFCC_buff;
	*MFCC_size = proc_conf.lifter_legnth * sizeof(*MFCC_buff);
	
	// Creo el Lifter (filtro de Cepstrum)
	if( (CepWeight = pvPortMalloc(proc_conf.lifter_legnth * sizeof(*CepWeight))) == NULL)
		Error_Handler();
	Lifter_float (CepWeight,proc_conf.lifter_legnth);
	
	// Instance IFFT - DCT
	if(arm_rfft_fast_init_f32 (&DCTinst, proc_conf.ifft_len) == ARM_MATH_ARGUMENT_ERROR)
		Error_Handler();
}
void finishProcessing(void){
	// Termino
	finishBasics();
	
	// Libero memoria
	vPortFree(MFCC_buff);							MFCC_buff = NULL;
	vPortFree(CepWeight);							CepWeight = NULL;
}
ProcStatus MFCC_float (uint16_t *frame, ProcStages *stages) {
	
	ProcStatus output = NO_VOICE;
	
	// Copio el puntero del nuevo frame al que usa audio_processing
	new_frame = frame;
	
	// Ejecuto la primera parte del procesamiento
	firstProcStage (vars_buffers);
	frame_num++;
	*stages = First_Stage;
	
	if (frame_num >1)
	{
		secondProcStage (use_vad, vars_buffers);
		*stages |= Second_Stage;
		
		/* Check if it is a Voiced Frame */
		if( !use_vad || (Energy > THD_E   &&  Frecmax < THD_FMX) || SpFlat > THD_SF)
		{
			thirdProcStage (MFCC_buff, vars_buffers);
			output = VOICE;
			*stages |= Third_Stage;
		}
		
		if(vars_buffers != NULL)
		{
			if(output == NO_VOICE)
			{
				memset(vars_buffers->MelWin, 0, proc_conf.mel_banks * sizeof(*(vars_buffers->MelWin)));
				memset(vars_buffers->LogWin, 0, proc_conf.ifft_len  * sizeof(*(vars_buffers->LogWin)));
				memset(vars_buffers->CepWin, 0, proc_conf.ifft_len  * sizeof(*(vars_buffers->CepWin)));
				vars_buffers->VAD = 0;
			}
			else
				vars_buffers->VAD = 1;
		}
	}

	
	return output;
}
//
//-------- CALIBRATION FUNCTIONS --------
//
CalibStatus initCalibration	(uint32_t *num_frames,CalibConf *calib_config, ProcConf *configuration, Proc_var *ptr_vars_buffers){
	// Initialized basic configuration
	initBasics (configuration, true, ptr_vars_buffers);
	
	// Copy Calibration configuration
	calib_conf = *calib_config;
	
	// Allocate space for variables
	SilEnergy  = pvPortMalloc(calib_conf.calib_len * sizeof(*SilEnergy));
	SilFrecmax = pvPortMalloc(calib_conf.calib_len * sizeof(*SilFrecmax));
	SilSpFlat  = pvPortMalloc(calib_conf.calib_len * sizeof(*SilSpFlat));
	if(SilEnergy == NULL || SilFrecmax == NULL || SilSpFlat == NULL )
		Error_Handler();
	
	// Initialize frame count
	*num_frames = calib_conf.calib_len;
	
	return CALIB_INITIATED;
}
CalibStatus Calibrate (uint16_t *frame, uint32_t frame_num, ProcStages *stages) {
	//TODO: ARREGLAR CALIBRATION QUE ESTA MAL
	if ( frame_num < calib_conf.calib_len)
	{
		// Copio el puntero del nuevo frame al que usa audio_processing
		new_frame = frame;
	
		// Ejecuto la primera parte del procesamiento
		firstProcStage (vars_buffers);
		frame_num++;
		*stages = First_Stage;
		
		if (frame_num >1)
		{
			secondProcStage (true, vars_buffers);
			*stages |= Second_Stage;
			
			/* Check if it is a Voiced Frame */
			SilEnergy[frame_num] = Energy;
			SilFrecmax[frame_num] = Frecmax;
			SilSpFlat[frame_num] = SpFlat;
		}
	}
	
	if ( frame_num < calib_conf.calib_len-1)
		return CALIB_IN_PROCESS;
	else
		return CALIB_FINISH;
	
}
CalibStatus endCalibration	(const bool save_calib_vars) {

	// Calculate Mean of Features
	arm_mean_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyMean);
	arm_mean_q31 ((q31_t*)SilFrecmax, calib_conf.calib_len,(q31_t*) &SilFmaxMean);
	arm_mean_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatMean);

	// Calculate Deviation of Features
	arm_std_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyDev);
	arm_std_q31 ((q31_t*)SilFrecmax, calib_conf.calib_len,(q31_t*) &SilFmaxDev);
	arm_std_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatDev);

	// Free memory
	vPortFree(SilEnergy);		SilEnergy = NULL;
	vPortFree(SilFrecmax);	SilFrecmax = NULL;
	vPortFree(SilSpFlat);		SilSpFlat = NULL;
	
	// Set Thresholds
	THD_E   = SilEnergyMean + calib_conf.thd_scl_eng * SilEnergyDev;
	THD_FMX = SilFmaxMean > calib_conf.thd_min_fmax ? SilFmaxMean : calib_conf.thd_min_fmax;
	THD_SF  = fabs(SilSpFlatMean) + calib_conf.thd_scl_sf * SilSpFlatDev;

	if(save_calib_vars)
	{
		FIL CalibFile;
		uint32_t byteswritten;     							/* File write/read counts */	
		
		// Guardo la Energía
		if(f_open(&CalibFile, "CLB_ENR.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilEnergyMean,sizeof(SilEnergyMean),(void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilEnergyDev,	sizeof(SilEnergyDev), (void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &THD_E, 				sizeof(THD_E), 				(void*)&byteswritten) != FR_OK) Error_Handler();
		f_close(&CalibFile);
			
		// Guardo la Frecuencia Máxima
		if(f_open(&CalibFile, "CLB_FMX.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilFmaxMean,sizeof(SilFmaxMean),(void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilFmaxDev, sizeof(SilFmaxDev),	(void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &THD_FMX,		sizeof(THD_FMX),		(void*)&byteswritten) != FR_OK) Error_Handler();
		f_close(&CalibFile);
		
				// Guardo el Spectral Flatness
		if(f_open(&CalibFile, "CLB_SF.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilSpFlatMean,sizeof(SilSpFlatMean),(void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &SilSpFlatDev,	sizeof(SilSpFlatDev),	(void*)&byteswritten) != FR_OK) Error_Handler();
		if(f_write(&CalibFile, &THD_SF, 			sizeof(THD_SF),				(void*)&byteswritten) != FR_OK) Error_Handler();
		f_close(&CalibFile);
	}
	
	// Finish basics
	finishBasics();	
	
	return CALIB_OK;
}
//---------------------------------------
//			BASE PROCESSING FUNCTIONS
//---------------------------------------
void initBasics (ProcConf *configuration, bool vad, Proc_var *ptr_vars_buffers) {
	
	// Copio la configuración de procesamiento
	proc_conf = *configuration;
	use_vad = vad;
	frame_num = 0;
	
	// Armo el filtro de Preénfasis
	// Los coeficientes tienen que estar almacenados en tiempo invertido (leer documentación)
	if( (Pre_enfasis_Coeef = pvPortMalloc( proc_conf.numtaps * sizeof(*Pre_enfasis_Coeef) ) ) == NULL )
		Error_Handler();
	Pre_enfasis_Coeef[0] = -proc_conf.alpha;
	Pre_enfasis_Coeef[1]= 1;
	
	// Alloco memoria para el filtro FIR
	if( (pState = pvPortMalloc( (proc_conf.numtaps + proc_conf.frame_net - 1) * sizeof(*pState) ) ) == NULL )
		Error_Handler();
	arm_fir_init_f32 (&FirInst, proc_conf.numtaps, Pre_enfasis_Coeef, pState, proc_conf.frame_net);
	
	// Alloco memoria para la variable frame_block y la inicializo en cero
	if( (frame_block = pvPortMalloc(proc_conf.frame_len*sizeof(*frame_block))) == NULL)
		Error_Handler();
	memset(frame_block, 0, proc_conf.frame_len*sizeof(*frame_block));		// Inicializo en 0

	// Creo la ventana de Hamming
	if( (HamWin = pvPortMalloc(proc_conf.frame_len* sizeof(*HamWin))) == NULL)
		Error_Handler();
	Hamming_float(HamWin,proc_conf.frame_len);
	
	// Instance FFT
	if(arm_rfft_fast_init_f32 (&RFFTinst, proc_conf.fft_len) == ARM_MATH_ARGUMENT_ERROR)
		Error_Handler();
	
	// Alloco memoria para var1 que la uso como auxiliar
	if( (var1 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*var1) ) ) == NULL )
		Error_Handler();
	
	// Allocate variables
	if(ptr_vars_buffers != NULL)
	{
		allocateProcVariables (ptr_vars_buffers);
		vars_buffers = ptr_vars_buffers;
	}
	else
	{
		if( (var2 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*var2) ) ) == NULL )
			Error_Handler();
	}
}
void finishBasics	(void) {
	// Libero memoria
	vPortFree(Pre_enfasis_Coeef);			Pre_enfasis_Coeef = NULL;
	vPortFree(pState);								pState = NULL;
	vPortFree(frame_block);						frame_block = NULL;
	vPortFree(HamWin);								HamWin = NULL;
	vPortFree(var1);									var1 = NULL;
	vPortFree(var2);									var2 = NULL;
	
	if(vars_buffers != NULL)
	{
		freeProcVariables(vars_buffers);
		vars_buffers = NULL;
	}
}
void firstProcStage (Proc_var *saving_var) {
	
	if(saving_var != NULL)
	{
		/* Convierto a Float y escalo */
		arm_q15_to_float((q15_t*)new_frame, saving_var->speech, proc_conf.frame_net);

		/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
		arm_fir_f32 (&FirInst, saving_var->speech, saving_var->FltSig, proc_conf.frame_net);

		// Shifteo la ventana FRAME_OVERLAP veces
		// Para ello, shifteo lo viejo y copio el nuevo frame al final
		memcpy(&frame_block[proc_conf.zero_padding_left], &frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap], (proc_conf.frame_net + proc_conf.frame_overlap) * sizeof(*frame_block));
		memcpy(&frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap + proc_conf.frame_net], saving_var->FltSig, proc_conf.frame_net * sizeof(*saving_var->FltSig));
	}
	else
	{
		/* Convierto a Float y escalo */
		arm_q15_to_float((q15_t*)new_frame, var1, proc_conf.frame_net);

		/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
		arm_fir_f32 (&FirInst, var1, var2, proc_conf.frame_net);

		// Shifteo la ventana FRAME_OVERLAP veces
		// Para ello, shifteo lo viejo y copio el nuevo frame al final
		memcpy(&frame_block[proc_conf.zero_padding_left], &frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap], (proc_conf.frame_net + proc_conf.frame_overlap) * sizeof(*frame_block));
		memcpy(&frame_block[proc_conf.zero_padding_left + proc_conf.frame_overlap + proc_conf.frame_net], var2, proc_conf.frame_net * sizeof(*var2));
	}
}
void secondProcStage (bool vad, Proc_var *saving_var) {

	uint32_t Index;
	float32_t AM,GM;
		
	if(saving_var != NULL)
	{
		/* Se le aplica la ventana de Hamming al segmento obtenido */
		arm_mult_f32 (frame_block, HamWin, saving_var->WinSig, proc_conf.frame_len);
				
		if (vad)
		{
			/* Calculo la energía */
			arm_power_f32 (saving_var->WinSig, proc_conf.frame_len, &Energy);
			Energy /= proc_conf.frame_len;
//			saving_var->vad.Energy = Energy;
		}
		
		/* Se calcula la STFT */
		arm_copy_f32 (saving_var->WinSig, var1, proc_conf.frame_len);
		arm_rfft_fast_f32(&RFFTinst,var1,saving_var->STFTWin,0);
		saving_var->STFTWin[1]=0;																									//Borro la componente de máxima frec

		/* Calculo el módulo de la FFT */
		arm_cmplx_mag_squared_f32  (saving_var->STFTWin, saving_var->MagFFT, proc_conf.fft_len/2);
		
		if (vad)
		{
			/* Obtengo la Fmax */
			arm_max_f32 (saving_var->MagFFT, proc_conf.fft_len/2, &aux, &Index);									// no me interesa el valor máximo, sino en que lugar se encuentra
			Frecmax = Index;
//			saving_var->vad.Frecmax = Frecmax;
			
			/* Calculo el Spectral Flatness */
			arm_mean_f32 (saving_var->MagFFT, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
			sumlog(saving_var->MagFFT, &GM, proc_conf.fft_len/2);
			GM = expf(GM/(proc_conf.fft_len/2));													// Geometric Mean
			SpFlat = 10*log10f(GM/AM);
//			saving_var->vad.SpFlat = SpFlat;
		}
	}
	else
	{
		/* Se le aplica la ventana de Hamming al segmento obtenido */
		arm_mult_f32 (frame_block, HamWin, var1, proc_conf.frame_len);

		if (vad)
		{
			/* Calculo la energía */
			arm_power_f32 (var1, proc_conf.frame_len, &Energy);
			Energy /= proc_conf.frame_len;
		}
		
		/* Se calcula la STFT */
		arm_rfft_fast_f32(&RFFTinst,var1,var2,0);
		var2[1]=0;																									//Borro la componente de máxima frec

		/* Calculo el módulo de la FFT */
		arm_cmplx_mag_squared_f32  (var2, var1, proc_conf.fft_len/2);
		
		if (vad)
		{
			/* Obtengo la Fmax */
			arm_max_f32 (var1, proc_conf.fft_len/2, &aux, &Index);									// no me interesa el valor máximo, sino en que lugar se encuentra
			Frecmax = Index; //TODO Multiplicar por AUDIO_FREQ/FFT_LEN

			/* Calculo el Spectral Flatness */
			arm_mean_f32 (var1, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
			sumlog(var1, &GM, proc_conf.fft_len/2);
			GM = expf(GM/(proc_conf.fft_len/2));							// Geometric Mean
			SpFlat = 10*log10f(GM/AM);
		}
	}
}
void thirdProcStage (float32_t *MFCC, Proc_var *saving_var) {
	uint32_t i;
	if(saving_var != NULL)
	{
		/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
		arm_mat_init_f32 (&MagFFTMtx,   proc_conf.fft_len/2, 1, saving_var->MagFFT);				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
		arm_mat_init_f32 (&MelWinMtx, proc_conf.mel_banks, 1, saving_var->MelWin);					// Se crea una matriz para almacenar el resultado
		if(arm_mat_mult_f32(&MelFilt, &MagFFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)		// MelFilt[MEL_BANKS,proc_conf.fft_len] * MagFFTMtx[proc_conf.fft_len,1] = MelWinMtx [MEL_BANKS,1]
			Error_Handler();
		
		/* Se obtienen los valores logaritmicos de los coeficientes y le hago zero-padding */
		arm_fill_f32 	(0,saving_var->LogWin,proc_conf.ifft_len);
		for (i=0; i<proc_conf.mel_banks; i++)
			saving_var->LogWin[i*2+2] = log10f(saving_var->MelWin[i]);
		
		/* Se Anti-transforma aplicando la DCT-II ==> hago la anti-transformada real */
		arm_rfft_fast_f32(&DCTinst,saving_var->LogWin,saving_var->CepWin,1);

		/* Se pasa la señal por un filtro en el campo Cepstral */
		arm_mult_f32 (saving_var->CepWin,CepWeight,MFCC,proc_conf.lifter_legnth);
	}
	else
	{
		/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
		arm_mat_init_f32 (&MagFFTMtx,   proc_conf.fft_len/2, 1, var1);																				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
		arm_mat_init_f32 (&MelWinMtx, proc_conf.mel_banks, 1, var2);																				// Se crea una matriz para almacenar el resultado
		if(arm_mat_mult_f32(&MelFilt, &MagFFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)	Error_Handler();			// MelFilt[MEL_BANKS,proc_conf.fft_len] * MagFFTMtx[proc_conf.fft_len,1] = MelWinMtx [MEL_BANKS,1]
		
		
		/* Se obtienen los valores logaritmicos de los coeficientes y le hago zero-padding */
		arm_fill_f32 	(0,var1,proc_conf.ifft_len);
		for (i=0; i<proc_conf.mel_banks; i++)
			var1[i*2+2] = log10f(var2[i]);
		
		
		/* Se Anti-transforma aplicando la DCT-II ==> hago la anti-transformada real */
		arm_rfft_fast_f32(&DCTinst,var1,var2,1);
		
		/* Se pasa la señal por un filtro en el campo Cepstral */
		arm_mult_f32 (var2,CepWeight,MFCC,proc_conf.lifter_legnth);
	}
}
void Hamming_float (float32_t *Hamming, uint32_t length) {
	uint32_t i;
	const float32_t a=0.54 , b=0.46;
	
	for(i=0; i < length; i++) {
		Hamming[i] = a - b * arm_cos_f32 ((float32_t)2*PI*i/length);
	}
}
void Lifter_float (float32_t *Lifter, uint32_t length) {
	
	float32_t theta;
	uint32_t n;
	
	for(n=0; n<length; n++) {
		theta = PI*n/length;
		Lifter[n] = 1 + arm_sin_f32(theta) * length/2;
	}
}




//---------------------------------------
//					MISCELLANEOUS FUNCTIONS
//---------------------------------------
void allocateProcVariables (Proc_var *var) {
	var->speech		= pvPortMalloc(proc_conf.frame_net * sizeof(*(var->speech)));		// Señal de audio escalada
	var->FltSig		= pvPortMalloc(proc_conf.frame_net * sizeof(*(var->FltSig)));		// Señal de audio pasada por el Filtro de Pre-Enfasis
	
	var->WinSig		= pvPortMalloc(proc_conf.frame_len * sizeof(*(var->WinSig)));		// Señal de filtrada multiplicada por la ventana de Hamming
	var->STFTWin	= pvPortMalloc(proc_conf.fft_len   * sizeof(*(var->STFTWin)));	// Transformada de Fourier en Tiempo Corto
	var->MagFFT		= pvPortMalloc(proc_conf.fft_len/2 * sizeof(*(var->MagFFT)));		// Módulo del espectro
	var->MelWin		= pvPortMalloc(proc_conf.mel_banks * sizeof(*(var->MelWin)));		// Espectro pasado por los filtros de Mel
	var->LogWin		= pvPortMalloc(proc_conf.ifft_len  * sizeof(*(var->LogWin)));		// Logaritmo del espectro filtrado
	var->CepWin		= pvPortMalloc(proc_conf.ifft_len  * sizeof(*(var->CepWin)));		// Señal cepstral
	
	if(var->speech == NULL || var->FltSig == NULL || var->WinSig == NULL || var->STFTWin == NULL || 
		 var->MagFFT == NULL || var->MelWin == NULL || var->LogWin == NULL || var->CepWin == NULL)
		Error_Handler();
}
void freeProcVariables (Proc_var *var) {
	vPortFree(var->speech);				var->speech = NULL;
	vPortFree(var->FltSig);				var->FltSig = NULL;
	vPortFree(var->WinSig);				var->WinSig = NULL;
	vPortFree(var->STFTWin);			var->STFTWin = NULL;
	vPortFree(var->MagFFT);				var->MagFFT = NULL;
	vPortFree(var->MelWin);				var->MelWin = NULL;
	vPortFree(var->LogWin);				var->LogWin = NULL;
	vPortFree(var->CepWin);				var->CepWin = NULL;
}
uint8_t Open_proc_files (Proc_files *files, const bool vad) {
	
	UINT byteswritten;
	
	// Escribo un archivo con los coeficientes de la ventana de Hamming			
	if(f_open(&files->HamWinFile, "HamWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)Error_Handler();
	if(f_write(&files->HamWinFile, HamWin, proc_conf.frame_len * sizeof(*HamWin), &byteswritten) != FR_OK)	Error_Handler();
	f_close(&files->HamWinFile);

	// Escribo un archivo con los coeficientes del Filtro Cepstral
	if(f_open(&files->CepWeiFile, "CepWei.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_write(&files->CepWeiFile, CepWeight, proc_conf.lifter_legnth * sizeof(*CepWeight), &byteswritten) != FR_OK)	Error_Handler();
	f_close(&files->CepWeiFile);
					
	//Abro los archivos
	if(f_open(&files->SpeechFile, "Speech.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->FltSigFile, "FltSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->FrameFile, 	"Frames.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->WinSigFile, "WinSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->STFTWinFile,"FFTWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->MagFFTFile, "MagFFT.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->MelWinFile, "MelWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->LogWinFile, "LogWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->CepWinFile, "CepWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

	if (vad)
	{
		if(f_open(&files->VADFile,			 "VAD.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->EnerFile,		"Energy.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->FrecFile,  "FrecMax.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->SFFile,		"SpecFlat.bin",	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	}

//	if(f_open(&files->nosirve, "nosirve.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

	return 0;
}
uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad, ProcStages stage) {
	UINT byteswritten;
	
	//Escribo los valores intermedios en un archivo
	
	if (stage & First_Stage)
	{
		if(f_write(&files->SpeechFile,  data->speech, proc_conf.frame_net * sizeof(*(data->speech)),  &byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->FltSigFile,  data->FltSig, proc_conf.frame_net * sizeof(*(data->FltSig)),  &byteswritten) != FR_OK)	Error_Handler();
	}
	
	if(stage & Second_Stage)
	{
		if(f_write(&files->FrameFile,  	frame_block,  proc_conf.frame_len * sizeof(*frame_block),  		&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->WinSigFile,  data->WinSig, proc_conf.frame_len * sizeof(*(data->WinSig)),  &byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->STFTWinFile, data->STFTWin,proc_conf.fft_len		* sizeof(*(data->STFTWin)), &byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->MagFFTFile,  data->MagFFT, proc_conf.fft_len/2 * sizeof(*(data->MagFFT)),  &byteswritten) != FR_OK)	Error_Handler();
	}
	
	if(stage & Third_Stage)
	{
		if(f_write(&files->MelWinFile,  data->MelWin, proc_conf.mel_banks * sizeof(*(data->MelWin)),  &byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->LogWinFile,  data->LogWin, proc_conf.ifft_len  *	sizeof(*(data->LogWin)),  &byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->CepWinFile,  data->CepWin, proc_conf.ifft_len  * sizeof(*(data->CepWin)),  &byteswritten) != FR_OK)	Error_Handler();
	}
	
	if(vad)
	{
		if(f_write(&files->VADFile,	&data->VAD,	sizeof(data->VAD),&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->EnerFile,&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->FrecFile,&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->SFFile,	&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler();
	}
	return 1;
}
uint8_t Close_proc_files (Proc_files *files, const bool vad) {
		
	f_close(&files->SpeechFile);
	f_close(&files->FltSigFile);
	f_close(&files->FrameFile);
	f_close(&files->WinSigFile);
	f_close(&files->STFTWinFile);
	f_close(&files->MagFFTFile);
	f_close(&files->MelWinFile);
	f_close(&files->LogWinFile);
	f_close(&files->CepWinFile);

	if(vad)
	{
		f_close(&files->VADFile);
		f_close(&files->EnerFile);
		f_close(&files->FrecFile);
		f_close(&files->SFFile);
	}
	
//	f_close(&files->nosirve);
	return 1;
}

