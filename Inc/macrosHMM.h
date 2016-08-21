/**
  ******************************************************************************
  * @file    macrosHMM.h
  * @author  Mariano Marufo da Silva
	* @email 	 mmarufodasilva@est.frba.utn.edu.ar
  ******************************************************************************
  */  

#ifndef MACROSHMM_H
#define MACROSHMM_H

#include "ApplicationConfig.h"

#define NPALABRAS 11
#define NCOEFS ((LIFTER_LENGTH+1)*3) // TIENE QUE CORRESPONDER CON EL VALOR DE DENOM1
#define DENOM1 (3.6686599e+015) // (2*PI)^(NCOEFS/2)
#define NESTADOS 16
#define NMEZCLAS 4

#endif // MACROSHMM_H
