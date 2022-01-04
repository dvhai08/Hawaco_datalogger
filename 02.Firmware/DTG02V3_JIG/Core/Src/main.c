/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_debug.h"
#include "usb_device.h"
#include "usbd_customhid.h"
#include "usbd_custom_hid_if.h"
#include "stdbool.h"
#include "lwrb/lwrb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_RS485_BUFFER_SIZE               512
#define MAX_REPORT_BUFFER_SIZE              256
#define USB_RX_BUFFER_SIZE                  256
#define ADC_MEASUREMENT_INVERVAL_MS         500
#define RS485_IDLE_TIMEOUT_MS               10
#define USB_WAIT_TX_DONE_TIMEOUT_MS         (150)
#define APP_USB_WAIT_TX_CPLT(x)             xSemaphoreTake(m_sem_usb_tx_cplt, x)
#define APP_USB_TX_CPLT_DONE()              {   BaseType_t ctx_sw; xSemaphoreGiveFromISR(m_sem_usb_tx_cplt, &ctx_sw);  \
                                                portYIELD_FROM_ISR(ctx_sw);   \
                                            }
#define JIG_TOGGLE_IO_MS                    50
#define JIG_REQUEST_CMD                   "{\"test\":1}"
                                            
                                            
typedef struct
{
    uint8_t data[MAX_RS485_BUFFER_SIZE];
    uint32_t index;
    uint32_t rx_timestamp;
} rs485_rx_t;


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void hid_tx_cplt_callback(uint8_t epnum);
static uint32_t m_adc_voltage[ADC_MAX_CHANNEL];
static uint32_t m_adc_resister_div[ADC_MAX_CHANNEL] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1};        // Hardware resister divider on ADC port
static char m_report_data[MAX_REPORT_BUFFER_SIZE];
static rs485_rx_t m_rs485_rx = 
{
    .index = 0
};

static uint8_t m_hid_rx_raw[USB_RX_BUFFER_SIZE];
static lwrb_t m_ringbuffer_usb_rx;

void hid_rx_data_cb(uint8_t *buffer);
static void send_data_to_host(uint8_t *data, uint32_t length);

