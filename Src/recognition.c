/**
  ******************************************************************************
  * @file    audio_processing.c
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    28-Enero-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @version V2.0.0
  * @author  Mariano Marufo da Silva
	* @email 	 mmarufodasilva@est.frba.utn.edu.ar
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
		
		exponente = exponente + (*(data + i) - *(mu + i))*(*(data + i) - *(mu + i))*(*(invSigma + i));
		
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
