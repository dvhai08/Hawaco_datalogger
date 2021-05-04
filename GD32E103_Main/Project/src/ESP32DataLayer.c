/******************************************************************************
 * @file    	Transfer.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	03/03/2016
 * @brief   	Dung de trao doi du lieu voi Blackbox
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Hardware.h"
#include "DataDefine.h"
#include "Transfer.h"
#include "Utilities.h"
#include "MeasureInput.h"
#include "HardwareManager.h"
#include "Version.h"
#include "InternalFlash.h"
#include "Main.h"
#include "DriverUART.h"
#include "DataDefine.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern __IO uint16_t adc_vin_value;
/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
SmallBuffer_t ESPBuffer;
SmallBuffer_t FMBuffer;

uint8_t esp32RxMessageTimeout = 0;
uint8_t esp32RebootTimeout = 0;
uint8_t workerTimeout1000ms = 0;

uint32_t currentIOState = 0;

//static uint8_t DataOutTimeOut = 0;
//static uint16_t TimeOut1s = 0;
//static uint16_t TimeOut5s = 0;
//static uint16_t PingTimeOut = 0;

char BanTinESP[150];
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void ProcessNewESPData(void);
static void ProcessNewFMData(void);

/*****************************************************************************/
/**
 * @brief	:  	Tick every 1s
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	04/03/2016
 * @version	:
 * @reviewer:	
 */
char espMessage[200];
//void ESP32_WokerSendState(void)
//{
//    /** Gui cac thong tin dinh ky len mach master
//	* FM,LOCAL_MODE=<FM/MIC/IDLE>,FREQ=<FREQ>,VOL=<>,SNR=<>,RSSI=<>,GPS=<VD,KD>,REG12=65,VERSION=4,CRC=12345#
//	*/
//    uint8_t index = 0;
//    uint8_t localVolume = xSystem.Parameter.LocalVolume;

//    //Read Vin voltage
//    uint32_t vin = adc_vin_value * ADC_VREF * 48 / ADC_12BIT_FACTOR;
////    DEBUG_PRINTF("ADC vin: %d\r\n", adc_vin_value);

//    memset(espMessage, 0, sizeof(espMessage));

//    /* Gui cac trang thai cua stm32 */
//    index = sprintf(espMessage, "WORKER|GPIO=%d|VIN=%u|ISO1=%u|ISO2=%d|",
//                    currentIOState, vin, GetISOInput1State(), GetISOInput2State());

//    /* Firmware Version */
//    index += sprintf(&espMessage[index], "VERSION=%u|", FIRMWARE_VERSION_CODE);

//    /* Add checksum */
//    uint16_t crc16 = CalculateCRC16((uint8_t *)espMessage, index);
//    index += sprintf(&espMessage[index], "CRC=%05u#", crc16);

//    /* Send to ESP32 */
//    UART_Puts(ESP32_UART, espMessage);

//    /** Giam sat trang thai cua ESP32, truong hop bi treo khi bat nguon -> reset */
//    if (xSystem.Status.isESP32Enable)
//    {
//        if (esp32RxMessageTimeout++ >= 20)
//        {
//            esp32RxMessageTimeout = 0;
//            DEBUG_PRINTF("\t!!! WARNING: ESP32 has died, reboot him....!!!\r\n");

//            ESP_RESET(0);
//            xSystem.Status.isESP32Enable = 0;
//            esp32RebootTimeout = 3; //cho 3s sau thi Enable lai esp32
//        }
//    }
//    else
//    {
//        if (esp32RebootTimeout)
//        {
//            esp32RebootTimeout--;
//            if (esp32RebootTimeout == 0)
//            {
//                DEBUG_PRINTF("\t\t ------ START ESP32 again -------\r\n");
//                ESP_RESET(1);
//                xSystem.Status.isESP32Enable = 1;
//                esp32RxMessageTimeout = 0;
//            }
//        }
//    }

//#if 0
//	DEBUG_PRINTF("Worker--> %s", espMessage);
//#endif
//}

/*****************************************************************************/
/**
 * @brief	:  	Tick every 10ms
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	04/03/2016
 * @version	:
 * @reviewer:	
 */
