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
#include "ale_dct2_f32.h"
#include "ApplicationConfig.h"

osMessageQId proc_msg;

//---------------------------------------
//				CONFIGURATION VARIALBES
//---------------------------------------
	Proc_conf	proc_conf;						// Processing configurartion
	Calib_conf calib_conf;					// Calibration configurartion
	
	Proc_var *vars_buffers = NULL;
//---------------------------------------
//				SPEECH PROCESSING VARIALBES
//---------------------------------------

	uint16_t  *new_frame = NULL;					// [proc_conf.frame_net];
	float32_t *pState = NULL;							// [proc_conf.numtaps + proc_conf.frame_net - 1]
	float32_t *Pre_enfasis_Coeef = NULL;	// [proc_conf.numtaps]

	float32_t *frame_block = NULL;				// [ proc_conf.frame_len ];
	float32_t *aux1 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
	float32_t *aux2 = NULL; 							// [ max(proc_conf.frame_len,proc_conf.fft_len) ];
	float32_t *MFCC_buff = NULL;					// [proc_conf.lifterlength];
	
	
	float32_t *HamWin = NULL; 						// [proc_conf.frame_len];
	float32_t *CepWeight = NULL; 					// [LIFTER_length];

	static float32_t MelBank [256*20] = {
	0,0,0,0,0,0,0,0,0,0,1.8776061e-01f,6.1692772e-01f,9.5704521e-01f,5.5711352e-01f,1.5718182e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,4.2954786e-02f,4.4288648e-01f,8.4281818e-01f,7.7378656e-01f,4.0109872e-01f,2.8410879e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2.2621344e-01f,5.9890128e-01f,9.7158912e-01f,6.7917563e-01f,3.3187576e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3.2082437e-01f,6.6812424e-01f,9.8562660e-01f,6.6198524e-01f,3.3834388e-01f,1.4702527e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.4373400e-02f,3.3801476e-01f,6.6165612e-01f,9.8529747e-01f,7.1210648e-01f,4.1051198e-01f,1.0891749e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2.8789352e-01f,5.8948802e-01f,8.9108251e-01f,8.2044839e-01f,5.3939890e-01f,2.5834941e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.7955161e-01f,4.6060110e-01f,7.4165059e-01f,9.7884627e-01f,7.1694223e-01f,4.5503819e-01f,1.9313414e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2.1153727e-02f,2.8305777e-01f,5.4496181e-01f,8.0686586e-01f,9.3591479e-01f,6.9185199e-01f,4.4778918e-01f,2.0372637e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6.4085208e-02f,3.0814801e-01f,5.5221082e-01f,7.9627363e-01f,9.6241134e-01f,7.3497440e-01f,5.0753747e-01f,2.8010053e-01f,5.2663594e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3.7588662e-02f,2.6502560e-01f,4.9246253e-01f,7.1989947e-01f,9.4733641e-01f,8.3713244e-01f,6.2518880e-01f,4.1324516e-01f,2.0130152e-01f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.6286756e-01f,3.7481120e-01f,5.8675484e-01f,7.9869848e-01f,9.9008283e-01f,7.9257706e-01f,5.9507129e-01f,3.9756552e-01f,2.0005975e-01f,2.5539764e-03f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9.9171712e-03f,2.0742294e-01f,4.0492871e-01f,6.0243448e-01f,7.9994025e-01f,9.9744602e-01f,8.1832857e-01f,6.3427715e-01f,4.5022572e-01f,2.6617429e-01f,8.2122869e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.8167143e-01f,3.6572285e-01f,5.4977428e-01f,7.3382571e-01f,9.1787713e-01f,9.0501495e-01f,7.3350134e-01f,5.6198773e-01f,3.9047412e-01f,2.1896052e-01f,4.7446910e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9.4985053e-02f,2.6649866e-01f,4.3801227e-01f,6.0952588e-01f,7.8103948e-01f,9.5255309e-01f,8.8438488e-01f,7.2455500e-01f,5.6472512e-01f,4.0489524e-01f,2.4506536e-01f,8.5235476e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.1561512e-01f,2.7544500e-01f,4.3527488e-01f,5.9510476e-01f,7.5493464e-01f,9.1476452e-01f,9.3048706e-01f,7.8154499e-01f,6.3260293e-01f,4.8366086e-01f,3.3471880e-01f,1.8577673e-01f,3.6834663e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6.9512939e-02f,2.1845501e-01f,3.6739707e-01f,5.1633914e-01f,6.6528120e-01f,8.1422327e-01f,9.6316534e-01f,8.9552950e-01f,7.5673355e-01f,6.1793761e-01f,4.7914167e-01f,3.4034573e-01f,2.0154978e-01f,6.2753841e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1.0447050e-01f,2.4326645e-01f,3.8206239e-01f,5.2085833e-01f,6.5965427e-01f,7.9845022e-01f,9.3724616e-01f,9.2913798e-01f,7.9979700e-01f,6.7045601e-01f,5.4111503e-01f,4.1177404e-01f,2.8243305e-01f,1.5309207e-01f,2.3751082e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7.0862017e-02f,2.0020300e-01f,3.2954399e-01f,4.5888497e-01f,5.8822596e-01f,7.1756695e-01f,8.4690793e-01f,9.7624892e-01f,9.0160302e-01f,7.8107290e-01f,6.6054279e-01f,5.4001268e-01f,4.1948256e-01f,2.9895245e-01f,1.7842234e-01f,5.7892225e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9.8396985e-02f,2.1892710e-01f,3.3945721e-01f,4.5998732e-01f,5.8051744e-01f,7.0104755e-01f,8.2157766e-01f,9.4210777e-01f,9.4162909e-01f,8.2930964e-01f,7.1699019e-01f,6.0467074e-01f,4.9235129e-01f,3.8003185e-01f,2.6771240e-01f,1.5539295e-01f,4.3073503e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5.8370915e-02f,1.7069036e-01f,2.8300981e-01f,3.9532926e-01f,5.0764871e-01f,6.1996815e-01f,7.3228760e-01f,8.4460705e-01f,9.5692650e-01f,9.3547118e-01f,8.3080307e-01f,7.2613497e-01f,6.2146687e-01f,5.1679876e-01f,4.1213066e-01f,3.0746255e-01f,2.0279445e-01f,9.8126347e-02f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	};

	arm_matrix_instance_f32 MelFilt = {MEL_BANKS, FFT_LEN/2, MelBank};
		
	arm_matrix_instance_f32 				MagFFTMtx, MelWinMtx;
	arm_fir_instance_f32 						FirInst;	
	arm_rfft_fast_instance_f32 			RFFTinst, DCTinst_rfft;
	ale_dct2_instance_f32						DCTinst;

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

	