static uint32_t m_last_slave_output[2];
static uint32_t m_slave_output_toggle_count[2];
static uint32_t m_rs485_pass = 10;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	DEBUG_INFO("DTG02V3 JIG\r\nBuild %s %s\r\n", __DATE__, __TIME__);
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
    
    // SET USB D+ to low =>> host will detect usb deconnect after software reset
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_12);
    HAL_Delay(50);
    
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_USART3_UART_Init();
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  MX_IWDG_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM15_Init();
  /* USER CODE BEGIN 2 */
    lwrb_init(&m_ringbuffer_usb_rx, m_hid_rx_raw, sizeof(m_hid_rx_raw));
    usbd_custom_hid_if_register_rx_callback(hid_rx_data_cb);
    
    // Start ADC conversion
    adc_start();
    uint32_t last_adc_tick;
    
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      
        uint32_t now = uwTick;
        
        // Read MCU output1 - 2 from slave
        uint32_t level = LL_GPIO_IsInputPinSet(MCU_OUT1_GPIO_Port, MCU_OUT1_Pin);
        if (level != m_last_slave_output[0])
        {
            m_last_slave_output[0] = level;
            m_slave_output_toggle_count[0]++;
        }
        
        level = LL_GPIO_IsInputPinSet(MCU_OUT2_GPIO_Port, MCU_OUT2_Pin);
        if (level != m_last_slave_output[1])
        {
            m_last_slave_output[1] = level;
            m_slave_output_toggle_count[1]++;
        }
        
        // Check ADC value and send data to PC
        if (now - last_adc_tick >= ADC_MEASUREMENT_INVERVAL_MS)
        {
            last_adc_tick = now;
            uint16_t *adc_result = adc_get_result();
            static uint32_t prev_vref = 0;
            for (uint32_t i = 0; i < ADC_MAX_CHANNEL; i++)
            {
                m_adc_voltage[i] = adc_result[i];
                if (ADC_CHANNEL_VREF != i)
                {
                    if (prev_vref == 0)
                    {
                        m_adc_voltage[i] = m_adc_voltage[i] *3300*m_adc_resister_div[i]/4096;
                    }
                    else
                    {
                        m_adc_voltage[i] = m_adc_voltage[i] *prev_vref*m_adc_resister_div[i]/4096; 
                    }
                }
                else
                {
                    //m_adc_voltage[i] = 3300UL * (*VREFINT_CAL)/m_adc_voltage[i];
                    m_adc_voltage[i] = __LL_ADC_CALC_VREFANALOG_VOLTAGE(m_adc_voltage[i], LL_ADC_RESOLUTION_12B);;
                    prev_vref = m_adc_voltage[i];
                }
            }        
            
            memset(m_report_data, 0, sizeof(m_report_data));
            uint32_t len = 0;
            len += sprintf((char*)m_report_data+len, "%s", "{");
            /**
             *  Frame send to PC
             *  {
                    "Outl":"PASSED",
                    "Out2":"FAILED",
                    "ADCx":1234,
                    "RS485":"PASSED"
                }
             */ 
            if (m_slave_output_toggle_count[0] > 10)
            {
                len += sprintf((char*)m_report_data+len, "\"Outl\":\"%s\",", "PASSED");

            }
            else
            {
                len += sprintf((char*)m_report_data+len, "\"Outl\":\"%s\",", "FAILED");
            }
            
            if (m_slave_output_toggle_count[1] > 10)
            {
                len += sprintf((char*)m_report_data+len, "\"Out2\":\"%s\",", "PASSED");

            }
            else
            {
                len += sprintf((char*)m_report_data+len, "\"Out2\":\"%s\"", "FAILED");
            }
            
            for (uint32_t i = 0; i < ADC_MAX_CHANNEL; i++)
            {
                len += sprintf((char*)m_report_data+len, "\"ADC%u\":%u,", i, m_adc_voltage[i]);
            }
            
            if (m_rs485_pass)
            {
                m_rs485_pass--;
                len += sprintf((char*)m_report_data+len, "\"RS485\":\"%s\"", "PASSED");
            }
            else
            {
                len += sprintf((char*)m_report_data+len, "\"RS485\":\"%s\"", "FAILED");
            }
            len += sprintf((char*)m_report_data+len, "%s", "}");
            
            send_data_to_host((uint8_t*)m_report_data, len);
            DEBUG_INFO("JIG =>>>>> PC : %s\r\n", (char*)m_report_data);

            
            LL_IWDG_ReloadCounter(IWDG);
        }
        
        // Forward RS485 packet to PC via USB
        if (m_rs485_rx.index && now - m_rs485_rx.rx_timestamp > RS485_IDLE_TIMEOUT_MS)
        {   
            // Send test cmd to slave            
            usart_rs485_send_data((uint8_t*)JIG_REQUEST_CMD, strlen((char*)JIG_REQUEST_CMD));
            
            // Get unique id
            static char m_last_device_id[13];
            char *device_id =  strstr((char*)m_rs485_rx.data, "\"id\":\"");
            if (device_id)
            {
                device_id += strlen("\"id\":\"");
                // If new device attached to JIG =>> reset previous device data and id
                m_last_device_id[12] = 0;
                if (strcmp(device_id, m_last_device_id))        
                {
                    DEBUG_INFO("New device %s attached\r\n", m_last_device_id);
                    memcpy(m_last_device_id, device_id, 12);
                    m_slave_output_toggle_count[0] = 0;
                    m_slave_output_toggle_count[1] = 0;
                }
                m_rs485_pass = 10;
            }
            
            // Forward data to PC
            static uint32_t rs485_count = 0;
            memset(m_report_data, 0, sizeof(m_report_data));
            uint32_t len = 0;
            len += snprintf((char*)m_report_data, sizeof(m_report_data) - 1, "%s", m_rs485_rx.data);
            DEBUG_WARN("RS485 =>>>>> PC : %s\r\n", (char*)m_report_data);
            send_data_to_host((uint8_t*)m_report_data, len);
            
            // Add RS485 counter
            memset(m_report_data, 0, sizeof(m_report_data));
            len = 0;
            len += snprintf((char*)m_report_data, sizeof(m_report_data) - 1, "{\"rs485_count\":%u}", ++rs485_count);
            send_data_to_host((uint8_t*)m_report_data, len);

            memset(&m_rs485_rx, 0, sizeof(m_rs485_rx));
        }
                
        // Read USB RX buffer and forward to slave
        uint8_t tmp;
        while (lwrb_read(&m_ringbuffer_usb_rx, &tmp, 1))
        {
            usart_rs485_send_data(&tmp, 1);
        }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1)
  {
  }
  LL_RCC_HSI14_Enable();

   /* Wait till HSI14 is ready */
  while(LL_RCC_HSI14_IsReady() != 1)
  {

  }
  LL_RCC_HSI14_SetCalibTrimming(16);
  LL_RCC_HSI48_Enable();

   /* Wait till HSI48 is ready */
  while(LL_RCC_HSI48_IsReady() != 1)
  {

  }
  LL_RCC_LSI_Enable();

   /* Wait till LSI is ready */
  while(LL_RCC_LSI_IsReady() != 1)
  {

  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI48);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI48)
  {

  }
  LL_SetSystemCoreClock(48000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
  LL_RCC_HSI14_EnableADCControl();
  LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_HSI48);
}

