#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#if 1
#include "main.h"
#define GSM_PWR_EN(x)			{	if (x) \
										LL_GPIO_SetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);	\
									else	\
										LL_GPIO_ResetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);	\
								}

#define GSM_PWR_RESET(x)		{	if (x) \
										LL_GPIO_SetOutputPin(GSM_RESET_GPIO_Port, GSM_RESET_Pin);	\
									else	\
										LL_GPIO_ResetOutputPin(GSM_RESET_GPIO_Port, GSM_RESET_Pin);	\
								}

#define GSM_PWR_KEY(x)			{	if (x) \
										LL_GPIO_SetOutputPin(GSM_PWR_KEY_GPIO_Port, GSM_PWR_KEY_Pin);	\
									else	\
										LL_GPIO_ResetOutputPin(GSM_PWR_KEY_GPIO_Port, GSM_PWR_KEY_Pin);	\
								}		

#define GSM_UART				0
#define RS485_UART				1

#ifdef HW_VERSION_0
#define LED1(x)             	{	if (x) \
										LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);	\
								}		

#define LED2(x)             	{	if (x) \
										LL_GPIO_ResetOutputPin(LED2_GPIO_Port, LED2_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(LED2_GPIO_Port, LED2_Pin);	\
									}	
#else
#define LED1(x)             	{	if (x) \
                                        LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);	\
                                    else	\
                                        LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);	\
                                }		

#define LED2(x)             	{	if (x) \
										LL_GPIO_SetOutputPin(LED2_GPIO_Port, LED2_Pin);	\
									else	\
										LL_GPIO_ResetOutputPin(LED2_GPIO_Port, LED2_Pin);	\
                                }	
#endif
				
#define TRANS_OUTPUT(x)      	{	if (x) \
										LL_GPIO_ResetOutputPin(TRANS_OUTPUT_GPIO_Port, TRANS_OUTPUT_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(TRANS_OUTPUT_GPIO_Port, TRANS_OUTPUT_Pin);	\
								}	                                
							
#define TRANS_1_OUTPUT(x)      	{	if (x) \
										LL_GPIO_ResetOutputPin(TRANS_OUT1_GPIO_Port, TRANS_OUT1_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(TRANS_OUT1_GPIO_Port, TRANS_OUT1_Pin);	\
								}	

#define TRANS_2_OUTPUT(x)      	{	if (x) \
										LL_GPIO_ResetOutputPin(TRANS_OUT2_GPIO_Port, TRANS_OUT2_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(TRANS_OUT2_GPIO_Port, TRANS_OUT2_Pin);	\
								}	

#define TRANS_3_OUTPUT(x)      	{	if (x) \
										LL_GPIO_ResetOutputPin(TRANS_OUT3_GPIO_Port, TRANS_OUT3_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(TRANS_OUT3_GPIO_Port, TRANS_OUT3_Pin);	\
								}	

#define TRANS_4_OUTPUT(x)      	{	if (x) \
										LL_GPIO_ResetOutputPin(TRANS_OUT4_GPIO_Port, TRANS_OUT4_Pin);	\
									else	\
										LL_GPIO_SetOutputPin(TRANS_OUT4_GPIO_Port, TRANS_OUT4_Pin);	\
								}								

#define ENABLE_NTC_POWER(x)     {	if (x) \
										LL_GPIO_SetOutputPin(VNTC_GPIO_Port, VNTC_Pin);	\
									else	\
										LL_GPIO_ResetOutputPin(VNTC_GPIO_Port, VNTC_Pin);	\
								}		

#define NTC_IS_POWERED()		LL_GPIO_IsOutputPinSet(VNTC_GPIO_Port, VNTC_Pin)						
								
								
#define ENABLE_INPUT_4_20MA_POWER(x)	{	if (x) \
												LL_GPIO_ResetOutputPin(EN_4_20MA_IN_GPIO_Port, EN_4_20MA_IN_Pin);	\
											else	\
												LL_GPIO_SetOutputPin(EN_4_20MA_IN_GPIO_Port, EN_4_20MA_IN_Pin);	\
										}