//
//--------	PROCESSING FUNCTIONS --------
//
void audioProc (void const *pvParameters)
{
	// Task variables
	Proc_args *args;
	osEvent event;
	uint8_t *aux;
	uint32_t read_size;
	VAD_var vad_vars;
	bool finish = false;	

	// Degug variables
	Proc_var debug_vars;
	Proc_files debug_files;

	// Buffers variables
	ringBuf flt_buff, mfcc_buff, delta_buff, delta2_buff;
	uint8_t proc_audio_client, proc_flt_client, proc_mfcc_client, proc_delta_client;
	uint8_t packg_mfcc_client, packg_delta_client, packg_delta2_client;
	float32_t *features;
		
	// Get arguments
	args = (Proc_args*) pvParameters;

	// Create Process Task State MessageQ
	osMessageQDef(proc_msg,10,uint32_t);
	proc_msg = osMessageCreate(osMessageQ(proc_msg),NULL);	
	
	// Init processing
	initProcessing (args->proc_conf, NULL);

	// Allocate space for aux variable
	if ( (aux = pvPortMalloc(args->proc_conf->frame_len * sizeof(float32_t) )) == NULL)
		Error_Handler("Error on malloc aux in audio processing");
	
	// Allocate space for features
	if ( (features = pvPortMalloc((1+proc_conf.lifter_length) * 3 * sizeof(*features) )) == NULL)
		Error_Handler("Error on malloc aux in audio processing");
	
	// Allocate space for aux variable
	if ( args->save_to_files )
	{
		f_chdir(args->path);
		Open_proc_files ( &debug_files, args->proc_conf->vad);
		allocateProcVariables	(&debug_vars);
		f_chdir("..");
	}
	
	// Init Ring Buffers
	ringBuf_init ( &flt_buff, 		args->proc_conf->frame_overlap 				* 	10 		* sizeof(float32_t), false);
	ringBuf_init ( &mfcc_buff, 		(1 + args->proc_conf->lifter_length)	* (5 + 5) * sizeof(float32_t), false);
	ringBuf_init ( &delta_buff, 	(1 + args->proc_conf->lifter_length)	* (5 + 5) * sizeof(float32_t), false);
	ringBuf_init ( &delta2_buff,	(1 + args->proc_conf->lifter_length)	* 	10 		* sizeof(float32_t), false);
	ringBuf_init ( args->features_buff,(1 + args->proc_conf->lifter_length)*3*		10 		* sizeof(float32_t), false);

	// Register Proc Clients in Ring Buffers
	ringBuf_registClient ( args->audio_buff,			args->proc_conf->frame_overlap * 1 * sizeof(uint16_t),	args->proc_conf->frame_overlap			* sizeof(uint16_t),	proc_msg, PROC_BUFF_READY,	&proc_audio_client);
	ringBuf_registClient ( &flt_buff,							args->proc_conf->frame_len     * 1 * sizeof(float32_t),	args->proc_conf->frame_overlap 			* sizeof(float32_t),NULL, 		NULL, 						&proc_flt_client);
	ringBuf_registClient ( &mfcc_buff,	   	(1 + args->proc_conf->lifter_length) * 5 * sizeof(float32_t),	(1 + args->proc_conf->lifter_length)* sizeof(float32_t),NULL, 		NULL,							&proc_mfcc_client);
	ringBuf_registClient ( &delta_buff,		 	(1 + args->proc_conf->lifter_length) * 5 * sizeof(float32_t),	(1 + args->proc_conf->lifter_length)* sizeof(float32_t),NULL, 		NULL,							&proc_delta_client);

	// Register Package Client in Ring Buffers
	ringBuf_registClient ( &mfcc_buff,	(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t),(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t), 	NULL,	NULL,	&packg_mfcc_client);
	ringBuf_registClient ( &delta_buff,	(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t),(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t),	NULL, NULL,	&packg_delta_client);
	ringBuf_registClient ( &delta2_buff,(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t),(1 + args->proc_conf->lifter_length) * 1 * sizeof(float32_t),	NULL, NULL,	&packg_delta2_client);
	
	//---------------------------- START TASK ----------------------------
	for(;;)
	{
		event = osMessageGet(proc_msg,osWaitForever);
		if(event.status == osEventMessage)
		{
			switch (event.value.v)
			{
				case PROC_BUFF_READY:
				{
					// Leemos del buffer de audio y procesamos la primera parte
					while ( ringBuf_read_const( args->audio_buff, proc_audio_client, aux ) == BUFF_OK)
					{
						// Procesamos la primera parte
						firstProcStage ( (float32_t*) aux, (uint16_t*) aux, &debug_vars);
						Append_proc_files ( &debug_files, &debug_vars, args->proc_conf->vad, First_Stage);
						
						// Guardamos la info en el buffer
						ringBuf_write  ( &flt_buff, aux, proc_conf.frame_overlap * sizeof(float32_t));
					}
									
					break;
				}
				case PROC_FINISH:
				{
					finish = true;
					
					// Leemos lo que queda en el buffer de audio
					ringBuf_read_all( args->audio_buff, proc_audio_client, aux, &read_size );
					
					// Hago zero padding de ser necesario
					memset(&aux[read_size], 0, proc_conf.frame_overlap * sizeof(uint16_t) - read_size);	
					
					 // Procesamos la primera parte
					firstProcStage 	( (float32_t*) aux, (uint16_t*) aux, &debug_vars);
					Append_proc_files ( &debug_files, &debug_vars, args->proc_conf->vad, First_Stage);
						
					// Guardamos la info en el buffer
					ringBuf_write 	( &flt_buff, aux, proc_conf.frame_overlap * sizeof(float32_t));
				}
				case PROC_KILL:
				{					
					// Libero memoria
					vPortFree(aux);
					vPortFree(features);
					
					if ( args->save_to_files )
					{
						Close_proc_files ( &debug_files, args->proc_conf->vad);
						freeProcVariables (&debug_vars);
					}
					
					// Me desregistro del bufer de audio
					ringBuf_unregistClient(args->audio_buff, proc_audio_client);
					
					// Finicializo el proceso
					finishProcessing();
			
					// Elimino los buffers
					ringBuf_deinit ( &flt_buff );
					ringBuf_deinit ( &mfcc_buff );
					ringBuf_deinit ( &delta_buff );
					ringBuf_deinit ( &delta2_buff );
	
					// Envío mensaje inidicando que termine
					osMessagePut(args->src_msg_id, args->src_msg_val, osWaitForever);
					
					// Espero a que todos los clinetes terminen de leer el buffer
					while( !is_ringBuf_empty ( args->features_buff ) )
						osDelay(10);

					// Elimino el buffer
					ringBuf_deinit(args->features_buff);
		
					// Turn off Processing LED
					LED_Off(BLED);
					
					// Destroy Message Que
					proc_msg = NULL;
					
					// Elimino la tarea
					osThreadTerminate(osThreadGetId());
				}
				default:
					break;
			}
			
			// Process filt audio while is posible
			while (ringBuf_read_const ( &flt_buff, proc_flt_client, aux ) == BUFF_OK)
			{
				// Process Second stage
				secondProcStage	( (float32_t*) aux, (float32_t*) aux, &debug_vars);
				Append_proc_files ( &debug_files, &debug_vars, args->proc_conf->vad, Second_Stage );
				
				// Process VAD
				VADFeatures			( &vad_vars, 				(float32_t*) aux, Energy);
				
				// Process Third stage
				thirdProcStage	( (float32_t*) aux, (float32_t*) aux, &debug_vars);
				Append_proc_files ( &debug_files, &debug_vars, args->proc_conf->vad, Third_Stage );
				
				ringBuf_write (&mfcc_buff, (uint8_t*) &Energy, sizeof(float32_t));
				ringBuf_write (&mfcc_buff, (uint8_t*) aux, proc_conf.lifter_length * sizeof(float32_t));
			}
			
			// Process mfcc while is posible
			while (ringBuf_read_const ( &mfcc_buff, proc_mfcc_client, aux ) == BUFF_OK)
			{
				deltaCoeff ( (float32_t*) aux, (float32_t*) aux);
				ringBuf_write (&delta_buff, aux, (proc_conf.lifter_length+1) * sizeof(float32_t));
			}

			// Process delta_mfcc while is posible
			while (ringBuf_read_const ( &delta_buff, proc_delta_client, aux ) == BUFF_OK)
			{
				deltaCoeff ( (float32_t*) aux, (float32_t*) aux);
				ringBuf_write (&delta2_buff, aux, (proc_conf.lifter_length+1) * sizeof(float32_t));
				
			}
			
			//Package mfcc, delta & delta2 and save in a buff
			if ( ringBuf_read_const ( &mfcc_buff, packg_mfcc_client, (uint8_t*) &features[ (1+proc_conf.lifter_length) * 0] ) == BUFF_OK )
				if ( ringBuf_read_const ( &delta_buff, packg_delta_client, (uint8_t*) &features[ (1+proc_conf.lifter_length) * 1] ) == BUFF_OK )
					if ( ringBuf_read_const ( &delta2_buff, packg_delta2_client, (uint8_t*) &features[ (1+proc_conf.lifter_length) * 2] ) == BUFF_OK )
						ringBuf_write (args->features_buff, (uint8_t*) features, (proc_conf.lifter_length+1)*3 * sizeof(float32_t));
						
					//TODO: Hacer una funcion de escritura para probar
			
			// Finish task
			if(finish)
				osMessagePut(proc_msg,PROC_KILL,0);
		}
	}
}

