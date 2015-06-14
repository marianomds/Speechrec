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
#include "error_handler.h"
#include "cmsis_os.h"
#include "ff.h"
#include "misc.h"

int fputc( int c, FILE *pxNotUsed ) 
{
		return(ITM_SendChar(c));
}

void printUsageErrorMsg(uint32_t CFSRValue)
{
   printf("Usage fault: ");
   CFSRValue >>= 16;                  // right shift to lsb
   if((CFSRValue & (1 << 9)) != 0) {
      printf("Divide by zero\n");
   }
}
void printBusErrorMsg(uint32_t CFSRValue)
{
   printf("Bus fault: ");
}
void printMemErrorMsg(uint32_t CFSRValue)
{
   printf("Memory fault: ");
}

void HardFault_Handler(void)
{
	printf("In Hard Fault Handler\n");
	printf("SCB->HFSR = 0x%08x\n", SCB->HFSR);
	
	if ((SCB->HFSR & (1 << 30)) != 0)
	{
		printf("Forced Hard Fault\n");
		printf("SCB->CFSR = 0x%08x\n", SCB->CFSR );
		
		// Usage Fault Error
		if((SCB->CFSR & 0xFFFF0000) != 0)
			printUsageErrorMsg(SCB->CFSR);
		
		// Bus Fault Error
		if((SCB->CFSR & 0x0000FF00) != 0)
			printBusErrorMsg(SCB->CFSR);
		
		// Memory Fault Error
		if((SCB->CFSR & 0x000000FF) != 0)
			printMemErrorMsg(SCB->CFSR);
	}
	Error_Handler("In Hard Fault Handler\n");
	__ASM volatile("BKPT #01");
	while(1);
}


void Error_Handler(char * str)
{

	// Go to root directory
	FIL log_file;
	open_append(&log_file,"0:/log.txt");
	f_printf(&log_file, "%s\r\n", str);
	f_close(&log_file);
	
	// Imprimo
	printf ("%s\r\n", str);
	
	// Comienzo con la secuencia de leds
	LED_On(RLED);
	LED_On(GLED);
	LED_On(BLED);
	LED_On(OLED);
	
  while(1){
		LED_Toggle(RLED);
		LED_Toggle(GLED);
		LED_Toggle(BLED);
		LED_Toggle(OLED);
			
		for(int i=2000000; i!=0 ;i--);
	}
}

