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
#include "misc.h"
#include "float.h"
#include "error_handler.h"
#include <ale_stdlib.h>


// Cálculo del log-likelihood para una secuencia de vectores de observaciones usando un 
// modelo con mezcla de Gausianas
float32_t Tesis_loglik(float32_t * data, uint16_t T, const  float32_t * transmat1, const  float32_t * transmat2, const  float32_t * mixmat, const  float32_t * mu, const  float32_t * logDenom, const  float32_t * invSigma)
{
	float32_t loglik;
	float32_t * B = NULL;
	
				
	// Aloco memoria para la matriz B
	B = malloc(NESTADOS * T * sizeof(*B)); // B[NESTADOS][T]

	// Cálculo de la matriz B de probabilidades de salida
	Tesis_mixgauss_logprob(data, mu, logDenom, invSigma, mixmat, B, T);
	
	// Cálculo del log-likelihood utilizando el procedimiento forward
	loglik = Tesis_forward(transmat1, transmat2, B, T);
	
	free(B);
	B = NULL;
	
	return loglik;
	
}


// Generación de la matriz B de probabilidades logarítmicas de salida calculando la pdf
// (probability density function) para cada estado del modelo HMM y en cada instante de
// la secuencia de vectores de observación
void Tesis_mixgauss_logprob(float32_t * data, const  float32_t * mu, const  float32_t * logDenom, const  float32_t * invSigma, const  float32_t * mixmat, float32_t * B, uint16_t T)
{
	uint16_t t;
	uint16_t q;
	uint16_t k;
	
	for (t = 0; t < T; t++)
	{
		
    // log(sum(mixmat.*B2)) ~ log(max(mixmat.*B2))
    // log(max(mixmat.*B2)) = max(log(mixmat.*B2))
    // max(log(mixmat.*B2)) = max(log(mixmat) + log(B2))
    // B2 (QxMxT) es como la matriz B(QxT), pero para una mezcla en particular
    // B(QxT) es la suma ponderada de B2 a lo largo de M
    // B(:,t) = max(mixmat + B2(:,:,t), [], 2);
		
			for (q = 0; q < NESTADOS; q++)
			{
				
				*(B + q*T + t) = fmaxf(*(mixmat + q*NMEZCLAS + 0) + Tesis_gaussian_logprob(data + NCOEFS*t, mu + q*NMEZCLAS*NCOEFS + 0*NCOEFS, *(logDenom + q*NMEZCLAS + 0), invSigma + q*NMEZCLAS*NCOEFS + 0*NCOEFS),
															 *(mixmat + q*NMEZCLAS + 1) + Tesis_gaussian_logprob(data + NCOEFS*t, mu + q*NMEZCLAS*NCOEFS + 1*NCOEFS, *(logDenom + q*NMEZCLAS + 1), invSigma + q*NMEZCLAS*NCOEFS + 1*NCOEFS));
				
				if (NMEZCLAS > 2)
				{
					
					for (k = 2; k < NMEZCLAS; k++)
					{
						
						*(B + q*T + t) = fmaxf(*(B + q*T + t),
											    				 *(mixmat + q*NMEZCLAS + k) + Tesis_gaussian_logprob(data + NCOEFS*t, mu + q*NMEZCLAS*NCOEFS + k*NCOEFS, *(logDenom + q*NMEZCLAS + k), invSigma + q*NMEZCLAS*NCOEFS + k*NCOEFS));
						
					}
					
				}
				
			}
		
	}
	
}


// Cálculo de la probabilidad logarítmica de una densidad gausiana de múltiples dimensiones
// para cada instante de la secuencia de vectores de observación
float32_t Tesis_gaussian_logprob(float32_t * data, const  float32_t * mu, const  float32_t logDenom, const  float32_t * invSigma)
{

	uint16_t i;
	float32_t exponente = 0;
	float32_t p;

	// Cálculo del exponente de la ecuación
	for (i = 0; i<NCOEFS; i++)
	{
		
		exponente = exponente + powf(*(data + i) - *(mu + i),2)*(*(invSigma + i));
		
	}	

	// Cálculo del logaritmo de la probabilidad
	p = -0.5*exponente - logDenom; 
	
	return p;
	
}