#define INPUT_POWER_4_20_MA_IS_ENABLE()       (LL_GPIO_IsOutputPinSet(EN_4_20MA_IN_GPIO_Port, EN_4_20MA_IN_Pin) ? 0 : 1)
                                        
#define ENABLE_OUTPUT_4_20MA_POWER(x)	{	if (x) \
												LL_GPIO_ResetOutputPin(ENABLE_OUTPUT_4_20MA_GPIO_Port, ENABLE_OUTPUT_4_20MA_Pin);	\
											else	\
												LL_GPIO_SetOutputPin(ENABLE_OUTPUT_4_20MA_GPIO_Port, ENABLE_OUTPUT_4_20MA_Pin);	\
										}	

#define RS485_POWER_EN(x)				{	if (x) \
												LL_GPIO_ResetOutputPin(RS485_EN_GPIO_Port, RS485_EN_Pin);	\
											else	\
												LL_GPIO_SetOutputPin(RS485_EN_GPIO_Port, RS485_EN_Pin);	\
										}	
#define RS485_DIR_TX()                  {   \
                                            LL_GPIO_SetOutputPin(RS485_DIR_GPIO_Port, RS485_DIR_Pin);     \
                                        }
   
#define RS485_DIR_RX()                  {   \
                                            LL_GPIO_ResetOutputPin(RS485_DIR_GPIO_Port, RS485_DIR_Pin);     \
                                        }                                        
#define RS485_GET_DIRECTION()               LL_GPIO_IsOutputPinSet(RS485_DIR_GPIO_Port, RS485_DIR_Pin)
#endif

#ifdef DTG02
#define BUZZER(x)                       {	if (x) \
												LL_GPIO_ResetOutputPin(BUZZER_GPIO_Port, BUZZER_Pin);	\
											else	\
												LL_GPIO_SetOutputPin(BUZZER_GPIO_Port, BUZZER_Pin);	\
										}   
#endif                                        
                                        
#define ADC_NUMBER_OF_CONVERSION_TIMES		10
//#define V_OFFSET_4_20MA_CHANNEL_0_MV        1
#define ADC_VBAT_RESISTOR_DIV				2
#define ADC_VIN_RESISTOR_DIV				7911

#ifdef DTG02
#define ADC_CHANNEL_DMA_COUNT				9
//#define ADC_VREF							3300

#define V_INPUT_3_4_20MA_CHANNEL_INDEX		0
#define VIN_24V_CHANNEL_INDEX				1
#define V_INPUT_2_4_20MA_CHANNEL_INDEX		2
#define V_INPUT_1_4_20MA_CHANNEL_INDEX		3
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		4
#define VBAT_CHANNEL_INDEX					5
#define V_NTC_TEMP_CHANNEL_INDEX				6
#define V_INTERNAL_CHIP_TEMP_CHANNEL_INDEX  7
#define V_REF_CHANNEL_INDEX                 8
//#define V_OFFSET_4_20MA_CHANNEL_1_MV        1
//#define V_OFFSET_4_20MA_CHANNEL_2_MV        1
//#define V_OFFSET_4_20MA_CHANNEL_3_MV        1
#else
#define ADC_CHANNEL_DMA_COUNT				6
//#define ADC_VREF							3300

#define VBAT_CHANNEL_INDEX					0
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		1
#define VIN_24V_CHANNEL_INDEX				2	
#define V_NTC_TEMP_CHANNEL_INDEX				3
#define V_INTERNAL_CHIP_TEMP_CHANNEL_INDEX  4
#define V_REF_CHANNEL_INDEX					5
#endif
#define VREF_OFFSET_MV						80
#define ADC_INPUT_4_20MA_GAIN               (50.0f)   

#define MEASURE_INPUT_PORT_0		0

