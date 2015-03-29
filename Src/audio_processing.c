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


//---------------------------------------
//				SPEECH PROCESSING VARIALBES
//---------------------------------------

	ProcConf proc_conf;				// Processing configurartion
	CalibConf calib_conf;			// Calibration configurartion
	uint16_t  *buff;					// [proc_conf.frame_len];
	float32_t *var1 ; 				// [proc_conf.frame_len];
	float32_t *var2 ; 				// [proc_conf.fft_len];
	float32_t *pState;
	
	float32_t *Pre_enfasis_Coeef;
	
	float32_t *HamWin 	 ; 		// [proc_conf.frame_len];
	float32_t *CepWeight ; 		// [LIFTER_LEGNTH];
	
//---------------------------------------
//							VAD VARIALBES
//---------------------------------------
	__align(8)
		float32_t GM,AM,aux;	// Gemotric Mean & Arithmetic Mean

		float32_t Energy;
		uint32_t  Frecmax;
		float32_t SpFlat;
	//	float32_t SilZeroCross;

		float32_t SilEnergyMean;
		float32_t SilSpFlatMean;
	//	float32_t SilZeroCrossMean;

		float32_t SilEnergyDev;
		float32_t SilSpFlatDev;
	//	float32_t SilZeroCrossDev;

		float32_t THD_E;
		float32_t THD_SF;


	
	extern float32_t MelBank [256*20];
	extern arm_matrix_instance_f32	MelFilt;
	
	arm_matrix_instance_f32 				MagFFTMtx, MelWinMtx;
	arm_fir_instance_f32 						FirInst;	
	arm_rfft_fast_instance_f32 			RFFTinst, DCTinst;
	
	
	
//---------------------------------------
//						USER FUNCTIONS
//---------------------------------------

