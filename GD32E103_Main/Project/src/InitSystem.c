/******************************************************************************
 * @file    	InitSystem.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	28/02/2016
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <string.h>
#include <stdio.h>
#include "InitSystem.h"
#include "Version.h"
#include "DataDefine.h"
#include "Utilities.h"
#include "InternalFlash.h"
#include "HardwareManager.h"
#include "Indicator.h"
#include "Main.h"
#include "GSM.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
System_t xSystem;
//extern DriverUART_t DriverUART;
//extern Debug_t	DriverDebug;
//extern uint8_t esp32RebootTimeout;

//extern Driver_InternalFlash_t DriverInternalFlash;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

  
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
const char WelcomeStr[] = "\t\t\tCopyright by BYTECH JSC\r\n";
//const char Bytech[] = {	
//"\r    ______  ______________________  __       _______ ______\r"
//"   / __ ) \\/ /_  __/ ____/ ____/ / / /      / / ___// ____/\r"
//"  / __  |\\  / / / / __/ / /   / /_/ /  __  / /\\__ \\/ /     \r"
//" / /_/ / / / / / / /___/ /___/ __  /  / /_/ /___/ / /___\r"   
//"/_____/ /_/ /_/ /_____/\\____/_/ /_/   \\____//____/\\____/   \r"
//};

extern __IO uint16_t  adc_vin_value;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void RCC_Config(void) ;
static void InitIO(void);
static void ADC_Config(void);
static void InitVariable(void) ;
static void DrawScreen(void);



void LockReadOutFlash(void)
{
//	if(FLASH_OB_GetRDP() != SET)
//	{
//		DEBUG_PRINTF("Flash: Chua Lock Read protection. Thuc hien Lock...");
//		
//		FLASH_Unlock();
//		FLASH_OB_Unlock();
//		FLASH_OB_RDPConfig(OB_RDP_Level_1);
//		FLASH_OB_Launch();                   // Option Bytes programming		
//		FLASH_OB_Lock();
//		FLASH_Lock();
//	}
//	else
//		DEBUG_PRINTF("Flash: Read protect is LOCKED!");
}
	
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
void InitSystem(void) 
{ 
	/* Set Vector table to offset address of main application - used with bootloader */
//	RelocateVectorTable();

	SystemInit();
	
	RCC_Config();
	InitIO();
	WatchDogInit();
	
	InitVariable();
	
//	UART_Init(DEBUG_UART, 115200);
//	UART_Init(GSM_UART, 115200);
//	UART_Init(RS485_UART, 9600);
	
//	ADC_Config();
	
	//Read protection -> luc nap lai se mass erase chip -> mat luon cau hinh
//	LockReadOutFlash();
	
//	DrawScreen();	
//	DetectResetReason();
//	
//	InitGSM();
		
	xSystem.Status.InitSystemDone = 1;
}

	
/*****************************************************************************/
/**
 * @brief	:  	DrawScreen
 * @param	:  	None
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	28/02/2016
 * @version	:
 * @reviewer:	
 */
static void DrawScreen(void)
{		
	DEBUG_RAW("\r\n");
	DEBUG_RAW(WelcomeStr);
	DEBUG_RAW("Device: VS-Worker\r\n");	
	DEBUG_RAW("Firmware: %s%03u\r\n", FIRMWARE_VERSION_HEADER, FIRMWARE_VERSION_CODE);
	DEBUG_RAW("Released: %s - %s\r\n",__RELEASE_DATE_KEIL__,__RELEASE_TIME_KEIL__); 
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
static void RCC_Config(void) 
{ 	  	
//#if 0
//	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC, ENABLE);
//	
//	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART4 | RCC_APB1Periph_USART3, ENABLE);
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG | RCC_APB2Periph_USART1, ENABLE);

	/* Setup SysTick Timer for 1 msec interrupts */ 
	if (SysTick_Config(SystemCoreClock / 1000))
	{ 		
		/* Capture error */
		NVIC_SystemReset();
	} 
	NVIC_SetPriority(SysTick_IRQn, 0x00);	
//#else
//    /* enable ADC clock */
//    rcu_periph_clock_enable(RCU_ADC);
//    /* enable DMA clock */
//    rcu_periph_clock_enable(RCU_DMA);
//    /* config ADC clock */
//    rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);
//    
//    rcu_periph_clock_enable(RCU_GPIOA);
//    rcu_periph_clock_enable(RCU_GPIOB);
//    rcu_periph_clock_enable(RCU_GPIOC);
//    rcu_periph_clock_enable(RCU_GPIOF);
//#endif
}


/*****************************************************************************/
/**
 * @brief	:  Ham khoi tao cac chan I/O khac
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	02/03/2016
 * @version	:
 * @reviewer:	
 */
