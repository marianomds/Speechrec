Cuando se genera el proyecto con el STM32CubeMx, hay que modificar:

Por priemra y única vez:

	- en usbh_conf.c

		    /* Drive high Charge pump */
		    /* USER CODE BEGIN 1 */ 
		    /* ToDo: Add IOE driver control */
			    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
		    /* USER CODE END 1 */ 


		    /* Drive low Charge pump */
		    /* USER CODE BEGIN 2 */
		    /* ToDo: Add IOE driver control */
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
		    /* USER CODE END 2 */


	-en usb_host.c

			/* USER CODE BEGIN 0 */
				extern osMessageQId appli_event;
			/* USER CODE END 0 */

			/* USER CODE BEGIN 2 */
			  switch(id)
			  { 
			  case HOST_USER_SELECT_CONFIGURATION:
			  break;
			    
			  case HOST_USER_DISCONNECTION:
			  Appli_state = APPLICATION_DISCONNECT;
				osMessagePut(appli_event,APPLICATION_DISCONNECT,0);
			  break;
			    
			  case HOST_USER_CLASS_ACTIVE:
			  Appli_state = APPLICATION_READY;
				osMessagePut(appli_event,APPLICATION_START,0);
			  break;

			  case HOST_USER_CONNECTION:
			  Appli_state = APPLICATION_START;
			  break;

			  default:
			  break; 
			  }
			/* USER CODE END 2 */

	-en freertos.c

			/* USER CODE BEGIN Init */
				Configure_Application();
			/* USER CODE END Init */

			/* USER CODE BEGIN StartDefaultTask */
			  /* Infinite loop */
			  for(;;)
			  {
			    osThreadTerminate(osThreadGetId());
			  }
			/* USER CODE END StartDefaultTask */


Cada vez que se genera el proyecto:

	-en startup_stm32f407xx.s:

			Stack_Size      EQU     0x00000800
			Heap_Size       EQU     0x00000400

	-en cmsis_os.h
		#if 1 /* Mail Queue Management Functions are not supported in this cmsis_os version, will be added in the next release  */
		struct os_mailQ_cb *os_mailQ_cb_##name; \

	-en cmsis_os.c
		#if 1 /* Mail Queue Management Functions are not supported in this cmsis_os version, will be added in the next release  */



AGREGAR UN MessageQDelete

void osMessageDelete (osMessageQId *queue_id)
{
  vQueueDelete (*queue_id);
	queue_id = NULL;
}