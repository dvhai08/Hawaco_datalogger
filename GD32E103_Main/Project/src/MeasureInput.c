/******************************************************************************
 * @file    	Measurement.c
 * @author  	
 * @version 	V1.0.0
 * @date    	15/01/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Measurement.h"
#include "DataDefine.h"
#include "Hardware.h"
#include "Utilities.h"
#include "HardwareManager.h"
#include "InternalFlash.h"
#include "main.h"
#include "app_bkup.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
/* constant for adc resolution is 12 bit = 4096 */
#define ADC_12BIT_FACTOR	4096


#define	RS485_DIR_OUT()		GPIO_SetBits(RS485_DR_PORT, RS485_DR_PIN)
#define	RS485_DIR_IN()		GPIO_ResetBits(RS485_DR_PORT, RS485_DR_PIN)

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
__IO uint16_t ADC_RegularConvertedValueTab[ADCMEM_MAXUNIT];
static uint8_t Timeout1000msTick = 0;

uint16_t VphotoAvg[2];
SmallBuffer_t SensorUartBuffer;

uint8_t lastPulseState = 0;
uint8_t curPulseState = 0;
uint16_t pulseLengthInMs = 0;
uint8_t isPulseTrigger = 0;
uint32_t beginPulseTime, endPulseTime;
int8_t pullState = -1;
uint32_t pullDeltaMs;
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void MeasureInputTick(void) ;
static void ProcessNewSensorUartData(void);

uint16_t AVGVphoto(uint16_t curVphoto)
{
	VphotoAvg[0] = VphotoAvg[1];
	VphotoAvg[1] = VphotoAvg[0] + (curVphoto - VphotoAvg[0])/10;
	return VphotoAvg[1];
}

/*****************************************************************************/
/**
 * @brief	:  	Tick every 1ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void Measure_PulseTick(void)
{
//	curPulseState = getPulseState();
//	if(curPulseState != lastPulseState)
//	{
//		//begin of rising edge
//		if(curPulseState == 1) {
//			pulseLengthInMs = 1;
//		} else {
//			//end of falling edge
//			DEBUG ("\r\nPulse length: %d ms", pulseLengthInMs);
//			
//			//valid pulse length: 10-1500ms
//			if(pulseLengthInMs >= 10 && pulseLengthInMs <= 1500)
//			{
//				xSystem.MeasureStatus.PulseCounterInBkup++;
//				DEBUG ("\r\nPulse's valid: %d", xSystem.MeasureStatus.PulseCounterInBkup);
				if (isPulseTrigger)
				{
					//Store to BKP register
					app_bkup_write_pulse_counter(xSystem.MeasureStatus.PulseCounterInBkup);
					isPulseTrigger = 0;
					DEBUG ("\r\n+++++++++ in %ums", pullDeltaMs);
				}
//			}
//			pulseLengthInMs = 0;
//		}
//		lastPulseState = curPulseState;
//	}
//	if(pulseLengthInMs) pulseLengthInMs++;
}
	
/*****************************************************************************/
/**
 * @brief	:  	Tick every 10ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t measureTimeout = 0;
void Measure_Tick(void)
{	
	if(xSystem.Status.InitSystemDone == 0) return;
		
	if(SensorUartBuffer.State)
	{
		SensorUartBuffer.State--;
		if(SensorUartBuffer.State == 0)
		{
			ProcessNewSensorUartData();
			SensorUartBuffer.BufferIndex = 0;
		}
	}
		
	/** Tick 1000ms */
	if(++Timeout1000msTick >= 100)
	{
		Timeout1000msTick = 0;
	}
}	