void ESP32_UART_Tick(void)
{
    if (ESPBuffer.State)
    {
        ESPBuffer.State--;
        if (ESPBuffer.State == 0)
        {
            ProcessNewESPData();
            ESPBuffer.BufferIndex = 0;
        }
    }

    //FM UART
    if (FMBuffer.State)
    {
        FMBuffer.State--;
        if (FMBuffer.State == 0)
        {
            ProcessNewFMData();
            FMBuffer.BufferIndex = 0;

            //Gui ban tin xen ke voi ban tin FM
            workerTimeout1000ms = 50;
        }
    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/03/2016
 * @version	:
 * @reviewer:	
 */
//uint8_t isValidFreq(uint16_t Freq)
//{
//    for (uint8_t i = 0; i < sizeof(xSystem.Parameter.Frequency); i++)
//    {
//        if (Freq == xSystem.Parameter.Frequency[i])
//            return 1;
//    }
//    return 0xFF;
//}

/*****************************************************************************/
/**
 * @brief	:  Dieu khien tat ca cac GPIO theo lenh tu ESP32
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	03/03/2016
 * @version	:
 * @reviewer:	
 */
//static void workerControlGpio(GPIO_Control_t *gpio)
//{
//    DEBUG_PRINTF("GPIO: %04X, AudioIn: %s, AudioOut: %s, PA: %s\r\n", gpio->Value,
//           gpio->Name.AudioInSW ? "MIC" : "AUX",
//           gpio->Name.AudioOutSW ? "FM" : "CODEC",
//           gpio->Name.AudioPAEn ? "ON" : "OFF");

//    AUDIO_IN(gpio->Name.AudioInSW);
//    AUDIO_OUT(gpio->Name.AudioOutSW);
//    AUDIO_PA(gpio->Name.AudioPAEn);
//    GSM_PWR_EN(gpio->Name.GSMPwrEn);
//    GSM_PWR_KEY(gpio->Name.GSMPwrKey);
//    GSM_RESET(gpio->Name.GSMReset);
//    ISO_OUT1(gpio->Name.ISOOut1);
//    ISO_OUT2(gpio->Name.ISOOut2);
//    LED1_BLUE(gpio->Name.LED1Blue);
//    LED1_RED(gpio->Name.LED1Red);
//    LED2_BLUE(gpio->Name.LED2Blue);
//    LED2_RED(gpio->Name.LED2Red);
//}

//void XuLyBanTin(char *msg, uint8_t length)
//{
//    //	DEBUG_PRINTF("ESP32: %d - %s", length, msg);
//    if (length <= 10)
//        return;

//    /* Check CRC: ,CRC=12345# */
//    char *crc = strstr(msg, "CRC=");
//    if (crc)
//    {
//        uint16_t crc16 = GetNumberFromString(4, crc);

//        /* Tinh CRC thuc cua chuoi: Tru 10 ki tu cuoi CRC=12345# */
//        uint16_t crcCal = CalculateCRC16((uint8_t *)msg, length - 10);

//        if (crc16 != crcCal)
//        {
//            DEBUG_PRINTF("CRC failed: %u - %u\r\n", crc16, crcCal);
//            return;
//        }
//    }
//    else
//    {
//        DEBUG_PRINTF("ERR: Sai dinh dang!\r\n");
//        return;
//    }

//    if (strstr(msg, "4G,"))
//    {
//        /* Forward ban tin xuong cho mach FM */
//        UART_Putb(FM_UART, (uint8_t *)msg, length);
//        return;
//    }

//    /** Xu ly cac lenh dieu khien worker */
//    if (strstr((char *)ESPBuffer.Buffer, "RESETWORKER"))
//    {
//        NVIC_SystemReset();
//    }

//    /** Lenh dieu khien GPIO */
//    char *gpioControl = strstr((char *)ESPBuffer.Buffer, "GPIO=");
//    if (gpioControl != NULL)
//    {
//        GPIO_Control_t gpio;
//        gpio.Value = GetNumberFromString(5, gpioControl);
//        currentIOState = gpio.Value;
//        workerControlGpio(&gpio);
//    }
//}

/*****************************************************************************/
/**
 * @brief	:  Xu ly ban tin nhan duoc tu ESP32
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	03/03/2016
 * @version	:
 * @reviewer:	
 */
static void ProcessNewESPData(void)
{
    esp32RxMessageTimeout = 0;
    //	DEBUG_PRINTF("ESP32--> %s", ESPBuffer.Buffer);

    /** Xu ly ban tin tu ESP32 */
    /** 4G,MODE=<FM/4G/MIC/NONE/IDLE>,FREQ_RUN=<10270>,FREQ1=<>,FREQ2=<>,FREQ3=<>,VOL=<70>,THRESHOLD=<90>,CRC=12345#
		* Dinh dang gia tri tan so: = Freq (Hz) /10000, vi du: 102.7MHz = 102700000 /10000 = 10270 
	* Lệnh ghi/đọc trực tiếp thanh ghi của KT0935R: KT_WRITE<xxx>=<yy> trong đó: xxx là địa chỉ thanh ghi, 3 digit, yy: giá trị cần ghi
	* KT_READ<xxx>
	*/
    //	if(strstr((char*)ESPBuffer.Buffer, "4G,") && ESPBuffer.BufferIndex > 10)
    if (ESPBuffer.BufferIndex > 10)
    {
        uint8_t IndexBuffer[5] = {0};
        uint8_t index = 1;
        //Tim vi tri cac ki tu "#", co the > 1 ban tin dinh lien nhau
        for (uint8_t i = 0; i < ESPBuffer.BufferIndex; i++)
        {
            if (ESPBuffer.Buffer[i] == '#')
            {
                IndexBuffer[index++] = i;
            }
        }
        if (index == 1)
            return;

        //Xu ly ban tin ESP32 (tach ban tin trong truong hop bi dinh lien)
        for (uint8_t j = 0; j < index; j++)
        {
            if (IndexBuffer[j + 1] > IndexBuffer[j])
            {
                memset(BanTinESP, 0, sizeof(BanTinESP));
                uint8_t MessageLength = 0;
                if (j == 0)
                {
                    MessageLength = IndexBuffer[j + 1] - IndexBuffer[j] + 1;
                    memcpy(BanTinESP, &ESPBuffer.Buffer[IndexBuffer[j]], MessageLength);
                }
                else
                {
                    MessageLength = IndexBuffer[j + 1] - IndexBuffer[j];
                    memcpy(BanTinESP, &ESPBuffer.Buffer[IndexBuffer[j] + 1], MessageLength);
                }

                //Xu ly tung ban tin
//                XuLyBanTin(BanTinESP, MessageLength);
            }
        }
    }
}

static void ProcessNewFMData(void)
{
    //	DEBUG_PRINTF("FM--> %s", FMBuffer.Buffer);

    //Forward len mach ESP32
    //UART_Putb(ESP32_UART, FMBuffer.Buffer, FMBuffer.BufferIndex);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	03/03/2016
 * @version	:
 * @reviewer:	
 */
static void ESPUARTAddData(uint8_t Data)
{
    ESPBuffer.Buffer[ESPBuffer.BufferIndex++] = Data;
    ESPBuffer.Buffer[ESPBuffer.BufferIndex] = 0;
    ESPBuffer.State = 7;
}

static void FMUARTAddData(uint8_t Data)
{
    FMBuffer.Buffer[FMBuffer.BufferIndex++] = Data;
    FMBuffer.Buffer[FMBuffer.BufferIndex] = 0;
    FMBuffer.State = 7;
}

/*******************************************************************************
 * Function Name  	: USART3_4_IRQHandler 
 * Return         	: None
 * Parameters 		: None
 * Created by		: Phinht
 * Date created	: 02/03/2016
 * Description		: This function handles USART1 global interrupt request. 
 * Notes			: 
 *******************************************************************************/

extern size_t driver_uart0_get_new_data_to_send(uint8_t *c);
extern size_t driver_uart1_get_new_data_to_send(uint8_t *c);

void USART0_IRQHandler(void)
{
    if (USART_GetITStatus(USART0, USART_IT_RXNE) != RESET)
    {
        uint8_t rx = USART_ReceiveData(USART0);
        ESPUARTAddData(rx);
    }
    
    uint32_t tmp = USART_CTL0(USART0);
        
    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_TBE)
        && (tmp & USART_CTL0_TBEIE))
    {
        /* transmit data */
        uint8_t c;
        if (driver_uart0_get_new_data_to_send(&c))
        {
            usart_data_transmit(USART0, c);
        }
        else
        {
            usart_interrupt_disable(USART0, USART_INT_TBE);
        }
    } 
    
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t rx = USART_ReceiveData(USART1);
        FMUARTAddData(rx);
    }
    
    uint32_t tmp = USART_CTL0(USART1);
        
    if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_TBE)
        && (tmp & USART_CTL0_TBEIE))
    {
        /* transmit data */
        uint8_t c;
        if (driver_uart1_get_new_data_to_send(&c))
        {
            usart_data_transmit(USART1, c);
        }
        else
        {
            usart_interrupt_disable(USART1, USART_INT_TBE);
        }
    }  
}

/********************************* END OF FILE *******************************/
