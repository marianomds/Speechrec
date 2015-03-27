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
#include "recognition.h"


float32_t dtw_reduce (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path){

	// Assert error
	if( a->numCols != b->numCols )
		Error_Handler();

	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
	float32_t aux , past[3];
	float32_t cost;
	const uint16_t width=10;
	float32_t *dtw_mtx;
	float32_t result = FLT_MAX;
	
	
	// Length of time series
	uint16_t costmtxrows = b->numRows;
	uint16_t costmtxcols = a->numRows;
	uint16_t params      = b->numCols; // == a->numCols;
	uint8_t reduce_rows  = 2;
	
	// Allocate memeory for matrices used for calculation	
	if( (dtw_mtx = pvPortMalloc( reduce_rows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)					Error_Handler();
	
	// Start at (0,0) and initialized everithing in inf
	dtw_mtx[0] = 0;
	for( i=1 ; i < reduce_rows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;
	
	
	// Get distance matrix
	for( i=1 ; i < costmtxrows ; i++ )
	{
		// No lo hago para el primer caso
		if(i != 1)
		{
			// Desplazo las reduce_rows-1 últimas filas al principio y seteo la nueva última fila a infinito
			memcpy(&dtw_mtx[0] , &dtw_mtx[costmtxcols] , costmtxcols * (reduce_rows-1));
			for( j=0 ; j < costmtxcols ; j++ )
				dtw_mtx[ (reduce_rows-1) * costmtxcols + j ] = FLT_MAX;
		}
		
		for( j = max(1,i-width) ; j < min(costmtxcols,i+width) ; j++ )
		{
			// Get distance
			cost = dist( &(a->pData[j*params]) , &(b->pData[i*params]) , params);
			
			// Calculate indexes
			idx 					=		(reduce_rows-1)	*	costmtxcols + 	j		;
			idx_diagonal	= (reduce_rows-1-1)	* costmtxcols + (j-1)	;
			idx_vertical	= (reduce_rows-1-1) * costmtxcols +   j  	;
			idx_horizontal=   (reduce_rows-1) * costmtxcols + (j-1)	;
			
			// Calculate arrive to node through:
			past[DIAGONAL]   = dtw_mtx[idx_diagonal];			// diagonal line
			past[VERTICAL] 	 = dtw_mtx[idx_vertical];			// vertical line
			past[HORIZONTAL] = dtw_mtx[idx_horizontal];		// horizontal line
			
			// Calculo el minimo de todos
			arm_min_f32 (past, sizeof(past), &aux, &path_idx);

			// Calculo la distancia a ese punto
			dtw_mtx[idx] = cost + aux;
		}
	}		
	
	// Free memory
	vPortFree(dtw_mtx);

	// Return distance value
	result = dtw_mtx[ reduce_rows * costmtxcols];
	return result;
}
float32_t dtw (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path){

	// Assert error
	if( a->numCols != b->numCols )
		Error_Handler();

	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
	float32_t aux , past[3];
	float32_t cost;
	const uint16_t width=10;
	float32_t *dtw_mtx;
	uint32_t	*whole_path;
	
	// Length of time series
	uint16_t costmtxcols = a->numRows;
	uint16_t costmtxrows = b->numRows;
	uint16_t params      = b->numCols; // == a->numCols;
	
	// Allocate memeory for matrices used for calculation	
	if( (dtw_mtx = pvPortMalloc( costmtxrows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)					Error_Handler();
	if( (whole_path = pvPortMalloc( costmtxrows * costmtxcols * sizeof(*whole_path) ) ) == NULL);		Error_Handler();
	
	// Initialized everithing in inf
	for( i=0 ; i < costmtxrows ; i++ )
		for( j=0 ; j < costmtxcols ; j++ )
			dtw_mtx[ i * costmtxcols + j ] = FLT_MAX;
	
	// Start at (0,0)
	dtw_mtx[0] = 0;
	
	// Get distance matrix
	for( i=1 ; i < costmtxrows ; i++ )
	{	
		for( j = max(1,i-width) ; j < min(costmtxcols,i+width) ; j++ )
		{
			// Get distance
			cost = dist( &(a->pData[i*params]) , &(b->pData[j*params]) , params);
			
			// Calculate indexes
			idx 					=		i		*	costmtxcols + 	j		;
			idx_diagonal	= (i-1) * costmtxcols + (j-1)	;
			idx_vertical	= (i-1) * costmtxcols +   j  	;
			idx_horizontal=   i   * costmtxcols + (j-1)	;
			
			// Calculate arrive to node through:
			past[DIAGONAL]   = dtw_mtx[idx_diagonal];			// diagonal line
			past[VERTICAL] 	 = dtw_mtx[idx_vertical];			// vertical line
			past[HORIZONTAL] = dtw_mtx[idx_horizontal];		// horizontal line
			
			// Calculo el minimo de todos
			arm_min_f32 (past, sizeof(past), &aux, &path_idx);

			// Calculo la distancia a ese punto
			dtw_mtx[idx] = cost + aux;
			
			// Save path to that node if necesary
			if (path != NULL) {
				switch(path_idx){
					case DIAGONAL:
						whole_path[idx] = idx_diagonal;
						break;
					
					case VERTICAL:
						whole_path[idx] = idx_vertical;
						break;
					
					case HORIZONTAL:
						whole_path[idx] = idx_horizontal;
						break;
					
					default:
						break;
				}
			}
		}
	}		
	if(path!=NULL){
		path[0] = whole_path[costmtxrows*costmtxcols];
		for(i=1 ; i < costmtxrows + costmtxcols ; i++)
			path[i] = whole_path[ path[i-1] ];
	}
	
	// Free memory
	vPortFree(dtw_mtx);
	vPortFree(whole_path);
	
	// Return distance value
	return dtw_mtx[costmtxrows*costmtxcols];
}

float32_t dist (float32_t *pSrcA, float32_t *pSrcB, uint16_t blockSize){
	float32_t *pDst;
	float32_t pResult;
	
	// Allocate memory
	pDst = pvPortMalloc(blockSize * sizeof(*pDst));
	
	// Calculo la distancia euclidia
	arm_sub_f32 (pSrcA, pSrcB, pDst, blockSize);		// pDst[n] = pSrcA[n] - pSrcB[n]
	arm_rms_f32 (pDst, blockSize, &pResult);				// sqrt(pDst[1]^2 + pDst[2]^2 + ... + pDst[n]^2)
	
	// Free Memory
	vPortFree(pDst);
	
	return pResult;
}
/*Function to find minimum of x and y*/
int min(int x, int y)
{
  return y ^ ((x ^ y) & -(x < y));
}
 
/*Function to find maximum of x and y*/
int max(int x, int y)
{
  return x ^ ((x ^ y) & -(x < y));
}
