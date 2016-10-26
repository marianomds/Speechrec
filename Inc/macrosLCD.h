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

#define     LCD_L1     0x80
#define     LCD_L2	   0xC0
#define     LCD_L3	   0x94
#define     LCD_L4	   0xD4
#define     LCD_L2_B	 0xD1
#define     LCD_L3_B	 0xA5
#define     LCD_L4_B	 0xE3
#define     LCD_NOL	   0x00   //lo pone en la posicion que estaba(uso el cero por que es un codigo invalido de posicion ya que el bit 7 tiene que estar en uno si o si)

#endif // MACROSLCD_H
