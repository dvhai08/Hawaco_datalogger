/******************************************************************************
 * @file    	GSM_HardwareLayer.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <string.h>
#include "Net_Config.h"
#include "Main.h"
#include "GSM.h"
#include "Hardware.h"
#include "HardwareManager.h"
#include "DataDefine.h"
#include "MQTTUser.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
GSM_Hardware_t GSM_Hardware;
static uint8_t CMEErrorCount;
static uint16_t CMEErrorTimeout = 0;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
GSM_Manager_t GSM_Manager;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void InitGSM(void) 
{ 
	gpio_init(GSM_PWR_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GSM_PWR_EN_PIN);
	gpio_init(GSM_PWR_KEY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GSM_PWR_KEY_PIN);
	gpio_init(GSM_RESET_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GSM_RESET_PIN);
	
	gpio_bit_reset(GSM_PWR_EN_PORT, GSM_PWR_EN_PIN);			//Tat nguon module
	gpio_bit_reset(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN);
	gpio_bit_reset(GSM_RESET_PORT, GSM_RESET_PIN);
	
#if 0		//HW co RI pin
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;	
	GPIO_InitStructure.GPIO_Pin = GSM_RI_PIN;						
	GPIO_Init(GSM_RI_PORT, &GPIO_InitStructure);
	
	 /* Connect GSM_RI_PIN Line to PB6 pin */
	SYSCFG_EXTILineConfig(GSM_RI_PORT_SOURCE, GSM_RI_PIN_SOURCE);
	
	/* Configure EXTI6 line */
	EXTI_InitStructure.EXTI_Line = GSM_EXTI_LINE;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
	
	/* Enable and set EXTI6 Interrupt priority */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif	//0

