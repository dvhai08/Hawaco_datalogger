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
#include "gsm_utilities.h"
#include "InternalFlash.h"
#include "HardwareManager.h"
#include "Main.h"
#include "gsm.h"
#include "MQTTUser.h"
#include "measure_input.h"
#include "app_bkup.h"
#include "InternalFlash.h"
#include "ControlOutput.h"

#define ADC_DMA			1

/* constant for adc threshold value 3.3V */
//#define ADC_VREF			3300
#define DIODE_OFFSET	0
#define RESISTOR_DIV	2
/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
System_t xSystem;
//extern DriverUART_t DriverUART;
//extern Debug_t	DriverDebug;
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
const char WelcomeStr[] = "\t\tCopyright by BYTECH JSC\r\n";
//const char Bytech[] = {	
//"\r    ______  ______________________  __       _______ ______\r"
//"   / __ ) \\/ /_  __/ ____/ ____/ / / /      / / ___// ____/\r"
//"  / __  |\\  / / / / __/ / /   / /_/ /  __  / /\\__ \\/ /     \r"
//" / /_/ / / / / / / /___/ /___/ __  /  / /_/ /___/ / /___\r"   
//"/_____/ /_/ /_/ /_____/\\____/_/ /_/   \\____//____/\\____/   \r"
//};

extern __IO uint16_t  adc_vin_value;
extern __IO uint16_t ADC_RegularConvertedValueTab[MEASURE_INPUT_ADC_DMA_UNIT];

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
void RCC_Config(void) ;
static void InitIO(void);
static void InitVariable(void) ;
static void DrawScreen(void);
static void RtcConfiguration(void);


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

	RCC_Config();
	WatchDogInit();
	RtcConfiguration();
	InitIO();
	InitVariable();
	
//	UART_Init(DEBUG_UART, 115200);
//	UART_Init(RS485_UART, 9600);
	
	ADC_Config();
	
	/*Init RTC backup register */
	app_bkup_init();
	
	//Read protection -> luc nap lai se mass erase chip -> mat luon cau hinh
//	LockReadOutFlash();
	
	DrawScreen();	
	DetectResetReason();
	
	InternalFlash_ReadConfig();
	
	measure_input_initialize();
	Output_Init();

    pmu_wakeup_pin_enable();

    gsm_internet_mode_t *internet_mode = gsm_get_internet_mode();
    
    gsm_init_hw();
    init_TcpNet();

    if (*internet_mode == GSM_INTERNET_MODE_PPP_STACK)
    {
        MQTT_Init();
    }
    
#if GSM_ENABLE	

#else
	gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);/*!< JTAG-DP disabled and SW-DP enabled */
	// gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, DISABLE);
	
	gpio_init(GSM_PWR_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_PWR_EN_PIN);
	gpio_init(GSM_PWR_KEY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_PWR_KEY_PIN);
	gpio_init(GSM_RESET_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_RESET_PIN);
	
	//Tat nguon module
	GSM_PWR_EN(0);
	GSM_PWR_RESET(1);
	GSM_PWR_KEY(0);
#endif		
	xSystem.Status.InitSystemDone = 1;
	xSystem.Status.DisconnectTimeout = 1;
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
	DEBUG_RAW("Device: HAWACO Gateway\r\n");	
	DEBUG_RAW("Firmware: %s%02u\r\n", FIRMWARE_VERSION_HEADER, FIRMWARE_VERSION_CODE);
	DEBUG_RAW("Released: %s - %s\r\n",__RELEASE_DATE_KEIL__, __RELEASE_TIME_KEIL__); 
	
	uint32_t clkSys = rcu_clock_freq_get(CK_SYS)/1000000;
	uint32_t clkAHB = rcu_clock_freq_get(CK_AHB)/1000000;
	uint32_t clkAPB1 = rcu_clock_freq_get(CK_APB1)/1000000;
	uint32_t clkAPB2 = rcu_clock_freq_get(CK_APB2)/1000000;
	DEBUG_RAW("Clock: %u-%u-%u-%uMhz\r\n", clkSys, clkAHB, clkAPB1, clkAPB2);
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
void RCC_Config(void) 
{
    /* Setup SysTick Timer for 1 msec interrupts */ 
    if (SysTick_Config(SystemCoreClock / 1000))
    { 		
        /* Capture error */
        NVIC_SystemReset();
    } 
    NVIC_SetPriority(SysTick_IRQn, 0x01);		

    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    nvic_irq_enable(LVD_IRQn, 0, 1);

    ///* configure the lvd threshold to 2.9v */
    pmu_lvd_select(PMU_LVDT_3);
	
    /* PMU clock enable */
    rcu_periph_clock_enable(RCU_PMU);
    /* configure ckout0 source in RCU */
    rcu_ckout0_config(RCU_CKOUT0SRC_CKSYS);
    /* enable the GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    /* enable the AF clock */
    rcu_periph_clock_enable(RCU_AF);

    /* enable ADC clock */
    rcu_periph_clock_enable(RCU_ADC0);
    /* enable DMA clock */
    rcu_periph_clock_enable(RCU_DMA0);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV16);
}

