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
								
#define SENS_420mA_PWR_OFF()		LL_GPIO_SetOutputPin(ENABLE_OUTPUT_4_20MA_GPIO_Port, ENABLE_OUTPUT_4_20MA_Pin);								
#define SENS_420mA_PWR_ON()			LL_GPIO_ResetOutputPin(ENABLE_OUTPUT_4_20MA_GPIO_Port, ENABLE_OUTPUT_4_20MA_Pin);		
							
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
								
								
#define ENABLE_INOUT_4_20MA_POWER(x)	{	if (x) \
												LL_GPIO_ResetOutputPin(EN_4_20MA_IN_GPIO_Port, EN_4_20MA_IN_Pin);	\
											else	\
												LL_GPIO_SetOutputPin(EN_4_20MA_IN_GPIO_Port, EN_4_20MA_IN_Pin);	\
										}

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
#define RS485_DIR_IS_TX()               LL_GPIO_IsOutputPinSet(RS485_DIR_GPIO_Port, RS485_DIR_Pin)
#endif


#endif /* __HARDWARE_H__ */
