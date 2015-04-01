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

char dist_file_name[] = "DMtx_00.bin";

char *updateFilename_ahora (char *Filename) {
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
float32_t dtw_reduce (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, uint16_t *path){

	// Assert error
	if( a->numCols != b->numCols )
		Error_Handler();

	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
//	float32_t aux , past[3];
	float32_t cost;
	uint16_t width=10;
	float32_t *dtw_mtx;
	float32_t result = FLT_MAX;
	FIL dist_mtx_file;
	uint32_t byteswrite;
	
	// Length of time series
	uint16_t costmtxrows = b->numRows + 1;
	uint16_t costmtxcols = a->numRows + 1;
	uint16_t params      = b->numCols; // == a->numCols;
	uint8_t reduce_rows  = 2;
	
	uint32_t dtw_mtx_size = reduce_rows * costmtxcols;
	uint32_t idx_dtw_mtx_last_row = (reduce_rows-1) * costmtxcols;
	
	// Abro un archivo para salvar la matriz de distancia
	for(; f_stat(dist_file_name,NULL)!= FR_NO_FILE; updateFilename_ahora(dist_file_name));
	f_open(&dist_mtx_file, dist_file_name ,FA_WRITE | FA_OPEN_ALWAYS);
	
	// Adapt width
	width = (uint16_t) max(width, abs(costmtxrows-costmtxcols));
	
	// Allocate memeory for matrices used for calculation	
	if( (dtw_mtx = pvPortMalloc( reduce_rows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)					Error_Handler();
		
	// Start at (0,0) and initialized everithing in inf. I put costmtxcols, because it will be moved
	dtw_mtx[costmtxcols] = 0;
	for( i=costmtxcols+1 ; i < reduce_rows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;
	
	
	// Go through rows
	for( i=1 ; i < costmtxrows ; i++ )
	{
		// Escribo la nueva última fila
		f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite);
		
		// Shifteo una fila hacia arriba y seteo la nueva última fila a infinito
		memcpy(&dtw_mtx[0] , &dtw_mtx[costmtxcols] , (dtw_mtx_size - costmtxcols) * sizeof(*dtw_mtx));
		for( j=idx_dtw_mtx_last_row ; j < dtw_mtx_size ; j++ )
			dtw_mtx[j] = FLT_MAX;
		
		// Go through columns
		for( j = max(1,i-width) ; j < min(costmtxcols,i+width) ; j++ )
		{
			// Get distance
			cost = dist( &(a->pData[(j-1)*params]) , &(b->pData[(i-1)*params]) , params);
			
			// Calculate indexes
			idx 					=								idx_dtw_mtx_last_row + 	j		;
			idx_diagonal	= idx_dtw_mtx_last_row - costmtxcols + (j-1);
			idx_vertical	= idx_dtw_mtx_last_row - costmtxcols +   j  ;
			idx_horizontal= 							idx_dtw_mtx_last_row + (j-1);
			
//			idx 					=		(reduce_rows-1)	*	costmtxcols + 	j		;
//			idx_diagonal	= (reduce_rows-1-1)	* costmtxcols + (j-1)	;
//			idx_vertical	= (reduce_rows-1-1) * costmtxcols +   j  	;
//			idx_horizontal=   (reduce_rows-1) * costmtxcols + (j-1)	;
//			
			// Calculate arrive to node through:
//			past[DIAGONAL]   = dtw_mtx[idx_diagonal];			// diagonal line
//			past[VERTICAL] 	 = dtw_mtx[idx_vertical];			// vertical line
//			past[HORIZONTAL] = dtw_mtx[idx_horizontal];		// horizontal line
			
			// Calculo el minimo de todos
//			arm_min_f32 (past, sizeof(past), &aux, &path_idx);

			if(dtw_mtx[idx_diagonal] < dtw_mtx[idx_vertical])
				if(dtw_mtx[idx_diagonal] < dtw_mtx[idx_horizontal])
					path_idx = idx_diagonal;
				else
					path_idx = idx_horizontal;
			else
				if(dtw_mtx[idx_vertical] < dtw_mtx[idx_horizontal])
					path_idx = idx_vertical;
				else
					path_idx = idx_horizontal;
				
				
			// Calculo la distancia a ese punto
			dtw_mtx[idx] = cost + dtw_mtx[path_idx];
		}
	}		
	
	// Escribo la última fila
	f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite);
	
	// Cierro el archivo
	f_close(&dist_mtx_file);

	// Free memory
	vPortFree(dtw_mtx);

	// Return distance value
	result = dtw_mtx[ dtw_mtx_size - 1 ];
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
	float32_t *pDst,power,result;
	
	// Allocate memory
	pDst = pvPortMalloc(blockSize * sizeof(*pDst));
	
	// Calculo la distancia euclidia
	arm_sub_f32 (pSrcA, pSrcB, pDst, blockSize);		// pDst[n] = pSrcA[n] - pSrcB[n]
	arm_power_f32	(pDst, blockSize, &power); 				// pDst[1]^2 + pDst[2]^2 + ... + pDst[n]^2
	arm_sqrt_f32 	(power,&result);									// sqrt(power)
	
	// Free Memory
	vPortFree(pDst);
	
	return result;
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