static void RtcConfiguration(void)
{
    nvic_irq_enable(RTC_IRQn,1,0);
    /* enable PMU and BKPI clocks */
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_periph_clock_enable(RCU_PMU);
    /* allow access to BKP domain */
    pmu_backup_write_enable();

    /* reset backup domain */
    bkp_deinit();

    /* enable LXTAL */
    rcu_osci_on(RCU_LXTAL);
    /* wait till LXTAL is ready */
    rcu_osci_stab_wait(RCU_LXTAL);
    
    /* select RCU_LXTAL as RTC clock source */
    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

    /* enable RTC Clock */
    rcu_periph_clock_enable(RCU_RTC);

    /* wait for RTC registers synchronization */
    rtc_register_sync_wait();

    /* wait until last write operation on RTC registers has finished */
    rtc_lwoff_wait();

    /* enable the RTC second and alarm interrupt*/
    rtc_interrupt_enable(RTC_INT_SECOND);
    rtc_interrupt_enable(RTC_INT_ALARM);
    /* wait until last write operation on RTC registers has finished */
    rtc_lwoff_wait();

    /* set RTC prescaler: set RTC period to 1s */
    rtc_prescaler_set(32767);

    /* wait until last write operation on RTC registers has finished */
    rtc_lwoff_wait();

    /* enable and set EXTI interrupt to the lowest priority */
    nvic_irq_enable(RTC_ALARM_IRQn, 2U, 0U);
    /* configure EXTI line */
    exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_17);

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
    /* configure LED GPIO port */ 
    gpio_init(LED1_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_10MHZ, LED1_PIN);
    gpio_init(LED2_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_10MHZ, LED2_PIN);

//	//Led on
	LED1(1);
	LED2(1);
}

/**
  * @brief  ADC1 channel configuration
  * @param  None
  * @retval None
  */
static volatile bool adc_started = false;
void ADC_Config(void)
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
    
    if (adc_started)
    {
        return;
    }
    rcu_periph_clock_enable(RCU_ADC0);
    /* enable DMA clock */
    rcu_periph_clock_enable(RCU_DMA0);


    /* config the GPIO as analog mode */
    gpio_init(ADC_VIN_PORT, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, ADC_VIN_PIN);
    gpio_init(ADC_SENS_PORT, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, ADC_SENS_PIN);
	 
#if ADC_DMA
	 /** ================= DMA config ==================== */
	 /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC DMA_channel configuration */
    dma_deinit(DMA0, DMA_CH0);
    
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    dma_data_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr = (uint32_t)&ADC_RegularConvertedValueTab[0];
    dma_data_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;  
    dma_data_parameter.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number = MEASURE_INPUT_ADC_DMA_UNIT;
    dma_data_parameter.priority = DMA_PRIORITY_HIGH;
    dma_init(DMA0, DMA_CH0, &dma_data_parameter);

    dma_circulation_enable(DMA0, DMA_CH0);
    nvic_irq_enable(DMA0_Channel0_IRQn,0,0);
    dma_interrupt_enable(DMA0, DMA_CH0, DMA_INT_FTF);
  
    /* enable DMA channel */
    dma_channel_enable(DMA0, DMA_CH0);
#endif

	 /** ================= ADC config ==================== */
	 /* reset ADC */
    adc_deinit(ADC0);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC contineous function enable */
#if ADC_DMA
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    /* ADC scan mode disable */
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
#else
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
#endif
    /* ADC data alignment config */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    
    /* ADC channel length config */
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, MEASURE_INPUT_ADC_DMA_UNIT);
	 
    /* ADC regular channel config */
    adc_regular_channel_config(ADC0, 0, ADC_SENS_CHANNEL, ADC_SAMPLETIME_55POINT5);
    adc_regular_channel_config(ADC0, 1, ADC_VIN_CHANNEL, ADC_SAMPLETIME_55POINT5);

    /* ADC trigger config */
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_NONE); 
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
    
    /* enable ADC interface */
    adc_enable(ADC0);
    Delayms(1);
	 
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC0);
#if ADC_DMA
    /* ADC DMA function enable */
    adc_dma_mode_enable(ADC0);
#else
    nvic_irq_enable(ADC0_1_IRQn, 0, 0);
    adc_interrupt_flag_clear(ADC0, ADC_INT_FLAG_EOC);
    adc_interrupt_enable(ADC0, ADC_INT_EOC);
#endif 
	 /* ADC software trigger enable */
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    adc_started = true;
#endif
}

void AdcStop(void)
{
    if (adc_started == false)
    {
        return;
    }
    adc_interrupt_flag_clear(ADC0, ADC_INT_FLAG_EOIC);
    dma_circulation_disable(DMA0, DMA_CH0);
    adc_disable(ADC0);
    adc_deinit(ADC0);
    dma_channel_disable(DMA0, DMA_CH0);
    dma_deinit(DMA0, DMA_CH0);
    rcu_periph_clock_disable(RCU_ADC0);
    rcu_periph_clock_disable(RCU_DMA0);
    adc_started = false;
}

uint32_t AdcUpdate(void)
{
    return RESISTOR_DIV*(ADC_RegularConvertedValueTab[ADCMEM_VSYS] * ADC_VREF/ ADC_12BIT_FACTOR) + DIODE_OFFSET;
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