/* USER CODE BEGIN 4 */
uint32_t sys_get_ms(void)
{
	return uwTick;
}

static volatile bool m_hid_tx_cplt = false;
void hid_tx_cplt_callback(uint8_t epnum)
{
    m_hid_tx_cplt = true;
}

static bool wait_hid_complete(uint32_t timeout_ms)
{
    uint32_t prev = uwTick;
    m_hid_tx_cplt = false;
    while (m_hid_tx_cplt == false)
    {
        if (uwTick - prev >= timeout_ms)
        {
            LL_IWDG_ReloadCounter(IWDG);
            break;
        }
    }
    return m_hid_tx_cplt;
}

void send_data_to_host(uint8_t *data, uint32_t length)
{            
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
    {
        return;
    }
    
    uint8_t *p = data;
    uint32_t now = uwTick;
    uint32_t remain = length % CUSTOM_HID_EPIN_SIZE;
    length -= remain;
    for (uint32_t i = 0; i < (length)/CUSTOM_HID_EPIN_SIZE; i++)     // 64 = sizeof usb hid max transfer
    {
        if (USBD_OK == USBD_CUSTOM_HID_SendReport_FS((uint8_t*)p, CUSTOM_HID_EPIN_SIZE))
        {
            if (wait_hid_complete(USB_WAIT_TX_DONE_TIMEOUT_MS))
            {
                DEBUG_VERBOSE("HID TX complete in %ums\r\n", uwTick - now);
            }
            now = uwTick;
        }
        else
        {
            DEBUG_ERROR("Send report error\r\n");
        }
        p += CUSTOM_HID_EPIN_SIZE;
    }
    
    if (remain)
    {
        uint8_t tmp[CUSTOM_HID_EPIN_SIZE];
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, p, remain);
        if (USBD_OK == USBD_CUSTOM_HID_SendReport_FS(tmp, CUSTOM_HID_EPIN_SIZE))
        {
            if (wait_hid_complete(USB_WAIT_TX_DONE_TIMEOUT_MS))
            {
                DEBUG_VERBOSE("HID TX complete in %ums\r\n", uwTick - now);
            }
            now = uwTick;
        }
        else
        {
            DEBUG_ERROR("Send report error\r\n");
        }
    }
}

void hid_rx_data_cb(uint8_t *buffer)
{
    DEBUG_INFO("HID RX %s\r\n", buffer);
    for (uint32_t i = 0; i < CUSTOM_HID_EPIN_SIZE; i++)       // 64 = USB HID max rx buffer size
    {
        if (buffer[i])
        {
            lwrb_write(&m_ringbuffer_usb_rx, buffer+i, 1);
        }
        else        // only accept string from PC
        {
            break;
        }
    }
}

void on_rs485_uart_cb(uint8_t data)
{
    if (data == 0)      // Only accept ascii character
    {
        return;
    }
//    DEBUG_RAW("%c", data);
    if (m_rs485_rx.index < MAX_RS485_BUFFER_SIZE)
    {
       m_rs485_rx.data[m_rs485_rx.index++] = data;
       m_rs485_rx.rx_timestamp = uwTick;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
    DEBUG_ERROR("Error_Handler\r\n");
    __disable_irq();
    NVIC_SystemReset();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    DEBUG_ERROR("Assert failed on file %s, line %u\r\n", __FILE__, __LINE__);
	NVIC_SystemReset();
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