#ifdef DTG02
#define NUMBER_OF_INPUT_4_20MA						4
#define MEASURE_NUMBER_OF_WATER_METER_INPUT			2
#define	NUMBER_OF_INPUT_ON_OFF						4
#define NUMBER_OF_OUTPUT_ON_OFF						4
#define MEASURE_INPUT_PORT_1		                1
#else
#define	NUMBER_OF_OUTPUT_ON_OFF						1
#define NUMBER_OF_INPUT_4_20MA						1
#define NUMBER_OF_OUTPUT_4_20MA                     1
#define MEASURE_NUMBER_OF_WATER_METER_INPUT			1		
#endif
#define MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN         0
#define MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN         1
#define MEASUREMENT_MAX_MSQ_IN_RAM                  24

#define MEASUREMENT_QUEUE_STATE_IDLE                0
#define MEASUREMENT_QUEUE_STATE_PENDING             1       // Dang cho de doc
#define MEASUREMENT_QUEUE_STATE_PROCESSING          2       // Da doc nhung dang xu li, chua release thanh free

#define RS485_DATA_TYPE_INT16						0
#define RS485_DATA_TYPE_INT32						1
#define RS485_DATA_TYPE_FLOAT						2
#define RS485_MAX_SLAVE_ON_BUS						2
#define RS485_MAX_REGISTER_SUPPORT					16
#define RS485_REGISTER_ADDR_TOP						50000

#define APP_EEPROM_VALID_FLAG		0x15234519
#define APP_EEPROM_SIZE				(6*1024)

#define APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN		0 // Meter mode 0 : PWM++, DIR--
#define APP_EEPROM_METER_MODE_ONLY_PWM				1 // Meter mode 1 : PWM++
#define APP_EEPROM_METER_MODE_PWM_F_PWM_R			2 // Meter mode 2 : PWM_F & PWM_R
#define APP_EEPROM_METER_MODE_MAX_ELEMENT           2
#define APP_EEPROM_MAX_PHONE_LENGTH                 16
#define APP_EEPROM_MAX_SERVER_ADDR_LENGTH           64
#define APP_EEPROM_MAX_NUMBER_OF_SERVER				2
#ifdef DTG02
#define APP_EEPROM_NB_OF_INPUT_4_20MA               4
#else
#define APP_EEPROM_NB_OF_INPUT_4_20MA               1
#endif
#define APP_EEPROM_MAX_SYMBOL_LENGTH				6

#define APP_FLASH_VALID_DATA_KEY                    0x12345678               
#ifdef DTG01
#define APP_FLASH_NB_OFF_4_20MA_INPUT               2
#define APP_FLASH_NB_OF_METER_INPUT                 1
#else
#define APP_FLASH_NB_OFF_4_20MA_INPUT               NUMBER_OF_INPUT_4_20MA
#define APP_FLASH_NB_OF_METER_INPUT                 APP_EEPROM_METER_MODE_MAX_ELEMENT
#endif
#define APP_FLASH_RS485_MAX_SIZE                    (RS485_MAX_REGISTER_SUPPORT)
#define APP_SPI_FLASH_SIZE						    (1024*1024)
#define APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG  0xA5A5A5A5
#define APP_FLASH_DATA_HEADER_KEY                   0x9813567A


typedef struct
{
    uint32_t value[RS485_MAX_REGISTER_SUPPORT];
	uint8_t unit[APP_EEPROM_MAX_SYMBOL_LENGTH];
    uint16_t register_index;
    int8_t nb_of_register;
    uint8_t slave_addr;
	uint8_t data_type;
} __attribute__((packed)) measure_input_modbus_register_t;

typedef struct
{
	uint32_t forward;
	uint32_t reserve;
} __attribute__((packed)) measure_input_counter_t;

typedef struct
{
    uint32_t current_ma_mil_10;		// 4ma =>> 400
    int32_t adc_mv;					    // adc voltage
} input_4_20ma_lookup_t;

static const input_4_20ma_lookup_t lookup_table_4_20ma_input[] =
{
    {  0,        400      },
    {  400,      490    },
    {  500,      591    },  
    {  600,      688    },   
    {  710,      800    },     
    {  800,      890    },    
    {  900,      992    },    
    {  1100,     1098   },  
    {  1290,     1385   },    
    {  1525,     1617   },     
    {  1823,     1915   },     
    {  2000,     2100   },
};
                                  
                                  
#endif /* __HARDWARE_H__ */