// Cálculo del log-likelihood (posterior probability) utilizando el procedimiento forward
// A diferencia de la documentación, donde se diferencian las matrices alpha (matriz de 
// variables logarítmicas forward) y B (matriz B de probabilidades logarítmicas de salida)por
// separado, aquí se logró utilizar una misma matriz (alphaB) que cumpla la misma función que
// las dos anteriores, optimizando así el uso de la memoria. Tener en cuenta que en C, se la 
// pasa un puntero a la matriz B (acá llamada alphaB) a la función forward, y es modificada 
// dentro de la función, así que cuando termina la misma, dicha matriz tiene un valor distinto 
// a antes de empezar (está normalizada)
float32_t Tesis_forward(const  float32_t * transmat1, const  float32_t * transmat2, float32_t * alphaB, uint16_t T)
{

	uint16_t t;
	uint16_t i;
	float32_t m[NESTADOS];
	float32_t m2[NESTADOS];
	
	// Factor de escala (Rabiner, 1989)
	// scale(t) = Pr(O(t) | O(1:t-1)) = 1/c(t) as defined by Rabiner (1989).
	float32_t scale;
	
	// Inicialización del acumulador del log-likelihood
	float32_t loglik = 0;
	
	// Paso 1 del procedimiento Forward
	t = 0;
	// prior: vector de probabilidades de estados iniciales
	// log(prior*B) = log(prior) + log(B)
	// pero acá prior vale siempre [1 0 0...0]
	// log(1) = 0; log(0) = -inf
	// log(prior) = [0 -inf -inf...-inf]
	
	for (i = 1; i < NESTADOS; i++) // log(prior*B) = log(prior) + log(B); log(prior(1)) = 0
	{
		
		*(alphaB + i*T + t) = -INFINITY; // log(prior*B) = log(prior) + log(B); log(prior(2:q)) = -inf
		
	}
	
	scale = Tesis_lognormalise(alphaB + t, T); // Cálculo de la escala y normalización de alpha

	// Si para un instante el factor de escala da 0, toda la sequencia analizada no tiene 
	// posibilidad de ser modelizada por el correspondiente modelo HMM => retornamos con 
	// probabilidad 0 (loglik = -inf)
	if (scale == -INFINITY)
	{
		loglik = -INFINITY;
		return loglik;
	}
	else
	{
		loglik = loglik + scale; // Rabiner (1989), eq. 103: log(P) = sum(log(scale))
	}
	
	
	// Pasos 2 a T del procedimiento Forward
	for (t = 1; t < T; t++)
	{
    // Esta función no recibe toda la matriz de transiciones sino sólo sus dos diagonales 
		// distintas de cero (para optimizar el uso de la memoria). Entonces la multiplicación 
		// de la matriz transmat con la matriz  alpha se lleva a cabo en dos pasos, uno por 
		// cada diagonal, y luego se suman ambos.
		
		for (i = 0; i < NESTADOS-1; i++)
		{
			
			m[i] = *(transmat1 + i) + *(alphaB + i*T + t - 1); // log(transmat1*alpha) = log(transmat1) + log(alpha)
			
		}
		
		m[NESTADOS-1] = *(alphaB + (NESTADOS-1)*T + t - 1); // el último valor de transmat1 (diagonal principal) vale siempre 1 (log(1) = 0), por eso no se lo pasé

		m2[0] = -INFINITY; // la segunda diagonal no influye en el cálculo del primer valor, entonces vale 0. log(0) = -inf

		for (i = 1; i < NESTADOS; i++)
		{
			
			m2[i] = *(transmat2 + i - 1) + *(alphaB + (i-1)*T + t - 1); // log(transmat2*alpha) = log(transmat2) + log(alpha)
			
		}

		for (i = 0; i < NESTADOS; i++)
		{
			
			m[i] = fmaxf(m[i], m2[i]); // log(sum(m1 m2)) ~ log(max(m1 m2)); en lugar de m1 uso m para ahorrar memoria
			
		}
		
		for (i = 0; i < NESTADOS; i++)
		{
			
			*(alphaB + i*T + t) = m[i] + *(alphaB + i*T + t); // log(m*B) = log(m) + log(B)

		}

		scale = Tesis_lognormalise(alphaB + t, T); // Cálculo de la escala y normalización de alpha

    // Si para un instante el factor de escala da 0, toda la sequencia analizada no tiene 
		// posibilidad de ser modelizada por el correspondiente modelo HMM => retornamos con 
		// probabilidad 0 (loglik = -inf)
		if (scale == -INFINITY)
		{
			loglik = -INFINITY;
			return loglik;
		}
		else
		{
			loglik = loglik + scale; // Rabiner (1989), eq. 103: log(P) = sum(log(scale))
		} 

	}

	return loglik;
	
}

