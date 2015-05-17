/**
  ******************************************************************************
  * @file    msic.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    06-Mayo-2015
  * @brief   Some miscellaneous functions
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/

/* Includes ------------------------------------------------------------------*/
#include "misc.h"

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
FRESULT open_append (FIL* fp, const char* path) {
	FRESULT fr;

	/* Opens an existing file. If not exist, creates a new file. */
	fr = f_open(fp, path, FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
			/* Seek to end of the file to append data */
			fr = f_lseek(fp, f_size(fp));
			if (fr != FR_OK)
					f_close(fp);
	}
	return fr;
}
//---------------------------------------
//						MATH FUNCTIONS
//---------------------------------------
void arm_diff_f32 (float32_t *pSrc, float32_t *pDst, uint32_t blockSize) {
	// pDst[n] = pSrc[n] - pSrc[n-1]
		arm_sub_f32 (pSrc, &pSrc[1], pDst, blockSize-1);
		arm_negate_f32 (pDst, pDst, blockSize-1);
}
void cumsum (float32_t *pSrc, float32_t *pDst, uint32_t blockSize) {
	
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
void sumlog (float32_t *pSrc, float32_t *pDst, uint32_t blockSize) {
	
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
int min(int x, int y) {
  return y ^ ((x ^ y) & -(x < y));
}
int max(int x, int y) {
  return x ^ ((x ^ y) & -(x < y));
}


//---------------------------------------
//						WAVE FILE FUNCTIONS
//---------------------------------------

FRESULT newWavFile (char *Filename, WAVE_FormatTypeDef* WaveFormat, FIL *WavFile, WAVE_Audio_config audio_config) {
	
	FRESULT res;
	uint32_t byteswritten;
	
	// Open File
	res = f_open(WavFile, Filename, FA_CREATE_ALWAYS | FA_WRITE);

	/* If Error */
	if(res != FR_OK)
		return res;
		
	/* Initialized Wave Header File */
	WaveProcess_EncInit(WaveFormat,audio_config);
	
	/* Write data to the file */
	res = f_write(WavFile, WaveFormat->pHeader, 44, (void*) &byteswritten);
	
	/* Error when trying to write file*/
	if(res != FR_OK)
		f_close(WavFile);
	
	return res;
}

FRESULT closeWavFile (FIL *WavFile, WAVE_FormatTypeDef* WaveFormat, uint32_t audio_size){
	FRESULT res;
	uint32_t byteswritten;
	
	WaveProcess_HeaderUpdate(WaveFormat, audio_size);
	f_lseek(WavFile, 0);				
	f_write(WavFile, WaveFormat->pHeader, 44, (void*)&byteswritten);
	res = f_close(WavFile);
	
	return res;
}
uint32_t WaveProcess_EncInit(WAVE_FormatTypeDef* WaveFormat, WAVE_Audio_config audio_config) {  
	/* Initialize the encoder structure */
	WaveFormat->SampleRate = audio_config.SampleRate;        					/* Audio sampling frequency */
	WaveFormat->NbrChannels = audio_config.NbrChannels;       	/* Number of channels: 1:Mono or 2:Stereo */
	WaveFormat->BitPerSample = audio_config.BitPerSample;  	 	/* Number of bits per sample (16, 24 or 32) */
	WaveFormat->FileSize = 0x001D4C00;    											/* Total length of useful audio data (payload) */
	WaveFormat->SubChunk1Size = 44;       											/* The file header chunk size */
	WaveFormat->ByteRate = (WaveFormat->SampleRate * \
												(WaveFormat->BitPerSample/8) * \
												 WaveFormat->NbrChannels);        		/* Number of bytes per second  (sample rate * block align)  */
	WaveFormat->BlockAlign = WaveFormat->NbrChannels * \
													(WaveFormat->BitPerSample/8);   		/* channels * bits/sample / 8 */
	
	/* Parse the Wave file header and extract required information */
  if(WaveProcess_HeaderInit(WaveFormat))
    return 1;
	
  return 0;
}
uint32_t WaveProcess_HeaderInit(WAVE_FormatTypeDef* pWaveFormatStruct) {

/********* CHUNK DESCRIPTOR *********/	
	/* Write chunkID. Contains the letters "RIFF"	in ASCII form  ------------------------------------------*/
  pWaveFormatStruct->pHeader[0] = 'R';
  pWaveFormatStruct->pHeader[1] = 'I';
  pWaveFormatStruct->pHeader[2] = 'F';
  pWaveFormatStruct->pHeader[3] = 'F';

  /* Write the file length. This is the size of the entire file in bytes minus 8 bytes for the two
	fields not included in this count: ChunkID and ChunkSize. ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the recording opearation. 
	Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pWaveFormatStruct->pHeader[4] = 0x00;
  pWaveFormatStruct->pHeader[5] = 0x4C;
  pWaveFormatStruct->pHeader[6] = 0x1D;
  pWaveFormatStruct->pHeader[7] = 0x00;
	
  /* Write the file format, must be 'Wave'. Contains the letters "WaveE" -----------------------------------*/
  pWaveFormatStruct->pHeader[8]  = 'W';
  pWaveFormatStruct->pHeader[9]  = 'A';
  pWaveFormatStruct->pHeader[10] = 'V';
  pWaveFormatStruct->pHeader[11] = 'E';



/********* SUB-CHUNK DESCRIPTOR N°1 *********/	
  /* Write the format chunk, must be'fmt ' -----------------------------------*/
  pWaveFormatStruct->pHeader[12]  = 'f';
  pWaveFormatStruct->pHeader[13]  = 'm';
  pWaveFormatStruct->pHeader[14]  = 't';
  pWaveFormatStruct->pHeader[15]  = ' ';

  /* Write the length of the 'fmt' data (16 for PCM).
	This is the size of the rest of the Subchunk which follows this number. ------------------------*/
  pWaveFormatStruct->pHeader[16]  = 0x10;
  pWaveFormatStruct->pHeader[17]  = 0x00;
  pWaveFormatStruct->pHeader[18]  = 0x00;
  pWaveFormatStruct->pHeader[19]  = 0x00;

  /* Write the audio format. PCM = 1 ==> Linear quantization
	Values other than 1 indicate some form of compression. ------------------------------*/
  pWaveFormatStruct->pHeader[20]  = 0x01;
  pWaveFormatStruct->pHeader[21]  = 0x00;

  /* Write the number of channels (Mono = 1, Stereo = 2). ---------------------------*/
  pWaveFormatStruct->pHeader[22]  = pWaveFormatStruct->NbrChannels;
  pWaveFormatStruct->pHeader[23]  = 0x00;

  /* Write the Sample Rate in Hz ---------------------------------------------*/
  /* Write Little Endian ie. 8000 = 0x00001F40 => byte[24]=0x40, byte[27]=0x00*/
  pWaveFormatStruct->pHeader[24]  = (uint8_t)((pWaveFormatStruct->SampleRate & 0xFF));
  pWaveFormatStruct->pHeader[25]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 8) & 0xFF);
  pWaveFormatStruct->pHeader[26]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 16) & 0xFF);
  pWaveFormatStruct->pHeader[27]  = (uint8_t)((pWaveFormatStruct->SampleRate >> 24) & 0xFF);

  /* Write the Byte Rate
	==> SampleRate * NumChannels * BitsPerSample/8	-----------------------------------------------------*/
  pWaveFormatStruct->pHeader[28]  = (uint8_t)((pWaveFormatStruct->ByteRate & 0xFF));
  pWaveFormatStruct->pHeader[29]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 8) & 0xFF);
  pWaveFormatStruct->pHeader[30]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 16) & 0xFF);
  pWaveFormatStruct->pHeader[31]  = (uint8_t)((pWaveFormatStruct->ByteRate >> 24) & 0xFF);

  /* Write the block alignment 
	==> NumChannels * BitsPerSample/8 -----------------------------------------------*/
  pWaveFormatStruct->pHeader[32]  = pWaveFormatStruct->BlockAlign;
  pWaveFormatStruct->pHeader[33]  = 0x00;

  /* Write the number of bits per sample -------------------------------------*/
  pWaveFormatStruct->pHeader[34]  = pWaveFormatStruct->BitPerSample;
  pWaveFormatStruct->pHeader[35]  = 0x00;