void initProcessing(ProcConf *configuration, float32_t **MFCC, uint32_t *MFCC_size, Proc_var *saving_var){
	
	// Copio la configuración de procesamiento
	memcpy(&proc_conf,configuration,sizeof(ProcConf));
	
	// Alloco memoria para la variable buff y la inicializo en cero
	if( (buff = pvPortMalloc(proc_conf.frame_len*sizeof(*buff))) == NULL)
		Error_Handler();
	memset(buff, 0, proc_conf.frame_len*sizeof(*buff));
	
	// Get memory for variables
	if( (*MFCC = pvPortMalloc(proc_conf.lifter_legnth * sizeof(**MFCC))) == NULL)
		Error_Handler();
	*MFCC_size = proc_conf.lifter_legnth * sizeof(**MFCC);
	
	// Creo la ventana de Hamming
	if( (HamWin = pvPortMalloc(proc_conf.frame_len * sizeof(*HamWin))) == NULL)
		Error_Handler();
	Hamming_float(HamWin,proc_conf.frame_len);
	
	// Creo el Lifter (filtro de Cepstrum)
	if( (CepWeight = pvPortMalloc(proc_conf.lifter_legnth * sizeof(*CepWeight))) == NULL)
		Error_Handler();
	Lifter_float (CepWeight,proc_conf.lifter_legnth);
	
	// Armo el filtro de Preénfasis
	if( (Pre_enfasis_Coeef = pvPortMalloc( proc_conf.numtaps * sizeof(*Pre_enfasis_Coeef) ) ) == NULL )
		Error_Handler();
	
	// Los coeficientes tienen que estar almacenados en tiempo invertido (leer documentación)
	Pre_enfasis_Coeef[0] = -proc_conf.alpha;
	Pre_enfasis_Coeef[1]= 1;
	
	if( (pState = pvPortMalloc( (proc_conf.numtaps + proc_conf.frame_len - 1) * sizeof(*pState) ) ) == NULL )
		Error_Handler();
	arm_fir_init_f32 (&FirInst, proc_conf.numtaps, Pre_enfasis_Coeef, pState, proc_conf.frame_len);
	
	// Instance FFT
	if(arm_rfft_fast_init_f32 (&RFFTinst, proc_conf.fft_len))
		Error_Handler();
	
	// Instance IFFT - DCT
	if(arm_rfft_fast_init_f32 (&DCTinst, proc_conf.ifft_len) == ARM_MATH_ARGUMENT_ERROR)
		Error_Handler();
	
	// Alloco memoria para var1 que la uso como auxiliar
	if( (var1 = pvPortMalloc( proc_conf.frame_len * sizeof(*var1) ) ) == NULL )
		Error_Handler();
	
	if(saving_var != NULL)
	{
		if( (var2 = pvPortMalloc( proc_conf.frame_len * sizeof(*var1) ) ) == NULL )
		Error_Handler();
	}
}
void deinitProcessing(float32_t *MFCC){
	// Libero memoria
	vPortFree(buff);
	vPortFree(MFCC);
	vPortFree(HamWin);
	vPortFree(CepWeight);
	vPortFree(pState);
}
void allocateProcVariables (Proc_var *var){
	var->speech		= pvPortMalloc(proc_conf.frame_len * sizeof(*(var->speech)));		// Señal de audio escalada
	var->FltSig		= pvPortMalloc(proc_conf.frame_len * sizeof(*(var->FltSig)));		// Señal de audio pasada por el Filtro de Pre-Enfasis
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
void freeProcVariables (Proc_var *var){
	vPortFree(var->speech);
	vPortFree(var->FltSig);
	vPortFree(var->WinSig);
	vPortFree(var->STFTWin);
	vPortFree(var->MagFFT);
	vPortFree(var->MelWin);
	vPortFree(var->LogWin);
	vPortFree(var->CepWin);
}

void MFCC_float (uint16_t *frame, float32_t *MFCC, bool vad, Proc_var *saving_var) {

	// Muevo los datos de FRAME_OVERLAP al principio del buffer
	memcpy(&buff[0], &buff[proc_conf.frame_net], proc_conf.frame_overlap * sizeof(*buff));
	
	// Copio los nuevos datos de frame (FRAME_NET)
	memcpy(&buff[proc_conf.frame_overlap], frame, proc_conf.frame_net * sizeof(*buff));
	
	firstProcStage (vad, saving_var);
		
	/* Check if it is a Voiced Frame */
	if( !vad || (Energy > proc_conf.thd_e   &&   proc_conf.thd_fl < Frecmax && Frecmax< proc_conf.thd_fh) || SpFlat > proc_conf.thd_sf)
		secondProcStage (MFCC, saving_var);
	else
		memset(MFCC, 0, proc_conf.lifter_legnth * sizeof(*MFCC));
}
/**
  * @brief  Get from frame VAD variables
	* @param  Energy
	* @param  Max Frecuency
	* @param  Spectral Flatness
  */
void VAD_float (uint16_t *frame, VADVar *var, Proc_var *saving_var) {

	static uint16_t *proc;
	
	// Copio los nuevos datos de frame (FRAME_NET)
	memcpy(&buff[proc_conf.frame_overlap], frame, proc_conf.frame_net*sizeof(*buff));
	
	firstProcStage (true, saving_var);
		
	// Muevo los datos de FRAME_OVERLAP al principio del buffer
	memcpy(&buff[0], &buff[proc_conf.frame_net], proc_conf.frame_overlap*sizeof(*buff));
	
	/* Check if it is a Voiced Frame */
	Energy = var->Energy;
	Frecmax = var->Frecmax;
	SpFlat = var->SpFlat;
}
/**
  * @brief  Get from frame VAD variables
	* @param  Energy
	* @param  Max Frecuency
	* @param  Spectral Flatness
  */
void Calibrate (float32_t* SilEnergy, float32_t* SilFrecmax, float32_t* SilSpFlat){
	
	//TODO: en vez de leer la variables globales, levanto los archivos y leo desde ahi
	// Calculate Mean of Features
	arm_mean_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyMean);
//			arm_mean_f32 (SilFrecmax,  proc_conf.calib_len, &SilFrecmaxMean);
	arm_mean_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatMean);

	// Calculate Deviation of Features
	arm_std_f32 (SilEnergy, calib_conf.calib_len, &SilEnergyDev);
//			arm_std_f32 (SilFrecmax,  proc_conf.calib_len, &SilFrecmaxDev);
	arm_std_f32 (SilSpFlat, calib_conf.calib_len, &SilSpFlatDev);
	
	THD_E  = (float32_t) SilEnergyMean * proc_conf.thd_e * SilEnergyDev;
	THD_SF = (float32_t) abs(SilSpFlatMean) * proc_conf.thd_sf * SilSpFlatDev;

	FIL CalibFile;
	uint32_t byteswritten;     							/* File write/read counts */
	char CalibFileName[]="CALIBVAR.bin";
	
	// Record values in file
	if(f_open(&CalibFile, CalibFileName, FA_CREATE_NEW | FA_WRITE) != FR_OK) Error_Handler();
	
	// Guardo los vectores
	if(f_write(&CalibFile, &SilEnergy, 	sizeof(SilEnergy),	(void*)&byteswritten) != FR_OK) Error_Handler();
	if(f_write(&CalibFile, &SilFrecmax, sizeof(SilFrecmax),	(void*)&byteswritten) != FR_OK) Error_Handler();
	if(f_write(&CalibFile, &SilSpFlat,	sizeof(SilSpFlat),	(void*)&byteswritten) != FR_OK) Error_Handler();
	
	// Guardo las medias
	if(f_write(&CalibFile, &SilEnergyMean, 	sizeof(SilEnergyMean), 	(void*)&byteswritten) != FR_OK) Error_Handler();
	if(f_write(&CalibFile, &SilSpFlatMean,	sizeof(SilSpFlatMean),	(void*)&byteswritten) != FR_OK) Error_Handler();
	
	// Guardo las varianzas
	if(f_write(&CalibFile, &SilEnergyDev, 	sizeof(SilEnergyDev), 	(void*)&byteswritten) != FR_OK) Error_Handler();
	if(f_write(&CalibFile, &SilSpFlatDev,		sizeof(SilSpFlatDev),		(void*)&byteswritten) != FR_OK) Error_Handler();
	
	f_close(&CalibFile);
}
//---------------------------------------
//						HELP FUNCTIONS
//---------------------------------------

