/**
  ******************************************************************************
  * @file    ale_stdlib.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    09-Junio-2015
  * @brief   Standar lib
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
*/

#ifndef ALE_STDLIB_H
#define ALE_STDLIB_H

#include <cmsis_os.h>

#define malloc(size) pvPortMalloc(size)
#define free(ptr) 		vPortFree(ptr)

#endif // ALE_STDLIB_H

