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

#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#define DEBUG	1

#define CONFIG_FILE_NAME				"Config.ini"
#define PATERN_MAX_NAME_SIZE		20
#define AUDIO_FILE_EXTENSION		".wav"

/*---------------------------------------------------------------------------------
								DEFAULT APPLICATION CONFIGURATION PARAMETERS
-----------------------------------------------------------------------------------*/

#define AUDIO_FRAME_SIZE              512
#define AUDIO_RING_BUFF_SIZE          4
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
#define FRAME_OVERLAP									170 // (AUDIO_IN_FREQ/1000)*TIME_OVERLAP
#define FRAME_NET											FRAME_LEN - FRAME_OVERLAP

#define NUMTAPS												2
#define ALPHA													0.95f
#define FFT_LEN												512
#define MEL_BANKS											40
#define DCT_LEN												32			// Tiene que ser 2^N y mayor a MEL_BANKS
#define LIFTER_LENGTH									12
#define THETA													2

// VAD
#define	VAD_ENABLE										true
#define	AGE_THD												3
#define	TIMEOUT_THD										3

#define THD_Scl_ENERGY								2
#define THD_Scl_SF										0.5f
#define THD_min_FMAX									500
#define THD_max_FMAX									3500

#define PDM_BUFF_SIZE                 AUDIO_IN_FREQ/1000*AUDIO_IN_DECIMATOR*AUDIO_IN_CHANNEL_NBR/8
#define PCM_BUFF_SIZE                 AUDIO_IN_FREQ/1000*AUDIO_IN_CHANNEL_NBR
#define EXTEND_BUFF										32 // FRAME_LEN/PCM_BUFF_SIZE

#define CALIB_TIME										10 //segundos
#define CALIB_LEN 										CALIB_TIME*AUDIO_IN_FREQ/FRAME_LEN

#define RECO_WIDTH										10

#define PAT_DIR												"Patterns"
#define PAT_CONF_FILE									"patterns.ini"

#define PAT_NAME											""
#define	PAT_NUM												0

/*---------------------------------------------------------------------------------*/


#endif  // APPLICATION_CONFIG_H
