/**
  ******************************************************************************
  * @file    macrosLCD.h
  * @author  Mariano Marufo da Silva
  * @email 	 mmarufodasilva@est.frba.utn.edu.ar
  ******************************************************************************
  */  

#ifndef MACROSLCD_H
#define MACROSLCD_H


#define LCD_REG_SEL_Port GPIOC
#define LCD_REG_SEL_Pin GPIO_PIN_14
#define LCD_ENABLE_Port GPIOC
#define LCD_ENABLE_Pin GPIO_PIN_15
#define LCD_DATOS_8bits GPIOD // sólo los bits 0 a 7

#endif // MACROSLCD_H
