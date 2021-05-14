#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include "gd32e10x.h"
#include "gd32e10x_gpio.h"
#include "gd32e10x_usart.h"

#define RCC_GetFlagStatus	rcu_flag_get
#define GPIO_ReadOutputDataBit              gpio_output_bit_get
#define GPIO_ReadInputDataBit               gpio_input_bit_get
#define GPIO_SetBits                       gpio_bit_set
#define	GPIO_ResetBits							gpio_bit_reset
#define	GPIO_WriteBit								gpio_bit_write
#define BitAction                           bit_status

#define USART_GetITStatus(x, y)             usart_interrupt_flag_get((uint32_t)x, y)
#define USART_IT_RXNE                       USART_INT_FLAG_RBNE
#define USART_ReceiveData(x)                usart_data_receive((uint32_t)x)
#define USART_GetFlagStatus(x, y)           usart_flag_get((uint32_t)x, y)
#define USART_ClearFlag(x,y)                usart_flag_clear((uint32_t)x, y)
#define USART_FLAG_ORE                      USART_FLAG_ORERR
#define USART_FLAG_FE                       USART_FLAG_FERR

#define __USE_SENSOR_UART__	0	

/** Giao tiep voi mach sensor qua UART hay qua dem xung input */
#if __USE_SENSOR_UART__
/************************* Defines for USART1 port ***************************/
#define DEGBUG_UART				USART1
#define	DEBUG_UART_CLK	RCU_USART1

#define DEBUG_TX_PIN_SOURCE GPIO_PinSource2
#define DEBUG_RX_PIN_SOURCE GPIO_PinSource3

#define DEBUG_TX_PIN GPIO_PIN_2
#define DEBUG_RX_PIN GPIO_PIN_3

#define DEBUG_UART_GPIO GPIOA
#define DEBUG_UART_IRQ USART1_IRQn
#else
#define	SENS_PULSE_PORT	GPIOA
#define	SENS_PULSE_PIN	GPIO_PIN_2
#define	SENS_PULSE_PORT_SOURCE 	GPIO_PORT_SOURCE_GPIOA
#define SENS_PULSE_PIN_SOURCE 	GPIO_PIN_SOURCE_2
#define SENS_PULSE_EXTI_LINE 	EXTI_2
#define getPulseState() GPIO_ReadInputDataBit(SENS_PULSE_PORT, SENS_PULSE_PIN)

#define	SENS_DIR_PORT	GPIOA
#define	SENS_DIR_PIN	GPIO_PIN_3

#endif	//__USE_SENSOR_UART__

/************************* Defines for USART0 port ***************************/
#define GSM_UART				USART0
#define	GSM_UART_CLK	RCU_USART0

#define GSM_TX_PIN GPIO_PIN_9
#define GSM_RX_PIN GPIO_PIN_10

#define GSM_UART_GPIO GPIOA
#define GSM_UART_IRQ USART0_IRQn
#define	GSM_UART_Handler	USART0_IRQHandler

//Dieu khien GSM Power Enable, active 1
#define GSM_PWR_EN_PIN GPIO_PIN_15
#define GSM_PWR_EN_PORT GPIOA
#define GSM_PWR_EN(x) GPIO_WriteBit(GSM_PWR_EN_PORT, GSM_PWR_EN_PIN, (BitAction)x)

//Dieu khien GSM reset, active 1
#define GSM_RESET_PIN GPIO_PIN_14
#define GSM_RESET_PORT GPIOB
#define GSM_PWR_RESET(x) GPIO_WriteBit(GSM_RESET_PORT, GSM_RESET_PIN, (BitAction)x)

//Dieu khien GSM Power key, active 1
#define GSM_PWR_KEY_PIN GPIO_PIN_12
#define GSM_PWR_KEY_PORT GPIOB
#define GSM_PWR_KEY(x) GPIO_WriteBit(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN, (BitAction)x)

//Chan GSM RI
#define GSM_RI_PIN GPIO_PIN_11
#define GSM_RI_PORT GPIOA
#define GSM_RI_PORT_SOURCE 	GPIO_PORT_SOURCE_GPIOA
#define GSM_RI_PIN_SOURCE 	GPIO_PIN_SOURCE_11
#define GSM_RI_EXTI_LINE 	EXTI_11