#if 1	//HW co RI pin
    /* configure RI (PA11) pin as input */
    gpio_init(GSM_RI_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GSM_RI_PIN);
    
    /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI10_15_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(GSM_RI_PORT_SOURCE, GSM_RI_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(GSM_RI_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(GSM_RI_EXTI_LINE);
#endif	//__RI_INTERRUPT__
	
	/* Cap nguon cho Module */	
	Delayms(3000);
	gpio_bit_set(GSM_PWR_EN_PORT, GSM_PWR_EN_PIN);
	Delayms(1000);	//Cho VBAT on dinh
	
	/* Tao xung |_| de Power On module, min 1s  */
	gpio_bit_set(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN);
	Delayms(1000);
	gpio_bit_reset(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN);

	/* Khoi tao UART cho GSM modem */
	UART_Init(GSM_UART, 115200);	
	
	GSM_Manager.TimeOutOffAfterReset = 90;
}

void GSM_PowerControl(uint8_t State)
{
	if(State == GSM_OFF)
	{
		//Neu dang trong qua trinh update FW -> Khong OFF
		if(xSystem.FileTransfer.State != FT_NO_TRANSER) 
			return;
		
		GSM_Manager.isGSMOff = 1;
		if(GSM_Manager.GSMReady)
		{
			/* Tao xung |_| de Power Off module, min 1s  */
			GPIO_WriteBit(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN, 1);
			Delayms(1500);	
			GPIO_WriteBit(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN, 0);
		}
		
		Delayms(5000);
		//Ngat nguon VBAT 4.2V
		GPIO_WriteBit(GSM_PWR_EN_PORT, GSM_PWR_EN_PIN, 0);
		
		//Reset cac bien
		GSM_Manager.Mode = GSM_AT_MODE;
		GSM_Manager.GSMReady = 0;
		GSM_Manager.RISignal = 0;
		xSystem.Status.ServerState = NOT_CONNECTED;
		xSystem.Status.MQTTServerState = MQTT_INIT;
		
		//Disable luon UART
		usart_disable(GSM_UART);
		usart_deinit(GSM_UART);
	}
	else if(State == GSM_ON)
	{
		GSM_Manager.isGSMOff = 0; 
		
		//Khoi tao lai UART
		UART_Init(GSM_UART, 115200);
		
		/* Cap nguon 4.2V */
		gpio_bit_set(GSM_PWR_EN_PORT, GSM_PWR_EN_PIN);
		Delayms(1000);	//Wait for VBAT stable
		
		/* Tao xung |_| de Power On module, min 1s  */
		gpio_bit_set(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN);
		Delayms(1000);
		gpio_bit_reset(GSM_PWR_KEY_PORT, GSM_PWR_KEY_PIN);
			
		GSM_Manager.FirstTimePower = 0;
	}
}

/*****************************************************************************/
/*						CAC HAM PHUC VU CHO PPP STACK						 */	
/*****************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  	Init GSM serial
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void init_serial (void) 
{
	GSM_Hardware.Modem.RxBuffer.IndexIn = 0;	
	GSM_Hardware.Modem.RxBuffer.IndexOut = 0;
	GSM_Hardware.Modem.TxBuffer.IndexIn = 0;
	GSM_Hardware.Modem.TxBuffer.IndexOut = 0;
	GSM_Hardware.Modem.TX_Active = __FALSE;
	
	/* Enable USART2 interrupts. */
	NVIC_EnableIRQ(GSM_UART_IRQ);	
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface in PPP mode
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_putchar (uint8_t c) 
{
	ModemBuffer_t *p = &GSM_Hardware.Modem.TxBuffer;

	if(GSM_Manager.Mode == GSM_AT_MODE) return __FALSE;
	
	/* Write a byte to serial interface */
	if ((uint8_t)(p->IndexIn + 1) == p->IndexOut) 
	{
		/* Serial transmit buffer is full. */
		return (__FALSE);
	}
	
	/* Disable ngat USART2 */
	NVIC_DisableIRQ(GSM_UART_IRQ);
	__nop();
	
	if (GSM_Hardware.Modem.TX_Active == __FALSE) 
	{
		/* Send directly to UART. */
//		GSM_UART->DR   = (U8)c;
//		GSM_UART->CR1 |= 0x0080;			/* Enable TXE interrupt */
		USART_DATA(GSM_UART) = ((uint16_t)USART_DATA_DATA & c);
		usart_interrupt_enable(GSM_UART, USART_INT_TBE);		/* Enable TBE interrupt */
		
		GSM_Hardware.Modem.TX_Active = __TRUE;
	}
	else 
	{
		/* Add data to transmit buffer. */
		p->Buffer [p->IndexIn++] = c;
        if(p->IndexIn == MODEM_BUFFER_SIZE) p->IndexIn = 0;
	}
	
	/* Enable Ngat USART2 tro lai */
	NVIC_EnableIRQ(GSM_UART_IRQ);
	
	return (__TRUE);
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface in AT mode 
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_put_at_character (uint8_t c) 
{
	ModemBuffer_t *p = &GSM_Hardware.Modem.TxBuffer;
	
	/* Write a byte to serial interface */
	if ((uint8_t)(p->IndexIn + 1) == p->IndexOut) 
	{
		/* Serial transmit buffer is full. */
		return (__FALSE);
	}
	
	/* Disable ngat USART2 */
	NVIC_DisableIRQ(GSM_UART_IRQ);
	__nop();
	
	if (GSM_Hardware.Modem.TX_Active == __FALSE) 
	{
		/* Send directly to UART. */
//		GSM_UART->DR   = (U8)c;
//		GSM_UART->CR1 |= 0x0080;			/* Enable TXE interrupt */
		USART_DATA(GSM_UART) = ((uint16_t)USART_DATA_DATA & c);
		usart_interrupt_enable(GSM_UART, USART_INT_TBE);		/* Enable TXE interrupt */
		
		GSM_Hardware.Modem.TX_Active = __TRUE;
	}
	else 
	{
		/* Add data to transmit buffer. */
		p->Buffer [p->IndexIn++] = c;
		if(p->IndexIn == MODEM_BUFFER_SIZE) p->IndexIn = 0;
	}
	
	/* Enable Ngat USART2 tro lai */
	NVIC_EnableIRQ(GSM_UART_IRQ);
	
	return (__TRUE);
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_put_at_string (char *str) 
{
	while(*str)
	{
		com_put_at_character (*str++);
	}
	
	return __TRUE;
}
/*****************************************************************************/
/**
 * @brief	:  	Read a byte from GSM serial interface
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
int com_getchar (void) 
{
    uint8_t rtnValue;
    
	/* Read a byte from serial interface */
	ModemBuffer_t *p = &GSM_Hardware.Modem.RxBuffer;

	if (p->IndexIn == p->IndexOut) 
	{
		/* Serial receive buffer is empty. */
		return (-1);
	}
	
    rtnValue = p->Buffer[p->IndexOut++];
    
    if(p->IndexOut == MODEM_BUFFER_SIZE) p->IndexOut = 0;
        
	return rtnValue;
}

/*****************************************************************************/
/**
 * @brief	:  	Init GSM serial
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void GSM_UARTHandler(void) 
{
	/* Serial Rx and Tx interrupt handler. */
	ModemBuffer_t *p;

	if(usart_flag_get(GSM_UART, USART_FLAG_RBNE) == 1)		//RBNE = 1
//	if (GSM_UART->SR & 0x0020) //RXNE = 1
	{
		uint8_t ch = usart_data_receive(GSM_UART);
		GSM_Manager.TimeOutConnection = 0;				//Reset thoi gian timeout
		
		/* Receive Buffer Not Empty */
		p = &GSM_Hardware.Modem.RxBuffer;
		if ((U8)(p->IndexIn + 1) != p->IndexOut) 
		{
			p->Buffer [p->IndexIn++] = ch;
            if(p->IndexIn == MODEM_BUFFER_SIZE) p->IndexIn = 0;
		}
//		USART_ClearITPendingBit(GSM_UART, USART_IT_RXNE);		
		usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
	}
	
	if ((usart_flag_get(GSM_UART, USART_FLAG_TBE) == 1) && (GSM_Hardware.Modem.TX_Active == __TRUE)) //TBE = 1
//	if ((GSM_UART->SR & 0x0080) && (GSM_Hardware.Modem.TX_Active == __TRUE)) //TXE = 1
	{
		//USART_ClearITPendingBit(GSM_UART, USART_IT_TXE);
		usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);
		
		/* Transmit Data Register Empty */
		p = &GSM_Hardware.Modem.TxBuffer;
		if (p->IndexIn != p->IndexOut) 
		{
//			USART_SendData(GSM_UART ,(uint8_t)(p->Buffer [p->IndexOut++]));
			usart_data_transmit(GSM_UART, (p->Buffer [p->IndexOut++]));
         if(p->IndexOut == MODEM_BUFFER_SIZE) p->IndexOut = 0;
		}
		else 
		{
//			USART_ITConfig(GSM_UART, USART_IT_TXE, DISABLE);
			usart_interrupt_disable(GSM_UART, USART_INT_TBE);		/* Disable TXE interrupt */
			GSM_Hardware.Modem.TX_Active = __FALSE;
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  Return status Transmitter active/not active. When transmit buffer is empty, 'tx_active' is FALSE.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */	
BOOL com_tx_active (void) 
{
	return GSM_Hardware.Modem.TX_Active;
}
	
/*****************************************************************************/
/**
 * @brief	:  	Initializes the modem variables and control signals DTR & RTS.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */	
void modem_init () 
{
	GSM_Hardware.Modem.State = MODEM_IDLE;
}

/*****************************************************************************/
/**
 * @brief	:  	Modem dial target number 'dialnum'
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_dial (uint8_t *dialnum) 
{
	GSM_Hardware.Modem.DialNumber = dialnum;
	GSM_Hardware.Modem.State = MODEM_DIAL;
}
/*****************************************************************************/
/**
 * @brief	:  	This function puts Modem into Answering Mode
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_listen () 
{
}

/*****************************************************************************/
/**
 * @brief	:  	This function clears DTR to force the modem to hang up if 
 *				it was on line and/or make the modem to go to command mode.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_hangup () 
{
}
/*****************************************************************************/
/**
 * @brief	:  	Checks if the modem is online. Return false when not
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL modem_online() 
{
	if(GSM_Hardware.Modem.State == MODEM_ONLINE) 
	{
		return __TRUE;
	}
	return __FALSE;
}


/*****************************************************************************/
/**
 * @brief	: This is a main thread for MODEM Control module. It is called on every 
 *				system timer timer tick to implement delays easy (called by TCPNet system)
 *				By default this is every 100ms. The 'sytem tick' timeout is set in 'Net_Config.c'
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_run () 
{		
	if(GSM_Hardware.ATCommand.TimeoutATC)
	{
		GSM_Hardware.ATCommand.CurrentTimeoutATC++;
		
		if(GSM_Hardware.ATCommand.CurrentTimeoutATC > GSM_Hardware.ATCommand.TimeoutATC)
		{
			GSM_Hardware.ATCommand.CurrentTimeoutATC = 0;
			
			if(GSM_Hardware.ATCommand.RetryCountATC)
			{
				GSM_Hardware.ATCommand.RetryCountATC--;
			}
			
			if(GSM_Hardware.ATCommand.RetryCountATC == 0)
			{
				GSM_Hardware.ATCommand.TimeoutATC = 0;
                
				if(GSM_Hardware.ATCommand.SendATCallBack != NULL)
				{
					GSM_Hardware.ATCommand.SendATCallBack(EVEN_TIMEOUT,NULL);				
				}
			}				
			else
			{
				DEBUG("\rResend ATC: %s",GSM_Hardware.ATCommand.CMD);
				com_put_at_string(GSM_Hardware.ATCommand.CMD);
			}			
		}
		
		if(strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),GSM_Hardware.ATCommand.ExpectResponseFromATC))
		{
			GSM_Hardware.ATCommand.TimeoutATC = 0;
            
			if(GSM_Hardware.ATCommand.SendATCallBack != NULL)
			{
				GSM_Hardware.ATCommand.SendATCallBack(EVEN_OK,GSM_Hardware.ATCommand.ReceiveBuffer.Buffer);
			}			
		}		
	}
	
	/* Xu ly time out +CME ERROR */
	if(CMEErrorCount > 0)
	{
		CMEErrorTimeout++;
		if(CMEErrorTimeout > 350)	/* Trong vong 35s xuat hien >= 10 lan +CME ERROR */
		{
			CMEErrorTimeout = 0;
			if(CMEErrorCount >= 10)
			{
				DEBUG("\r+CME ERROR too much!");
				GSM_Manager.State = GSM_RESET;
				GSM_Manager.Step = 0;
			}
			CMEErrorCount = 0;
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  	Modem character process event handler. This function is called when
 *				a new character has been received from the modem in command mode 
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL modem_process (uint8_t ch) 
{
	if(GSM_Hardware.ATCommand.TimeoutATC)
	{
		GSM_Hardware.ATCommand.ReceiveBuffer.Buffer[GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex++] = ch;
		GSM_Hardware.ATCommand.ReceiveBuffer.Buffer[GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex] = 0;
	}
	else
	{
#if __HOPQUY_GSM__
		UART_Putc(DEBUG_UART, ch);		//DEBUG GSM : Hien thi noi dung gui ra tu module GSM
#endif	/* __HOPQUY_GSM__ */
		
		GSM_Hardware.ATCommand.ReceiveBuffer.Buffer[GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex++] = ch;
				
		if(ch == '\r' || ch == '\n')
		{
			/* Xu ly du lieu tien +CUSD: 0, "MobiQTKC:20051d,31/07/2016 KM2:50000d KM3:48500d KM1:27800d", 15 */
			if(strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),"+CUSD"))
			{
				DEBUG("\rTai khoan: %s", strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),"+CUSD"));
				MQTT_SendBufferToServer((char*)GSM_Hardware.ATCommand.ReceiveBuffer.Buffer, "CUSD");
			}
			
			/* Xu ly ban tin +CME ERROR */
			if(strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),"+CME ERROR:"))
			{
				CMEErrorCount++;
			}
			
			GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex = 0;
			memset(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer,0,SMALL_BUFFER_SIZE);
		}
	}
		
	/* A connection has been established; the DCE is moving from command state to online data state */
	if(strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),"CONNECT"))
	{		
		GSM_Hardware.Modem.State = MODEM_ONLINE;
		GSM_Manager.GSMReady = 1;
		return (__TRUE);
	}
	