void MeasureTick1000ms(void)
{
	MeasureInputTick();
	static uint8_t StoreMeasureResultTick = 0;
	static uint16_t Measure420mATick = 0;
	
	if(xSystem.Status.ADCOut)
	{
		DEBUG ("\r\nADC: Vin: %d, Vsens: %d", ADC_RegularConvertedValueTab[ADCMEM_VSYS],
			ADC_RegularConvertedValueTab[ADCMEM_V20mV]); 
		DEBUG ("\r\nVin: %d mV, Vsens: %d mV", xSystem.MeasureStatus.Vin, xSystem.MeasureStatus.Vsens);
	}
			
	/* === DO DAU VAO 4-20mA DINH KY === //
	*/
	if(++Measure420mATick >= xSystem.Parameters.TGDoDinhKy)
	{
		Measure420mATick = 0;
		
		//Cho phep do thi moi do
		if(xSystem.Parameters.input.name.ma420)
		{
			SENS_420mA_PWR_ON();
			measureTimeout = 10;
		}
	}
	if(measureTimeout > 0)
	{
		measureTimeout--;
		if(measureTimeout == 0) {
			DEBUG ("\r\n--- Timeout measure ---");
			SENS_420mA_PWR_OFF();
		}
	}
	
	/* Save pulse counter to flash every 30s */
	if(++StoreMeasureResultTick >= 30)
	{
		StoreMeasureResultTick = 0;
		
		//Neu counter in BKP != in flash -> luu flash
		if(xSystem.MeasureStatus.PulseCounterInBkup != xSystem.MeasureStatus.PulseCounterInFlash)
		{
			xSystem.MeasureStatus.PulseCounterInFlash = xSystem.MeasureStatus.PulseCounterInBkup;
			InternalFlash_WriteMeasures();
			uint8_t res = InternalFlash_WriteConfig();
			DEBUG ("Save pulse counter %u to flash: %s\r\n", xSystem.MeasureStatus.PulseCounterInFlash, res ? "FAIL" : "OK"); 
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  	InitMeasurement
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void Measure_Init(void) 
{
	//Magnet switch
	gpio_init(SWITCH_IN_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, SWITCH_IN_PIN);
	
	//Pulse input
	gpio_init(SENS_PULSE_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, SENS_PULSE_PIN);
	
	 /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI0_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(SWITCH_PORT_SOURCE, SWITCH_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(SWITCH_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(SWITCH_EXTI_LINE);
	
#if 1
	 /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI2_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(SENS_PULSE_PORT_SOURCE, SENS_PULSE_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(SENS_PULSE_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(SENS_PULSE_EXTI_LINE);
#endif	
	
	/* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
	* -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
	*/
	xSystem.MeasureStatus.PulseCounterInBkup = app_bkup_read_pulse_counter();
	if(xSystem.MeasureStatus.PulseCounterInBkup < xSystem.MeasureStatus.PulseCounterInFlash) {
		xSystem.MeasureStatus.PulseCounterInBkup = xSystem.MeasureStatus.PulseCounterInFlash;
	}
	DEBUG ("Pulse counter in BKP: %d\r\n", xSystem.MeasureStatus.PulseCounterInBkup);
}

uint8_t isPowerOnRS485(void)
{
	return GPIO_ReadOutputDataBit(RS485_PWR_EN_PORT, RS485_PWR_EN_PIN);
}
	
/*
* Gia tri do thay doi 50mm tro len
*/
uint8_t isUltraValueChanged(int32_t lastVal)
{
//	int diff = xSystem.MeasureStatus.DistanceUltra - lastVal;
//	DEBUG ("\r\nDistance Ultra: %d - %d, diff: %d", xSystem.MeasureStatus.DistanceUltra, lastVal, diff);
//	if(diff > 50 || diff < -50) return 1;
	
	return 0;
}
	
/*
* Gia tri do thay doi 50mm tro len
*/
uint8_t isLaserValueChanged(int32_t lastVal)
{
//	int diff = xSystem.MeasureStatus.DistanceLaser - lastVal;
//	DEBUG ("\r\nDistance Laser: %d - %d, diff: %d", xSystem.MeasureStatus.DistanceLaser, lastVal, diff);
//	if(diff > 50 || diff < -50) return 1;
	
	return 0;
}

/*****************************************************************************/
/**
 * @brief	:  	Do dien ap nguon Solar va Battery. Tick every 1s
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
static void MeasureInputTick(void) 
{		
	//Dau vao 4-20mA: chi tinh toan khi thuc hien do
	if(measureTimeout > 0)
	{
		float V20mA = ADC_RegularConvertedValueTab[ADCMEM_V20mV] * ADC_VREF/ ADC_12BIT_FACTOR;
		xSystem.MeasureStatus.Input420mA = V20mA/124;	//R = 124
	}
	
//	if(xSystem.Status.ADCOut)
//	{
//		DEBUG ("\r\nV20mA: %d, Current 4-20mA: %d.%d, Distance: %d", 
//			(uint32_t)V20mA, iCurrent/10, iCurrent%10, distanceUltra);
//	}
//		
//	/* Neu dang do Ultrasound thi moi duyet ket qua do */
//	if(isPowerOnRS485())
//	{
//		DEBUG ("\r\nUtrasound distance: %u", distanceUltra);
//		
//		if(distanceUltra > 0)
//		{
//			xSystem.MeasureStatus.DistanceUltra = distanceUltra;
//			
//			//Neu co sai khac nhieu voi ket qua do cu thi gui ban tin len server
//			if(isUltraValueChanged(lastUltraMeasureVal))
//			{
//				DEBUG ("\r\n--- ULTRA VALUE changed, update cloud...");
//				xSystem.Status.YeuCauGuiTin = 1;
//				
//				RTC_WriteBackupRegister(ULTRA_LAST_VALUE_ADDR, 0x6789);
//			}
//			uint32_t lastUltraVal = RTC_ReadBackupRegister(ULTRA_LAST_VALUE_ADDR);
//			DEBUG ("\r\nRead last value from RTC: %X", lastUltraVal);
//			
//			lastUltraMeasureVal = xSystem.MeasureStatus.DistanceUltra;
//			
//			//B2. Gui ket qua do cho bang slave
//			sendDistanceTick = 2;
//		}
//	}
}

/*****************************************************************************/
/**
 * @brief	:  	Xu ly ban tin UART sensor laser
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
static void ProcessNewSensorUartData(void)
{
#if USE_SENSOR_LASER
	//Dinh dang du lieu tra ve: 0xFF abcdef (mm) ASCII, bao gom: space, "-", va 0 - 9
	//Tach lay du lieu dang so tu 0 - 9
	char digitBuff[10] = {0};
	uint8_t index = 0;
	
	DEBUG ("\r\nLaserSensor: %s", SensorUartBuffer.Buffer);
	
	if(SensorUartBuffer.Buffer[0] != 0xFF) return;
	
	//Reset timeout!
	measureLaserTimeout = 0;
	
	//Step1: Sensor return "----" => turn on laser to prepare measure
	if(strstr((char*)SensorUartBuffer.Buffer, "----")) {
		//Step2. Send command to measure
		UART_Putc(SENSOR_UART, 'O');
		return;
	}
	
	//Step3: check measurement result and send turn off command
	for(uint8_t i = 0; i < SensorUartBuffer.BufferIndex; i++)
	{
		if(SensorUartBuffer.Buffer[i] >= '0' && SensorUartBuffer.Buffer[i] <= '9')
		{
			digitBuff[index++] = SensorUartBuffer.Buffer[i];
			if(index ==10) break;
		}
	}
	if(index > 0)
	{
		uint32_t distanceLaserInMilimet = GetNumberFromString(0, digitBuff);
		DEBUG ("\r\nLaser distance: %d (mm)", distanceLaserInMilimet);
		
		xSystem.MeasureStatus.DistanceLaser = distanceLaserInMilimet;
		
		//Neu co sai khac nhieu voi ket qua do cu thi gui ban tin len server
		if(isLaserValueChanged(lastLaserMeasureVal))
		{
			DEBUG ("\r\n--- LASER VALUE changed, update cloud...");
			xSystem.Status.YeuCauGuiTin = 1;
		}
		lastLaserMeasureVal = xSystem.MeasureStatus.DistanceUltra;
					
		//Send command turn off laser
		UART_Putc(SENSOR_UART, 'U');
	}
#endif	//USE_SENSOR_LASER
}
	
uint16_t GetInputPower(void)
{    
	return xSystem.MeasureStatus.Vin;
}

static void Sensor_AddUartData(uint8_t Data) 
{ 
	SensorUartBuffer.Buffer[SensorUartBuffer.BufferIndex++] = Data;
	if(SensorUartBuffer.BufferIndex >= SMALL_BUFFER_SIZE)
		SensorUartBuffer.BufferIndex = 0;
	SensorUartBuffer.Buffer[SensorUartBuffer.BufferIndex] = 0;
	
    SensorUartBuffer.State = 7;
}

/*******************************************************************************
 * Function Name  	: USART1_IRQHandler 
 * Return         	: None
 * Parameters 		: None
 * Created by		: Phinht
 * Date created	: 28/02/2016
 * Description		: This function handles USART1 global interrupt request. 
 * Notes			: 
 *******************************************************************************/
void RS485_UART_Handler(void) 
{	
	if(usart_interrupt_flag_get(RS485_UART, USART_INT_FLAG_RBNE) != RESET)
	{
		usart_interrupt_flag_clear(RS485_UART, USART_INT_FLAG_RBNE);
		Sensor_AddUartData(USART_ReceiveData(RS485_UART));
	}
}


/*!
    \brief      this function handles external lines 2 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
#if 1
void EXTI2_IRQHandler(void)
{
    /* check the Pulse input pin */
   if (RESET != exti_interrupt_flag_get(EXTI_2))
	{
#if 1
		if(getPulseState()) 
		{
			beginPulseTime = sys_get_ms();
			pullState = 0;
		} 
		else if (pullState == 0)
		{
			pullState = -1;
			endPulseTime = sys_get_ms();
			if (endPulseTime > beginPulseTime)
			{
				pullDeltaMs = endPulseTime - beginPulseTime;
			}
			else
			{
				pullDeltaMs = 0xFFFFFFFF - beginPulseTime + endPulseTime;
			}
			
			if (pullDeltaMs > 200)
			{
				isPulseTrigger = 1;
				xSystem.MeasureStatus.PulseCounterInBkup++;
			}
		}
#else
		isPulseTrigger = 1;
#endif
      exti_interrupt_flag_clear(EXTI_2);
   }
}
#endif

/********************************* END OF FILE *******************************/