static void InitIO(void)
{	
	 /* enable the GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOA);
	 rcu_periph_clock_enable(RCU_GPIOB);
	
	 /* enable the AF clock */
//	 rcu_periph_clock_enable(RCU_AF);
	 
    /* configure LED GPIO port */ 
    gpio_init(LED1_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LED1_PIN);
	 gpio_init(LED2_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, LED2_PIN);
	
    LED1(0);
	 LED2(0);
}

/**
  * @brief  ADC1 channel configuration
  * @param  None
  * @retval None
  */
static void ADC_Config(void)
{
#if 0
	ADC_InitTypeDef     ADC_InitStructure;
	GPIO_InitTypeDef    GPIO_InitStructure;
	DMA_InitTypeDef   DMA_InitStructure;
	
	/* ADC1 DeInit */  
	ADC_DeInit(ADC1);

	/* GPIOC Periph clock enable */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);

	/* ADC1 Periph clock enable */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* DMA1 clock enable */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 , ENABLE);

	/* DMA1 Channel1 Config */
	DMA_DeInit(DMA1_Channel1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC1_DR_Address;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&adc_vin_value;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = 1;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	
	/* DMA1 Channel1 enable */
	DMA_Cmd(DMA1_Channel1, ENABLE);

	/* Configure ADC Channel8 as analog input */
	GPIO_InitStructure.GPIO_Pin = ADC_VIN_PIN ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
	GPIO_Init(ADC_VIN_PORT, &GPIO_InitStructure);

	/* Initialize ADC structure */
	ADC_StructInit(&ADC_InitStructure);

	/* Configure the ADC1 in continuous mode withe a resolution equal to 12 bits  */
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE; 
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_ScanDirection = ADC_ScanDirection_Upward;
	ADC_Init(ADC1, &ADC_InitStructure); 

	/* Convert the ADC1 Channel8 with 55.5 Cycles as sampling time */ 
	ADC_ChannelConfig(ADC1, ADC_VIN_CHANNEL , ADC_SampleTime_55_5Cycles); 

	/* ADC Calibration */
	ADC_GetCalibrationFactor(ADC1);

	/* ADC DMA request in circular mode */
	ADC_DMARequestModeConfig(ADC1, ADC_DMAMode_Circular);

	/* Enable ADC_DMA */
	ADC_DMACmd(ADC1, ENABLE);  

	/* Enable the ADC peripheral */
	ADC_Cmd(ADC1, ENABLE);     
  
	/* Wait the ADRDY flag */
	uint32_t Timeout = 0xFFFFFFF;
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADRDY) && Timeout) {
		Timeout--;
		if(Timeout % 10000) ResetWatchdog();
	}	  

	/* ADC1 regular Software Start Conv */ 
	ADC_StartOfConversion(ADC1);
#else    
//    gpio_mode_set(ADC_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_GPIO_PIN);
//    
//    /* ADC contineous function enable */
//    adc_special_function_config(ADC_CONTINUOUS_MODE, ENABLE);
//    /* ADC trigger config */
//    adc_external_trigger_source_config(ADC_REGULAR_CHANNEL, ADC_EXTTRIG_REGULAR_NONE); 
//    /* ADC data alignment config */
//    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);
//    /* ADC channel length config */
//    adc_channel_length_config(ADC_REGULAR_CHANNEL, 1U);
// 
//    /* ADC regular channel config */
//    adc_regular_channel_config(0U, BOARD_ADC_CHANNEL, ADC_SAMPLETIME_55POINT5);
//    adc_external_trigger_config(ADC_REGULAR_CHANNEL, ENABLE);
//    
//    /* enable ADC interface */
//    adc_enable();
//    Delayms(1U);
//    /* ADC calibration and reset calibration */
//    adc_calibration_enable();

//    /* ADC analog watchdog threshold config */
//    adc_watchdog_threshold_config(0x0400U, 0x0A00U);
//    /* ADC analog watchdog single channel config */
//    adc_watchdog_single_channel_enable(BOARD_ADC_CHANNEL);
//    /* ADC interrupt config */
//    adc_interrupt_enable(ADC_INT_WDE);

//    /* ADC software trigger enable */
//    adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
#endif
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
static void InitVariable(void)
{
//    xSystem.Driver.UART = &DriverUART;
//    xSystem.Debug = &DriverDebug;
//    xSystem.Driver.InternalFlash = &DriverInternalFlash;
}


///*****************************************************************************/
///**
//  * @brief  Retargets the C library DEBUG_PRINTF function to the USART.
//  * @param  None
//  * @retval None
//  */
//PUTCHAR_PROTOTYPE 
//{	
//	xSystem.Driver.UART->Putc(DEBUG_UART, ch);
//	return ch;
//}

/********************************* END OF FILE *******************************/