/********* SUB-CHUNK DESCRIPTOR N°2 *********/	
  /* Write the Data chunk. Contains the letters "data" ------------------------------------*/
  pWaveFormatStruct->pHeader[36]  = 'd';
  pWaveFormatStruct->pHeader[37]  = 'a';
  pWaveFormatStruct->pHeader[38]  = 't';
  pWaveFormatStruct->pHeader[39]  = 'a';

  /* Write the number of sample data. This is the number of bytes in the data.
	==> NumSamples * NumChannels * BitsPerSample/8  -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pWaveFormatStruct->pHeader[40]  = 0x00;
  pWaveFormatStruct->pHeader[41]  = 0x4C;
  pWaveFormatStruct->pHeader[42]  = 0x1D;
  pWaveFormatStruct->pHeader[43]  = 0x00;
  
  /* Return 0 if all operations are OK */
  return 0;
}
uint32_t WaveProcess_HeaderUpdate(WAVE_FormatTypeDef* pWaveFormatStruct, uint32_t adudio_size) {
  /* Write the file length ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the 
   recording opearation.  Example: 661500 Btyes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pWaveFormatStruct->pHeader[4] = (uint8_t)(adudio_size+36);
  pWaveFormatStruct->pHeader[5] = (uint8_t)((adudio_size+36) >> 8);
  pWaveFormatStruct->pHeader[6] = (uint8_t)((adudio_size+36) >> 16);
  pWaveFormatStruct->pHeader[7] = (uint8_t)((adudio_size+36) >> 24);
  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pWaveFormatStruct->pHeader[40] = (uint8_t)(adudio_size); 
  pWaveFormatStruct->pHeader[41] = (uint8_t)(adudio_size >> 8);
  pWaveFormatStruct->pHeader[42] = (uint8_t)(adudio_size >> 16);
  pWaveFormatStruct->pHeader[43] = (uint8_t)(adudio_size >> 24); 
  /* Return 0 if all operations are OK */
  return 0;
}