uint8_t Open_proc_files (Proc_files *files, const bool vad){
	
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
	if(f_open(&files->WinSigFile, "WinSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->STFTWinFile,"FFTWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->MagFFTFile, "MagFFT.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->MelWinFile, "MelWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->LogWinFile, "LogWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	if(f_open(&files->CepWinFile, "CepWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

	if (vad)
	{
//		if(f_open(&files->VADFile,			 "VAD.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->EnerFile,		"Energy.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->FrecFile,  "FrecMax.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
		if(f_open(&files->SFFile,		"SpecFlat.bin",	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();
	}

//	if(f_open(&files->nosirve, "nosirve.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

	return 0;
}
/**
 *
 */
uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad){
	UINT byteswritten;
	
	//Escribo los valores intermedios en un archivo
	if(f_write(&files->SpeechFile,  data->speech, proc_conf.frame_len * sizeof(*(data->speech)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->FltSigFile,  data->FltSig, proc_conf.frame_len * sizeof(*(data->FltSig)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->WinSigFile,  data->WinSig, proc_conf.frame_len * sizeof(*(data->WinSig)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->STFTWinFile, data->STFTWin,proc_conf.fft_len		* sizeof(*(data->STFTWin)), 	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->MagFFTFile,  data->MagFFT, proc_conf.fft_len/2 * sizeof(*(data->MagFFT)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->MelWinFile,  data->MelWin, proc_conf.mel_banks * sizeof(*(data->MelWin)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->LogWinFile,  data->LogWin, proc_conf.ifft_len  *	sizeof(*(data->LogWin)),  	&byteswritten) != FR_OK)	Error_Handler();
	if(f_write(&files->CepWinFile,  data->CepWin, proc_conf.ifft_len  * sizeof(*(data->CepWin)),  	&byteswritten) != FR_OK)	Error_Handler();

	if(vad)
	{
//		if(f_write(&files->VADFile,	&output,		sizeof(output),		&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->EnerFile,&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->FrecFile,&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler();
		if(f_write(&files->SFFile,	&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler();
	}
	return 1;
}
/**
 *
 */
uint8_t Close_proc_files (Proc_files *files, const bool vad){
		
	f_close(&files->SpeechFile);
	f_close(&files->FltSigFile);
	f_close(&files->WinSigFile);
	f_close(&files->STFTWinFile);
	f_close(&files->MagFFTFile);
	f_close(&files->MelWinFile);
	f_close(&files->LogWinFile);
	f_close(&files->CepWinFile);

	if(vad)
	{
//		f_close(&files->VADFile);
		f_close(&files->EnerFile);
		f_close(&files->FrecFile);
		f_close(&files->SFFile);
	}
	
//	f_close(&files->nosirve);
	return 1;
}

//---------------------------------------
//			BASE PROCESSING FUNCTIONS
//---------------------------------------
/**
* @brief  Calibration
* @param  
* @retval 
*/
void firstProcStage (bool vad, Proc_var *saving_var) {

	uint32_t Index;
	float32_t AM,GM;
		
	if(saving_var != NULL)
	{
		/* Convierto a Float y escalo */
		arm_q15_to_float((q15_t*)buff, saving_var->speech, proc_conf.frame_len);

		/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
		arm_fir_f32 (&FirInst, saving_var->speech, saving_var->FltSig, proc_conf.frame_len);

		/* Se le aplica la ventana de Hamming al segmento obtenido */
		arm_mult_f32 (saving_var->FltSig, HamWin, saving_var->WinSig, proc_conf.frame_len);
				
		if (vad)
		{
			/* Calculo la energía */
			arm_power_f32 (saving_var->WinSig, proc_conf.frame_len, &Energy);
			Energy /= proc_conf.frame_len;
			saving_var->vad.Energy = Energy;
		}
		
	// REVISAR LO DE ABAJO, NO SE SI ESTA BIEN
	//	/* Hago Zero-Padding si es necesario */
	//	if(proc_conf.fft_len != proc_conf.frame_len){
	//		//Relleno con ceros en la parte izquierda
	//		arm_fill_f32 	(0,STFTWin,(proc_conf.fft_len-proc_conf.frame_len)/2);
	//		//Relleno con ceros en la parte derecha
	//		arm_fill_f32 	(0,&STFTWin[(proc_conf.fft_len+proc_conf.frame_len)/2],(proc_conf.fft_len-proc_conf.frame_len)/2);
	//	}
		
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
			saving_var->vad.Frecmax = Frecmax;
			
			/* Calculo el Spectral Flatness */
			arm_mean_f32 (saving_var->MagFFT, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
			sumlog(var1, &GM, proc_conf.fft_len/2);
			GM = expf(GM/(proc_conf.fft_len/2));													// Geometric Mean
			SpFlat = 10*log10f(GM/AM);
			saving_var->vad.SpFlat = SpFlat;
		}
	}
	else
	{
		/* Convierto a Float y escalo */
		arm_q15_to_float((q15_t*)buff, var1, proc_conf.frame_len);

		/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
		arm_fir_f32 (&FirInst, var1, var2, proc_conf.frame_len);

		/* Se le aplica la ventana de Hamming al segmento obtenido */
		arm_mult_f32 (var2, HamWin, var1, proc_conf.frame_len);
		
		if (vad)
		{
			/* Calculo la energía */
			arm_power_f32 (var1, proc_conf.frame_len, &Energy);
			Energy /= proc_conf.frame_len;
		}

	// REVISAR LO DE ABAJO, NO SE SI ESTA BIEN
	//	/* Hago Zero-Padding si es necesario */
	//	if(proc_conf.fft_len != proc_conf.frame_len){
	//		//Relleno con ceros en la parte izquierda
	//		arm_fill_f32 	(0,STFTWin,(proc_conf.fft_len-proc_conf.frame_len)/2);
	//		//Relleno con ceros en la parte derecha
	//		arm_fill_f32 	(0,&STFTWin[(proc_conf.fft_len+proc_conf.frame_len)/2],(proc_conf.fft_len-proc_conf.frame_len)/2);
	//	}
		
		/* Se calcula la STFT */
		arm_rfft_fast_f32(&RFFTinst,var1,var2,0);
		var2[1]=0;																									//Borro la componente de máxima frec

		/* Calculo el módulo de la FFT */
		arm_cmplx_mag_squared_f32  (var2, var1, proc_conf.fft_len/2);
		
		if (vad)
		{
			/* Obtengo la Fmax */
			arm_max_f32 (var1, proc_conf.fft_len/2, &aux, &Index);									// no me interesa el valor máximo, sino en que lugar se encuentra
			Frecmax = Index;

			/* Calculo el Spectral Flatness */
			arm_mean_f32 (var1, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
			sumlog(var1, &GM, proc_conf.fft_len/2);
			GM = expf(GM/(proc_conf.fft_len/2));							// Geometric Mean
			SpFlat = 10*log10f(GM/AM);
		}
	}
}
/**
* @brief  Calibration
* @param  
* @retval 
*/
void secondProcStage (float32_t *MFCC, Proc_var *saving_var) {
	uint32_t i;
	if(saving_var != NULL)
	{
		/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
		arm_mat_init_f32 (&MagFFTMtx,   proc_conf.fft_len/2, 1, saving_var->MagFFT);				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
		arm_mat_init_f32 (&MelWinMtx, proc_conf.mel_banks, 1, saving_var->MelWin);				// Se crea una matriz para almacenar el resultado
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
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void Hamming_float (float32_t *Hamming, uint32_t length) {
	uint32_t i;
	const float32_t a=0.54 , b=0.46;
	
	for(i=0; i < length; i++) {
		Hamming[i] = a - b * arm_cos_f32 ((float32_t)2*PI*i/length);
	}
}
/**
  * @brief  Coefficients of the Hamming Window
	* @param  Hamming: Address of the vector where the coefficients are going to be save
	* @param  Length: Length of the Hamming Window
  * @retval 
  */
void Lifter_float (float32_t *Lifter, uint32_t length) {
	
	float32_t theta;
	uint32_t n;
	
	for(n=0; n<length; n++) {
		theta = PI*n/length;
		Lifter[n] = 1 + arm_sin_f32(theta) * length/2;
	}
}




//---------------------------------------
//						MATH FUNCTIONS
//---------------------------------------
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