//
//-------- CALIBRATION FUNCTIONS --------
//
#ifdef ahora_no
Calib_status initCalibration	(uint32_t *num_frames,Calib_conf *Proc_config, Proc_conf *configuration, Proc_var *ptr_vars_buffers)
{
	// Initialized basic configuration
	initBasics (configuration, true, ptr_vars_buffers);
	
	// Copy Calibration configuration
	Proc_conf = *Proc_config;
	
	// Allocate space for variables
	SilEnergy  = pvPortMalloc(Proc_conf.calib_len * sizeof(*SilEnergy));
	SilFrecmax = pvPortMalloc(Proc_conf.calib_len * sizeof(*SilFrecmax));
	SilSpFlat  = pvPortMalloc(Proc_conf.calib_len * sizeof(*SilSpFlat));
	if(SilEnergy == NULL || SilFrecmax == NULL || SilSpFlat == NULL )
		Error_Handler();
	
	// Initialize frame count
	*num_frames = Proc_conf.calib_len;
	
	return CALIB_INITIATED;
}
Calib_status Calibrate (uint16_t *frame, uint32_t frame_num, Proc_stages *stages)
{
	//TODO: ARREGLAR CALIBRATION QUE ESTA MAL
	if ( frame_num < Proc_conf.calib_len)
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
	
	if ( frame_num < Proc_conf.calib_len-1)
		return CALIB_IN_PROCESS;
	else
		return CALIB_FINISH;
	
}
Calib_status endCalibration	(const bool save_calib_vars)
	{

	// Calculate Mean of Features
	arm_mean_f32 (SilEnergy, Proc_conf.calib_len, &SilEnergyMean);
	arm_mean_q31 ((q31_t*)SilFrecmax, Proc_conf.calib_len,(q31_t*) &SilFmaxMean);
	arm_mean_f32 (SilSpFlat, Proc_conf.calib_len, &SilSpFlatMean);

	// Calculate Deviation of Features
	arm_std_f32 (SilEnergy, Proc_conf.calib_len, &SilEnergyDev);
	arm_std_q31 ((q31_t*)SilFrecmax, Proc_conf.calib_len,(q31_t*) &SilFmaxDev);
	arm_std_f32 (SilSpFlat, Proc_conf.calib_len, &SilSpFlatDev);

	// Free memory
	vPortFree(SilEnergy);		SilEnergy = NULL;
	vPortFree(SilFrecmax);	SilFrecmax = NULL;
	vPortFree(SilSpFlat);		SilSpFlat = NULL;
	
	// Set Thresholds
	THD_E   = SilEnergyMean + Proc_conf.thd_scl_eng * SilEnergyDev;
	THD_FMX = SilFmaxMean > Proc_conf.thd_min_fmax ? SilFmaxMean : Proc_conf.thd_min_fmax;
	THD_SF  = fabs(SilSpFlatMean) + Proc_conf.thd_scl_sf * SilSpFlatDev;

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
#endif
//
//-----	INIT PROCESSING FUNCTIONS	-------
//
void initProcessing (Proc_conf *configuration, Proc_var *ptr_vars_buffers)
{	
	// Copio la configuración de procesamiento
	proc_conf = *configuration;
	
	// Alloco memoria para aux1 y aux2
	//TODO: EN REALIDAD TENGO QUE ALOCAR LA MEMORIA MAXIMA ENTRE (FRAME_LEN,FFT_LEN, etc)
	if( (aux1 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*aux1) ) ) == NULL )
		Error_Handler("Error on malloc aux1 in audio processing");
	if( (aux2 = pvPortMalloc( max(proc_conf.frame_len,proc_conf.fft_len) * sizeof(*aux2) ) ) == NULL )
		Error_Handler("Error on malloc aux2 in audio processing");
	
	// Armo el filtro de Preénfasis
	// Los coeficientes tienen que estar almacenados en tiempo invertido (leer documentación)
	if( (Pre_enfasis_Coeef = pvPortMalloc( proc_conf.numtaps * sizeof(*Pre_enfasis_Coeef) ) ) == NULL )
		Error_Handler("Error on malloc pre_enfasis_coeef in audio processing");
	Pre_enfasis_Coeef[0] = -proc_conf.alpha;
	Pre_enfasis_Coeef[1]= 1;
	
	// Alloco memoria para el state del filtro de Preénfasis
	if( (pState = pvPortMalloc( (proc_conf.numtaps + proc_conf.frame_overlap - 1) * sizeof(*pState) ) ) == NULL )
		Error_Handler("Error on malloc pstate in audio processing");
	arm_fir_init_f32 (&FirInst, proc_conf.numtaps, Pre_enfasis_Coeef, pState, proc_conf.frame_overlap);
	
	// Creo la ventana de Hamming
	if( (HamWin = pvPortMalloc(proc_conf.frame_len* sizeof(*HamWin))) == NULL)
		Error_Handler("Error on malloc HamWin in audio processing");
	Hamming_float(HamWin,proc_conf.frame_len);
	
	// Instance FFT
	if(arm_rfft_fast_init_f32 (&RFFTinst, proc_conf.fft_len) == ARM_MATH_ARGUMENT_ERROR)
		Error_Handler("Error on RFFT instance in audio processing");
	
	// Creo el Lifter (filtro de Cepstrum)
	if( (CepWeight = pvPortMalloc(proc_conf.lifter_length * sizeof(*CepWeight))) == NULL)
		Error_Handler("Error on malloc CepWeight in audio processing");
	Lifter_float (CepWeight,proc_conf.lifter_length);

	// Instance DCT
	if(dct2_init_f32(&DCTinst, &DCTinst_rfft, proc_conf.dct_len, proc_conf.dct_len/2, sqrt(2.0f/proc_conf.dct_len)) == ARM_MATH_ARGUMENT_ERROR)
		Error_Handler("Error on dct2 instance in audio processing");
	
	// Allocate variables
	if(ptr_vars_buffers != NULL)
	{
		allocateProcVariables (ptr_vars_buffers);
		vars_buffers = ptr_vars_buffers;
	}
}
void finishProcessing	(void)
{
	// Libero memoria
	vPortFree(aux1);										aux1 = NULL;
	vPortFree(aux2);										aux2 = NULL;
	vPortFree(Pre_enfasis_Coeef);				Pre_enfasis_Coeef = NULL;
	vPortFree(pState);									pState = NULL;
	vPortFree(frame_block);							frame_block = NULL;
	vPortFree(HamWin);									HamWin = NULL;
	vPortFree(CepWeight);								CepWeight = NULL;
	
	if(vars_buffers != NULL)
	{
		freeProcVariables(vars_buffers);	vars_buffers = NULL;
	}
}
void Hamming_float (float32_t *Hamming, uint32_t length)
{
	uint32_t i;
	const float32_t a=0.54 , b=0.46;
	
	for(i=0; i < length; i++) {
		Hamming[i] = a - b * arm_cos_f32 ((float32_t)2*PI*i/length);
	}
}
void Lifter_float (float32_t *Lifter, uint32_t length)
{
	
	float32_t theta;
	uint32_t n;
	
	for(n=0; n<length; n++) {
		theta = PI*n/(length-1);
		Lifter[n] = 1 + arm_sin_f32(theta) * length/2;
	}
}
//
//-----	PROCESSING FUNCTIONS	-------
//
void firstProcStage 	(float32_t *filt_signal, uint16_t *audio, Proc_var *saving_var)
{
	
	/* Convierto a Float y escalo */
	arm_q15_to_float((q15_t*)audio, aux1, proc_conf.frame_overlap);

	/* Se aplica un filtro de Pre-énfais al segmento de señal obtenida */	
	arm_fir_f32 (&FirInst, aux1, filt_signal, proc_conf.frame_overlap);

	// Copio las variables de ser necesario
	if(saving_var != NULL)
	{
		arm_copy_f32 (aux1, saving_var->speech, proc_conf.frame_overlap);
		arm_copy_f32 (filt_signal, saving_var->FltSig, proc_conf.frame_overlap);
	}
}