#if (__HOPQUY_GSM__ == 0)
	/* The connection has been terminated or the attempt to establish a connection failed */
	if(strstr((char *)(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer),"NO CARRIER"))
	{
		memset(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer, 0, 20);

		/* Khi goto sleep -> NOCARRIER -> Khong mo lai PPP nua de vao SLEEP mode */
		if(GSM_Manager.State != GSM_SLEEP && GSM_Manager.State != GSM_GOTOSLEEP && 	
			GSM_Manager.State != GSM_READSMS)
		{
			DEBUG("\rModem disconnected, auto reconnect"); 
			GSM_Manager.State = GSM_REOPENPPP;
			GSM_Manager.Step = 0;
			GSM_Manager.GSMReady = 0;
		}
		else
		{
			DEBUG("\rModem disconnect because of go to sleep mode!");
		}
  }
#endif
	
	return (__FALSE);
}


/*****************************************************************************/
/**
 * @brief	:  	Send a command to GSM module
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void SendATCommand  (char *Command, char *ExpectResponse, uint16_t Timeout, 
						uint8_t RetryCount, GSM_SendATCallBack_t CallBackFunction)
{
	if(Timeout == 0 || CallBackFunction == NULL)
	{
		com_put_at_string(Command);
		return;
	}
		
	GSM_Hardware.ATCommand.CMD = Command;
	GSM_Hardware.ATCommand.ExpectResponseFromATC = ExpectResponse;
	GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex = 0;
	GSM_Hardware.ATCommand.ReceiveBuffer.State = BUFFER_STATE_IDLE;
	GSM_Hardware.ATCommand.RetryCountATC = RetryCount;
	GSM_Hardware.ATCommand.SendATCallBack = CallBackFunction;
	GSM_Hardware.ATCommand.TimeoutATC = Timeout / 100;
	GSM_Hardware.ATCommand.CurrentTimeoutATC = 0;
	
	mem_set(GSM_Hardware.ATCommand.ReceiveBuffer.Buffer,0,SMALL_BUFFER_SIZE);
	GSM_Hardware.ATCommand.ReceiveBuffer.BufferIndex = 0;
	GSM_Hardware.ATCommand.ReceiveBuffer.State = BUFFER_STATE_IDLE;
	
	com_put_at_string(Command);
	
	DEBUG("\rATC: %s",Command);
}

/********************************* END OF FILE *******************************/
