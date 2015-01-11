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

#ifndef _APPLICATION_CONFIG_H
#define _APPLICATION_CONFIG_H

#define DEBUG	1
#define _VAD_	1

#define CONFIG_FILE_NAME				"Config.ini"
#define PATERN_MAX_NAME_SIZE		8


/*---------------------------------------------------------------------------------
								DEFAULT CONFIGURATION PARAMETERS OF THE APPLICATION
-----------------------------------------------------------------------------------*/

#define AUDIO_IN_FREQ                 16000		//[Hz]
#define AUDIO_IN_BW_HIGH							8000		//[Hz]
#define AUDIO_IN_BW_LOW								10			//[Hz]
#define AUDIO_IN_BIT_RESOLUTION       16
#define AUDIO_IN_CHANNEL_NBR          1 			// Mono = 1, Stereo = 2
#define AUDIO_IN_VOLUME               64
#define AUDIO_IN_DECIMATOR						64

#define TIME_WINDOW										25			// [miliseconds]
#define TIME_OVERLAP									10			// [miliseconds]

#define FRAME_LEN											512	// (int) 2^(AUDIO_IN_FREQ/1000*(TIME_WINDOW))
#define FRAME_OVERLAP									160 // (AUDIO_IN_FREQ/1000)*TIME_OVERLAP
#define FRAME_NET											FRAME_LEN - FRAME_OVERLAP

#define NUMTAPS												2
#define ALPHA													0.95f
#define FFT_LEN												512
#define MEL_BANKS											20
#define IFFT_LEN											64			// Tiene que ser mayor a 2*MEL_BANKS
#define LIFTER_LEGNTH									13

#define PDM_BUFF_SIZE                 AUDIO_IN_FREQ/1000*AUDIO_IN_DECIMATOR*AUDIO_IN_CHANNEL_NBR/8
#define PCM_BUFF_SIZE                 AUDIO_IN_FREQ/1000*AUDIO_IN_CHANNEL_NBR
#define EXTEND_BUFF										32 // FRAME_LEN/PCM_BUFF_SIZE

#define CALIB_TIME										10 //segundos
#define CALIB_LEN 										CALIB_TIME*AUDIO_IN_FREQ/FRAME_LEN

#define CALIB_THD_ENERGY							2
#define CALIB_THD_FRECLOW							500
#define CALIB_THD_FRECHIGH						3500
#define CALIB_THD_SF									0.5f

#define PAT_DIR												"Pat"
#define PAT_CONF_FILE									"Pat.ini"

#define PAT_NAME											""
#define	PAT_NUM												0

/*---------------------------------------------------------------------------------*/


#endif  // _APPLICATION_CONFIG_H
