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

#define RS485_EN(x)						{	if (x) \
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
#define GAIN_INPUT_4_20MA_IN				143
#define VREF_OFFSET_MV						80
                                        
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