void secondProcStage	(float32_t *MagFFT, float32_t *frame_block, Proc_var *saving_var)
{
	
	/* Se le aplica la ventana de Hamming al segmento obtenido */
	arm_mult_f32 (frame_block, HamWin, aux1, proc_conf.frame_len);
	
	// Copio las variables de ser necesario
	if(saving_var != NULL)
		arm_copy_f32 (aux1, saving_var->WinSig, proc_conf.frame_len);
		
	/* Calculo la energía */
	arm_power_f32 (aux1, proc_conf.frame_len, &Energy);
	Energy /= proc_conf.frame_len;
	
	/* Se calcula la STFT */
	arm_rfft_fast_f32(&RFFTinst,aux1,aux2,0);
	aux2[0] += aux2[1];				//Sumo las componentes reales

	/* Calculo el módulo de la FFT */
	arm_cmplx_mag_squared_f32  (aux2, MagFFT, proc_conf.fft_len/2);

	// Copio las variables de ser necesario
	if(saving_var != NULL)
	{
		arm_copy_f32 (aux2, saving_var->STFTWin, proc_conf.frame_len);
		arm_copy_f32 (MagFFT, saving_var->MagFFT,  proc_conf.fft_len/2);
	}
}
void thirdProcStage 	(float32_t *MFCC, float32_t *MagFFT, Proc_var *saving_var)
{
	
	/* Se pasa el espectro por los filtros del banco de Mel y se obtienen los coeficientes */
	arm_mat_init_f32 (&MagFFTMtx, proc_conf.fft_len/2, 1, MagFFT);																				// Se convierte la STFT a una Matriz de filas=fftLen y columnas=1
	arm_mat_init_f32 (&MelWinMtx, proc_conf.mel_banks, 1, aux1);																				// Se crea una matriz para almacenar el resultado
	if(arm_mat_mult_f32(&MelFilt, &MagFFTMtx, &MelWinMtx) == ARM_MATH_SIZE_MISMATCH)
		Error_Handler("Error on thirdProcStage");
	// MelFilt[MEL_BANKS,proc_conf.fft_len] * MagFFTMtx[proc_conf.fft_len,1] = MelWinMtx [MEL_BANKS,1]
	
	/* Se obtienen los valores logaritmicos de los coeficientes */
	arm_fill_f32 	(0, aux2, proc_conf.dct_len);
	for (uint32_t i=0; i<proc_conf.mel_banks; i++)
		aux2[i] = log10f(aux1[i]);
	
	// Copio las variables
	if(saving_var != NULL)
	{
		arm_copy_f32 (aux1, saving_var->MelWin, proc_conf.mel_banks);
		arm_copy_f32 (aux2, saving_var->LogWin, proc_conf.dct_len);
	}
	
	/* Se Anti-transforma aplicando la DCT-II */
	ale_dct2_f32(&DCTinst, aux1, aux2);
	
	/* Se pasa la señal por un filtro en el campo Cepstral */
	arm_mult_f32 (aux2, CepWeight, MFCC, proc_conf.lifter_length);

	// Copio las variables
	if(saving_var != NULL)
	{
		arm_copy_f32 (aux2, saving_var->CepWin, proc_conf.dct_len/2);
		arm_copy_f32 (MFCC, saving_var->CepWin, proc_conf.lifter_length);
	}
	
}
void VADFeatures			(VAD_var *vad, float32_t *MagFFT, float32_t Energy)
{
	uint32_t Index;
	float32_t AM,GM;
	
	/* Obtengo la Fmax */
	arm_max_f32 (MagFFT, proc_conf.fft_len/2, &aux, &Index);									// no me interesa el valor máximo, sino en que lugar se encuentra
	Frecmax = Index;
	
	/* Calculo el Spectral Flatness */
	arm_mean_f32 (MagFFT, proc_conf.fft_len/2, &AM);	// Arithmetic Mean
	sumlog(MagFFT, &GM, proc_conf.fft_len/2);
	GM = expf(GM/(proc_conf.fft_len/2));													// Geometric Mean
	SpFlat = 10*log10f(GM/AM);
	
	vad->energy = Energy;
	vad->fmax = Frecmax;
	vad->energy = SpFlat;
}
void deltaCoeff				(float32_t *output, float32_t *input)
{
	int step = proc_conf.lifter_length+1;
	
	for(int i=0; i < proc_conf.lifter_length+1; i++)
		output[i] = ( (input[3*step+i] - input[1*step+i]) + 2 * (input[4*step+i] - input[0*step+i]) ) / 10;
}
//
//----- MISCELLANEOUS FUNCTIONS -----
//
void allocateProcVariables (Proc_var *var)
{
	var->speech		= pvPortMalloc(proc_conf.frame_overlap * sizeof(*(var->speech)));		// Señal de audio escalada
	var->FltSig		= pvPortMalloc(proc_conf.frame_overlap * sizeof(*(var->FltSig)));		// Señal de audio pasada por el Filtro de Pre-Enfasis
	
	var->WinSig		= pvPortMalloc(proc_conf.frame_len * sizeof(*(var->WinSig)));		// Señal de filtrada multiplicada por la ventana de Hamming
	
	var->STFTWin	= pvPortMalloc(proc_conf.fft_len   * sizeof(*(var->STFTWin)));	// Transformada de Fourier en Tiempo Corto
	var->MagFFT		= pvPortMalloc(proc_conf.fft_len/2 * sizeof(*(var->MagFFT)));		// Módulo del espectro
	
	var->MelWin		= pvPortMalloc(proc_conf.mel_banks * sizeof(*(var->MelWin)));		// Espectro pasado por los filtros de Mel
	
	var->LogWin		= pvPortMalloc(proc_conf.dct_len  * sizeof(*(var->LogWin)));		// Logaritmo del espectro filtrado
	var->CepWin		= pvPortMalloc(proc_conf.dct_len/2* sizeof(*(var->CepWin)));		// Señal cepstral
	
	if(var->speech == NULL || var->FltSig == NULL || var->WinSig == NULL || var->STFTWin == NULL || 
		 var->MagFFT == NULL || var->MelWin == NULL || var->LogWin == NULL || var->CepWin == NULL)
		Error_Handler("Error on malloc of proc variables");
}
void freeProcVariables (Proc_var *var)
{
	vPortFree(var->speech);				var->speech = NULL;
	vPortFree(var->FltSig);				var->FltSig = NULL;
	vPortFree(var->WinSig);				var->WinSig = NULL;
	vPortFree(var->STFTWin);			var->STFTWin = NULL;
	vPortFree(var->MagFFT);				var->MagFFT = NULL;
	vPortFree(var->MelWin);				var->MelWin = NULL;
	vPortFree(var->LogWin);				var->LogWin = NULL;
	vPortFree(var->CepWin);				var->CepWin = NULL;
}
uint8_t Open_proc_files (Proc_files *files, const bool vad)
{
	
	UINT byteswritten;
	
	// Escribo un archivo con los coeficientes de la ventana de Hamming			
	if(f_open(&files->HamWinFile, "HamWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)Error_Handler("Error on HamWinFile open");
	if(f_write(&files->HamWinFile, HamWin, proc_conf.frame_len * sizeof(*HamWin), &byteswritten) != FR_OK)	Error_Handler("Error on HamWinFile write");
	f_close(&files->HamWinFile);

	// Escribo un archivo con los coeficientes del Filtro Cepstral
	if(f_open(&files->CepWeiFile, "CepWei.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on CepWeiFile open");
	if(f_write(&files->CepWeiFile, CepWeight, proc_conf.lifter_length * sizeof(*CepWeight), &byteswritten) != FR_OK)	Error_Handler("Error on CepWeiFile write");
	f_close(&files->CepWeiFile);
					
	//Abro los archivos
	if(f_open(&files->SpeechFile, "Speech.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on Speech open");
	if(f_open(&files->FltSigFile, "FltSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on FltSig open");
	if(f_open(&files->FrameFile, 	"Frames.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on Frames open");
	if(f_open(&files->WinSigFile, "WinSig.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on WinSig open");
	if(f_open(&files->STFTWinFile,"FFTWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on FFTWin open");
	if(f_open(&files->MagFFTFile, "MagFFT.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on MagFFT open");
	if(f_open(&files->MelWinFile, "MelWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on MelWin open");
	if(f_open(&files->LogWinFile, "LogWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on LogWin open");
	if(f_open(&files->CepWinFile, "CepWin.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on CepWin open");

	if (vad)
	{
		if(f_open(&files->VADFile,			 "VAD.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on VAD open");
		if(f_open(&files->EnerFile,		"Energy.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on Energy open");
		if(f_open(&files->FrecFile,  "FrecMax.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on FrecMax open");
		if(f_open(&files->SFFile,		"SpecFlat.bin",	FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler("Error on SpecFlat open");
	}

//	if(f_open(&files->nosirve, "nosirve.bin", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)		Error_Handler();

	return 0;
}
uint8_t Append_proc_files (Proc_files *files, const Proc_var *data, const bool vad, Proc_stages stage)
{
	UINT byteswritten;
	
	//Escribo los valores intermedios en un archivo
	
	if (stage & First_Stage)
	{
		if(f_write(&files->SpeechFile,  data->speech, proc_conf.frame_overlap * sizeof(*(data->speech)),  &byteswritten) != FR_OK)	Error_Handler("Error on SpeechFile write");
		if(f_write(&files->FltSigFile,  data->FltSig, proc_conf.frame_overlap * sizeof(*(data->FltSig)),  &byteswritten) != FR_OK)	Error_Handler("Error on FltSigFile write");
	}
	
	if(stage & Second_Stage)
	{
		if(f_write(&files->FrameFile,  	frame_block,  proc_conf.frame_len * sizeof(*frame_block),  		&byteswritten) != FR_OK)	Error_Handler("Error on FrameFile write");
		if(f_write(&files->WinSigFile,  data->WinSig, proc_conf.frame_len * sizeof(*(data->WinSig)),  &byteswritten) != FR_OK)	Error_Handler("Error on WinSigFile write");
		if(f_write(&files->STFTWinFile, data->STFTWin,proc_conf.fft_len		* sizeof(*(data->STFTWin)), &byteswritten) != FR_OK)	Error_Handler("Error on STFTWinFile write");
		if(f_write(&files->MagFFTFile,  data->MagFFT, proc_conf.fft_len/2 * sizeof(*(data->MagFFT)),  &byteswritten) != FR_OK)	Error_Handler("Error on MagFFTFile write");
	}
	
	if(stage & Third_Stage)
	{
		if(f_write(&files->MelWinFile,  data->MelWin, proc_conf.mel_banks * sizeof(*(data->MelWin)),  &byteswritten) != FR_OK)	Error_Handler("Error on MelWinFile write");
		if(f_write(&files->LogWinFile,  data->LogWin, proc_conf.dct_len   *	sizeof(*(data->LogWin)),  &byteswritten) != FR_OK)	Error_Handler("Error on LogWinFile write");
		if(f_write(&files->CepWinFile,  data->CepWin, proc_conf.dct_len/2 * sizeof(*(data->CepWin)),  &byteswritten) != FR_OK)	Error_Handler("Error on CepWinFile write");
	}
	
	if(vad)
	{
		if(f_write(&files->VADFile,	&data->VAD,	sizeof(data->VAD),&byteswritten) != FR_OK)	Error_Handler("Error on VADFile write");
		if(f_write(&files->EnerFile,&Energy,		sizeof(Energy),		&byteswritten) != FR_OK)	Error_Handler("Error on EnerFile write");
		if(f_write(&files->FrecFile,&Frecmax,		sizeof(Frecmax),	&byteswritten) != FR_OK)	Error_Handler("Error on FrecFile write");
		if(f_write(&files->SFFile,	&SpFlat,		sizeof(SpFlat),		&byteswritten) != FR_OK)	Error_Handler("Error on SFFile write");
	}
	return 1;
}
uint8_t Close_proc_files (Proc_files *files, const bool vad)
{
		
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



////	ale_dct2_instance_f32 			dct2_instance;
////	arm_rfft_fast_instance_f32 	rfft_instance;
////	float32_t pState[64];
////
//// Los valores fueron obtenidos mediante la equación x = 50*cos((1:32)*2*pi/40);
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