// Normalización del vector A (con valores logarítmicos), de forma que sus valores no logaritmicos sumen 1
// Cálculo del factor de escala/normalización logarítmico
float32_t Tesis_lognormalise(float32_t * A, uint16_t T)
{
	uint16_t i;
	float32_t c;
	float32_t s;

	// c = max(A(:)); % log(sum(A)) ~ log(max(A))
	c = fmaxf(*A, *(A + T)); // log(sum(A)) ~ log(max(A))
	for (i = 2; i < NESTADOS; i++)
	{
		c = fmaxf(c, *(A + i*T)); // log(sum(A)) ~ log(max(A))
	}

	// En caso de que el factor de escala logarítmico de -inf, evitamos que el valor de la 
	// matriz normalizada se modifique
	if (c == -INFINITY)
		s = 0;
	else
		s = c;
	
	for (i = 0; i < NESTADOS; i++)
	{
		*(A + i*T) = *(A + i*T) - s; // log(A/s) = log(A) - log(s)
	}
	
	return c;
	
}


/*
float32_t dtw (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, float32_t *dist_mtx)
{

	// Assert error
	assert_param ( a->numCols == b->numCols );

	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
//	float32_t aux , past[3];
	float32_t cost;
	uint16_t width=10;
	float32_t *dtw_mtx;
	
	// Length of time series
	uint16_t costmtxcols = a->numRows;
	uint16_t costmtxrows = b->numRows;
	uint16_t params      = b->numCols; // == a->numCols;
	
	// Adapt width
	width = (uint16_t) max(width, abs(costmtxrows-costmtxcols));

	// Allocate mememory for matrices used for calculation	
	if(dist_mtx == NULL)
	{
		if( (dtw_mtx = malloc( costmtxrows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)
			Error_Handler("Error on malloc dtw_mtx in recognition");
	}
	else
		dtw_mtx = dist_mtx;
	
	// Start at (0,0) and initialized everithing else in Inf
	dtw_mtx[0] = 0;
	for( i=1 ; i < costmtxrows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;

	// Initialize dist measurement
	init_dist(params);	
	
	// Get distance matrix
	for( i=1 ; i < costmtxrows ; i++ )
	{	
		for( j = max(1,i-width) ; j < min(costmtxcols,i+1+width) ; j++ )
		{
			// Get distance
			cost = dist( &(a->pData[i*params]) , &(b->pData[j*params]));
			
			// Calculate indexes
			idx 			=	i	* costmtxcols +  j		;
			idx_diagonal	= (i-1) * costmtxcols + (j-1)	;
			idx_vertical	= (i-1) * costmtxcols +   j  	;
			idx_horizontal	=   i   * costmtxcols + (j-1)	;
			
//			// Calculate arrive to node through:
//			past[DIAGONAL]   = dtw_mtx[idx_diagonal];			// diagonal line
//			past[VERTICAL] 	 = dtw_mtx[idx_vertical];			// vertical line
//			past[HORIZONTAL] = dtw_mtx[idx_horizontal];		// horizontal line
//			
//			// Calculo el minimo de todos
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

	// De-init dist measurement
	deinit_dist();
	
	// Free memory
	if(dist_mtx == NULL)
		free(dtw_mtx);
	
	// Return distance value
	return dtw_mtx[costmtxrows*costmtxcols];
}


float32_t dtw_reduce (const arm_matrix_instance_f32 *a, const arm_matrix_instance_f32 *b, const bool save_dist_mtx, uint8_t width)
{
	// Assert error
	assert_param ( a->numCols == b->numCols );

	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
//	float32_t aux , past[3];
	float32_t cost;
//	uint16_t width=10;
	float32_t *dtw_mtx;
	float32_t result = FLT_MAX;
	FIL dist_mtx_file;
	uint32_t byteswrite;
	char dist_file_name[] = "Dmtx_00.bin";
	
	// Length of time series
	uint16_t costmtxrows = b->numRows + 1;
	uint16_t costmtxcols = a->numRows + 1;
	uint16_t params      = b->numCols; // == a->numCols;
	uint8_t reduce_rows  = 2;
	
	uint32_t dtw_mtx_size = reduce_rows * costmtxcols;
	uint32_t idx_dtw_mtx_last_row = (reduce_rows-1) * costmtxcols;
	
	if(save_dist_mtx)
	{
		// Abro un archivo para salvar la matriz de distancia
		for(; f_stat(dist_file_name,NULL)!= FR_NO_FILE; updateFilename(dist_file_name));
		if ( f_open(&dist_mtx_file, dist_file_name ,FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
			Error_Handler("Error on dtw_mtx write in recognition");
	}
	
	// Adapt width
	width = (uint8_t) max(width, abs(costmtxrows-costmtxcols));
	
	// Allocate memory for matrices used for calculation	
	if( (dtw_mtx = malloc( reduce_rows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)
		Error_Handler("Error on malloc dtw_mtx in recognition");
		
	// Start at (0,0) and initialized everithing in inf. I put costmtxcols, because it will be moved
	dtw_mtx[costmtxcols] = 0;
	for( i=costmtxcols+1 ; i < reduce_rows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;
	
		// Initialize dist measurement
	init_dist(params);
	
	// Go through rows
	for( i=1 ; i < costmtxrows ; i++ )
	{
		// Escribo la nueva última fila
		if(save_dist_mtx)
			if ( f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite) != FR_OK)
				Error_Handler("Error on dtw_mtx write in recognition");
		
		// Shifteo una fila hacia arriba y seteo la nueva última fila a infinito
		memcpy(&dtw_mtx[0] , &dtw_mtx[costmtxcols] , (dtw_mtx_size - costmtxcols) * sizeof(*dtw_mtx));
		for( j=idx_dtw_mtx_last_row ; j < dtw_mtx_size ; j++ )
			dtw_mtx[j] = FLT_MAX;
		
		// Go through columns
		for( j = max(1,i-width) ; j < min(costmtxcols,i+1+width) ; j++ )
		{
			// Get distance
			cost = dist( &(a->pData[(j-1)*params]) , &(b->pData[(i-1)*params]));
			
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
	
	// Escribo la última fila y cierro el archivo
	if(save_dist_mtx)
	{
		f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite);
		f_close(&dist_mtx_file);
	}

	// De-init dist measurement
	deinit_dist();
	
	// Return distance value
	result = dtw_mtx[ dtw_mtx_size - 1 ];
	
	// Free memory
	free(dtw_mtx);

	return result;
}
float32_t dtw_files (FIL *a, FIL *b, const uint8_t parameters_size, const bool save_dist_mtx)
{
	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
//	float32_t aux , past[3];
	float32_t cost;
	uint16_t width=10;
	float32_t *dtw_mtx = NULL;
	float32_t result = FLT_MAX;
	UINT bytes_read;
	float32_t *vec_a = NULL, *vec_b= NULL;
	
	
	// Length of time series
	uint16_t costmtxcols = f_size(a) /sizeof(float32_t) /parameters_size + 1;
	uint16_t costmtxrows = f_size(b) /sizeof(float32_t) /parameters_size + 1;
	uint16_t params      = parameters_size; // == a->numCols;

	// Allocate space for vectors
	vec_a = malloc(parameters_size * sizeof(*vec_a));
	vec_b = malloc(parameters_size * sizeof(*vec_b));
	
	// Adapt width
	width = (uint16_t) max(width, abs(costmtxrows-costmtxcols));

	// Allocate mememory for matrices used for calculation	
	if( (dtw_mtx = malloc( costmtxrows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)
		Error_Handler("Error on malloc dtw_mtx in recognition");
	
	// Start at (0,0) and initialized everithing else in Inf
	dtw_mtx[0] = 0;
	for( i=1 ; i < costmtxrows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;

	// Initialize dist measurement
	init_dist(params);	
	
	// Get distance matrix
	for( i=1 ; i < costmtxrows ; i++ )
	{	
		for( j = max(1,i-width) ; j < min(costmtxcols,i+1+width) ; j++ )
		{
			//Leo el archivo
			f_lseek(a,(i-1)*params);
			f_read(a, vec_a, parameters_size * sizeof(*vec_a), &bytes_read);
			
			f_lseek(b,(j-1)*params);
			f_read(b, vec_b, parameters_size * sizeof(*vec_b), &bytes_read);

			// Get distance
			cost = dist( vec_a , vec_b);
			
			// Calculate indexes
			idx 			=	i	* costmtxcols +  j		;
			idx_diagonal	= (i-1) * costmtxcols + (j-1)	;
			idx_vertical	= (i-1) * costmtxcols +   j  	;
			idx_horizontal	=   i   * costmtxcols + (j-1)	;
			
//			// Calculate arrive to node through:
//			past[DIAGONAL]   = dtw_mtx[idx_diagonal];			// diagonal line
//			past[VERTICAL] 	 = dtw_mtx[idx_vertical];			// vertical line
//			past[HORIZONTAL] = dtw_mtx[idx_horizontal];		// horizontal line
//			
//			// Calculo el minimo de todos
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

	// Free vecs memory
	free(vec_a);
	free(vec_b);
	
	// De-init dist measurement
	deinit_dist();

	// Get output
	result = dtw_mtx[costmtxrows*costmtxcols-1];
	
	//Save distance matrix if necesary
	if(save_dist_mtx)
	{
		FIL dist_mtx_file;
		UINT byteswritten;
		char dist_file_name[] = "Dmtx_00.bin";
		
		// Abro un archivo para salvar la matriz de distancia
		for(; f_stat(dist_file_name,NULL)!= FR_NO_FILE; updateFilename(dist_file_name));
		if ( f_open(&dist_mtx_file, dist_file_name ,FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
			Error_Handler("Error on dtw_mtx write in recognition");
		f_write(&dist_mtx_file, dtw_mtx, costmtxrows * costmtxcols * sizeof(*dtw_mtx ), &byteswritten);
		f_close(&dist_mtx_file);
	}
	
	// Free memory
	free(dtw_mtx);
	
	// Return distance value
	return result;
}


float32_t dtw_files_reduce (FIL *a, FIL *b, const uint8_t parameters_size, const bool save_dist_mtx)
{
	// Variables
	int32_t i,j;
	uint32_t idx, idx_vertical, idx_horizontal,idx_diagonal, path_idx;
//	float32_t aux , past[3];
	float32_t cost;
	uint16_t width=10;
	float32_t *dtw_mtx;
	float32_t result = FLT_MAX;
	FIL dist_mtx_file;
	uint32_t byteswrite,bytes_read;
	char dist_file_name[] = "Dmtx_00.bin";
	
	float32_t *vec_a = NULL, *vec_b= NULL;
	
	// Length of time series
	uint16_t costmtxcols = f_size(a) /sizeof(float32_t) /parameters_size + 1;
	uint16_t costmtxrows = f_size(b) /sizeof(float32_t) /parameters_size + 1;
	uint16_t params      = parameters_size; // == a->numCols;
	uint8_t reduce_rows  = 2;
	
	// Allocate space for vectors
	vec_a = malloc(parameters_size * sizeof(*vec_a));
	vec_b = malloc(parameters_size * sizeof(*vec_b));


	uint32_t dtw_mtx_size = reduce_rows * costmtxcols;
	uint32_t idx_dtw_mtx_last_row = (reduce_rows-1) * costmtxcols;
	
	if(save_dist_mtx)
	{
		// Abro un archivo para salvar la matriz de distancia
		for(; f_stat(dist_file_name,NULL)!= FR_NO_FILE; updateFilename(dist_file_name));
		if ( f_open(&dist_mtx_file, dist_file_name ,FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
			Error_Handler("Error on dtw_mtx write in recognition");
	}
	
	// Adapt width
	width = (uint16_t) max(width, abs(costmtxrows-costmtxcols));
	
	// Allocate memory for matrices used for calculation	
	if( (dtw_mtx = malloc( reduce_rows * costmtxcols * sizeof(*dtw_mtx) ) ) == NULL)
		Error_Handler("Error on malloc dtw_mtx in recognition");
		
	// Start at (0,0) and initialized everithing in inf. I put costmtxcols, because it will be moved
	dtw_mtx[costmtxcols] = 0;
	for( i=costmtxcols+1 ; i < reduce_rows * costmtxcols ; i++ )
		dtw_mtx[i] = FLT_MAX;
	
		// Initialize dist measurement
	init_dist(params);
	
	// Go through rows
	for( i=1 ; i < costmtxrows ; i++ )
	{
		// Escribo la nueva última fila
		if(save_dist_mtx)
			if ( f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite) != FR_OK)
				Error_Handler("Error on dtw_mtx write in recognition");
		
		// Shifteo una fila hacia arriba y seteo la nueva última fila a infinito
		memcpy(&dtw_mtx[0] , &dtw_mtx[costmtxcols] , (dtw_mtx_size - costmtxcols) * sizeof(*dtw_mtx));
		for( j=idx_dtw_mtx_last_row ; j < dtw_mtx_size ; j++ )
			dtw_mtx[j] = FLT_MAX;
		
		// Go through columns
		for( j = max(1,i-width) ; j < min(costmtxcols,i+1+width) ; j++ )
		{
			//Leo el archivo
			f_lseek(a,(i-1)*params);
			f_read(a, vec_a, parameters_size * sizeof(*vec_a), &bytes_read);
			
			f_lseek(b,(j-1)*params);
			f_read(b, vec_b, parameters_size * sizeof(*vec_b), &bytes_read);

			// Get distance
			cost = dist( vec_a , vec_b);
			
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
	
	// Escribo la última fila y cierro el archivo
	if(save_dist_mtx)
	{
		f_write(&dist_mtx_file, &dtw_mtx[idx_dtw_mtx_last_row], costmtxcols * sizeof(*dtw_mtx), &byteswrite);
		f_close(&dist_mtx_file);
	}

	// De-init dist measurement
	deinit_dist();
	
	// Return distance value
	result = dtw_mtx[ dtw_mtx_size - 1 ];
	
	// Free memory
	free(dtw_mtx);

	// Free vecs memory
	free(vec_a);
	free(vec_b);
	
	return result;
}
//---------------------------------------
//-						Distance Measure					-
//---------------------------------------
float32_t *pDst;
uint32_t dist_blockSize;
void init_dist (uint32_t blockSize)
{
	// Allocate memory
	if ( (pDst = malloc(blockSize * sizeof(*pDst))) == NULL)
		Error_Handler("Error on malloc pDst in recognition");
	
	// Set block size
	dist_blockSize = blockSize;
}
void deinit_dist (void)
{
	// Free Memory
	free(pDst);;
}
float32_t dist (float32_t *pSrcA, float32_t *pSrcB)
{
	
	float32_t power,result;
	
	// Calculo la distancia euclidia
	arm_sub_f32 	(pSrcA, pSrcB, pDst, dist_blockSize);		// pDst[n] = pSrcA[n] - pSrcB[n]
	arm_power_f32	(pDst, dist_blockSize, &power); 				// pDst[1]^2 + pDst[2]^2 + ... + pDst[n]^2
	arm_sqrt_f32 	(power,&result);												// sqrt(power)
	
	return result;
}

*/
