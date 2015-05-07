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