#define GSM_STATUS_PIN GPIO_PIN_12
#define GSM_STATUS_PORT GPIOA
/************************* Defines for USART2 for RS485 ***************************/
#define RS485_UART USART2
#define	RS485_UART_CLK		RCU_USART2

#define RS485_TX_PIN GPIO_PIN_10
#define RS485_RX_PIN GPIO_PIN_11

#define RS485_UART_GPIO GPIOB
#define RS485_UART_IRQ USART2_IRQn
#define	RS485_UART_Handler	USART2_IRQHandler

//Cap nguon cho ngoai vi RS485, active 0
#define	RS485_PWR_EN_PIN	GPIO_PIN_0
#define	RS485_PWR_EN_PORT	GPIOB
#define	RS485_PWR_ON()	 GPIO_ResetBits(RS485_PWR_EN_PORT, RS485_PWR_EN_PIN)
#define	RS485_PWR_OFF()	 GPIO_SetBits(RS485_PWR_EN_PORT, RS485_PWR_EN_PIN)

#define	RS485_DR_PIN	GPIO_PIN_13
#define	RS485_DR_PORT	GPIOB

//Detect ngoai vi RS485, A & B = 1 => Not detect
#define	RS485_DETECTA_PIN	GPIO_PIN_7
#define	RS485_DETECTA_PORT	GPIOA

#define	RS485_DETECTB_PIN	GPIO_PIN_15
#define	RS485_DETECTB_PORT	GPIOB


/************************* Other IOs ***************************/
//Cac chan dieu khien LED - active 0
#define LED1_PORT GPIOB
#define LED1_PIN GPIO_PIN_5
#define LED1(x) GPIO_WriteBit(LED1_PORT, LED1_PIN, (BitAction)x)

#define LED2_PORT GPIOB
#define LED2_PIN GPIO_PIN_6
#define LED2(x) GPIO_WriteBit(LED2_PORT, LED2_PIN, (BitAction)x)

//Dieu khien tran output, active 1
#define	TRAN_OUT_PORT	GPIOB
#define	TRAN_OUT_PIN	GPIO_PIN_1
#define	TRAN_OUTPUT(x)	GPIO_WriteBit(TRAN_OUT_PORT, TRAN_OUT_PIN, (BitAction)x)

//Cac chan  input - active 0
#define SWITCH_IN_PIN GPIO_PIN_0
#define SWITCH_IN_PORT GPIOA
#define	SWITCH_PORT_SOURCE 	GPIO_PORT_SOURCE_GPIOA
#define SWITCH_PIN_SOURCE 	GPIO_PIN_SOURCE_0
#define SWITCH_EXTI_LINE 	EXTI_0
#define getSwitchState() GPIO_ReadInputDataBit(SWITCH_IN_PORT, SWITCH_IN_PIN)


////==================== ADC ===================//
//#define ADC1_DR_Address 0x40012440

///* constant for adc resolution is 12 bit = 4096 */
#define ADC_12BIT_FACTOR 4096

///* constant for adc threshold value 3.3V */
#define ADC_VREF 3300
#define V_DIODE_DROP 250

#define ADC_VIN_PORT   GPIOA
#define ADC_VIN_PIN    GPIO_PIN_6
#define ADC_VIN_CHANNEL   ADC_CHANNEL_6		//ADC01_IN6

//4-20mA input
#define ADC_SENS_PIN GPIO_PIN_4
#define ADC_SENS_PORT GPIOA
#define ADC_SENS_CHANNEL ADC_CHANNEL_4		//ADC01_IN4

//Dieu khien cap nguon cho Sensor 4-20mA, active 0
#define SENS_PWR_EN_PORT GPIOA
#define SENS_PWR_EN_PIN GPIO_PIN_1
#define	SENS_420mA_PWR_ON()	 gpio_bit_reset(SENS_PWR_EN_PORT, SENS_PWR_EN_PIN)
#define	SENS_420mA_PWR_OFF()	 gpio_bit_set(SENS_PWR_EN_PORT, SENS_PWR_EN_PIN)

// DAC cho 4-20mA ouput, range 0.6 - 3V
#define	DAC_OUT_PORT		GPIOA
#define	DAC_OUT_PIN		GPIO_PIN_5	//DAC_OUT1


#endif /* __HARDWARE_H__ */
