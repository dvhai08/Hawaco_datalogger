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
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "iwdg.h"
#include "lptim.h"
#include "usart.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_eeprom.h"
#include "measure_input.h"
#include "lpf.h"
#include "stdbool.h"
#include "app_cli.h"
#include "app_wdt.h"
#include "app_sync.h"
#include "hardware_manager.h"
#include "app_bkup.h"
#include "control_output.h"
#include "gsm.h"
#include "hardware.h"
#include "modbus_master.h"
#include "string.h" 
#include "sys_ctx.h"
#include "app_rtc.h"
#include "app_debug.h"
#include "app_flash.h"
#include "ota_update.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
enum 
{
	FLASHIF_OK = 0,
	FLASHIF_ERASEKO,
	FLASHIF_WRITINGCTRL_ERROR,
	FLASHIF_WRITING_ERROR,
	FLASHIF_PROTECTION_ERRROR
};

#define ABS_RETURN(x,y)               (((x) < (y)) ? (y) : (x))

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UMM_HEAP_SIZE				2048
#define GSM_ENABLE					1
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
/* memory for ummalloc */
uint8_t g_umm_heap[UMM_HEAP_SIZE];
static volatile uint32_t m_delay_afer_wakeup_from_deep_sleep_to_measure_data;
static void task_feed_wdt(void *arg);
static void gsm_mnr_task(void *arg);
static void info_task(void *arg);
volatile uint32_t led_blink_delay = 0;
void FLASH_If_Init(void);
uint32_t FLASH_If_Erase(uint32_t start);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
//	__enable_irq();
	hardware_manager_get_reset_reason();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
#if 1
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_IWDG_Init();
  MX_RTC_Init();
  MX_LPTIM1_Init();
  MX_ADC_Init();
  MX_USART1_UART_Init();
  MX_LPUART1_UART_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
#endif
//	HAL_ADC
	DEBUG_RAW(RTT_CTRL_CLEAR);
	app_cli_start();
	app_bkup_init();
    app_eeprom_init();
	measure_input_initialize();
	control_ouput_init();
	adc_start();
	gsm_init_hw();
	
	app_sync_config_t config;
	config.get_ms = sys_get_ms;
	config.polling_interval_ms = 1;
	app_sync_sytem_init(&config);
	
	app_sync_register_callback(task_feed_wdt, 15000, SYNC_DRV_REPEATED, SYNC_DRV_SCOPE_IN_LOOP);
	app_sync_register_callback(gsm_mnr_task, 1000, SYNC_DRV_REPEATED, SYNC_DRV_SCOPE_IN_LOOP);
	app_sync_register_callback(info_task, 1000, SYNC_DRV_REPEATED, SYNC_DRV_SCOPE_IN_LOOP);
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
    sys_ctx_t *system = sys_ctx();
    app_flash_initialize();
//    FLASH_If_Init();
//    FLASH_If_Erase(DONWLOAD_START_ADDR);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#if GSM_ENABLE
	gsm_hw_layer_run();
#endif
	control_ouput_task();
	measure_input_task();
	app_cli_poll();
	app_sync_polling_task();
	if (led_blink_delay)
	{
		LED1(1);
		LED2(1);
	}
	else
	{
		LED1(0);
		LED2(0);
	}	
	if (system->status.is_enter_test_mode)
	{
		cfg->io_enable.name.output_4_20ma_enable = 1;
		if (cfg->io_enable.name.output_4_20ma_value == 0)
			cfg->io_enable.name.output_4_20ma_value = 10;
		cfg->io_enable.name.output_4_20ma_timeout_100ms = 100;
		control_output_dac_enable(1000000);
		cfg->io_enable.name.rs485_en = 1;
		ENABLE_INOUT_4_20MA_POWER(1);
	}
	
//	if (cfg->io_enable.name.rs485_en)
//	{
//		RS485_EN(cfg->io_enable.name.rs485_en);
//	}
	
//	__WFI();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_3;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_LPUART1
                              |RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_LPTIM1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  PeriphClkInit.LptimClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void sys_set_delay_time_before_deep_sleep(uint32_t ms)
{
	m_delay_afer_wakeup_from_deep_sleep_to_measure_data = 0;
	#warning "Please implement delay timeout afer wakeup from deep sleep"
}


uint32_t sys_get_ms()
{
	return HAL_GetTick();
}

void sys_delay_ms(uint32_t ms)
{
	uint32_t current_tick = HAL_GetTick();
	
	while (1)
	{
#ifdef WDT_ENABLE
        LL_IWDG_ReloadCounter(IWDG);
#endif
//		__WFI();
		if (HAL_GetTick() - current_tick >= (uint32_t)ms)
		{
			break;
		}
	}
}

static void task_feed_wdt(void *arg)
{
#ifdef WDT_ENABLE
	LL_IWDG_ReloadCounter(IWDG);
#endif
}

static void gsm_mnr_task(void *arg)
{
	gsm_manager_tick();
}

static void info_task(void *arg)
{	
	static uint32_t i = 0;
    sys_ctx_t *system = sys_ctx();
    
	if (system->status.is_enter_test_mode)
	{
		char buf[48];
		sprintf(buf, "%u\r\n", sys_get_ms());
		RS485_EN(1);
        RS485_DIR_RX();
        i = 5;
//		Modbus_Master_Write((uint8_t*)buf, strlen(buf));
	}
    if (i++ >= 5)
	{
		i = 0;
		adc_input_value_t *adc = adc_get_input_result();
        rtc_date_time_t time;
        app_rtc_get_time(&time);
		DEBUG_RAW("20%02u/%02u/%02u %02u:%02u:%02u : bat_mv %u-%u%, vin-24 %umV, 4-20mA in %u.%u, temp %u\r\n",
                    time.year,
                    time.month,
                    time.day,
                    time.hour,
                    time.minute,
                    time.second,
					adc->bat_mv, adc->bat_percent, 
					adc->vin_24,
					adc->i_4_20ma_in[0]/10, adc->i_4_20ma_in[0]%10,
					adc->temp);
	}
}

/**
  * @brief  Unlocks Flash for write access
  * @param  None
  * @retval None
  */
void FLASH_If_Init(void)
{
  /* Unlock the Program memory */
  HAL_FLASH_Unlock();

  /* Clear all FLASH flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR |
                         FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | FLASH_FLAG_FWWERR |
                         FLASH_FLAG_NOTZEROERR);
  /* Unlock the Program memory */
  HAL_FLASH_Lock();
}

/**
  * @brief  				This function does an erase of all user flash area
  * @param  				start: start of user flash area
  * @retval 				FLASHIF_OK : user flash area successfully erased
  *         				FLASHIF_ERASEKO : error occurred
  */
uint32_t FLASH_If_Erase(uint32_t start)
{
    DEBUG_PRINTF("Flash erase at addr 0x%08X\r\n", start);
    
    FLASH_If_Init();
    
	FLASH_EraseInitTypeDef desc;
	uint32_t result = FLASHIF_OK;
	uint32_t pageerror;


	HAL_FLASH_Unlock();

	desc.PageAddress = start;
	desc.TypeErase = FLASH_TYPEERASE_PAGES;

	/* NOTE: Following implementation expects the IAP code address to be < Application address */  
	if (start < FLASH_START_BANK2 )
	{
		desc.NbPages = (FLASH_START_BANK2 - start) / FLASH_PAGE_SIZE;
		if (HAL_FLASHEx_Erase(&desc, &pageerror) != HAL_OK)
		{
			result = FLASHIF_ERASEKO;
		}
	}

	if (result == FLASHIF_OK )
	{
		desc.PageAddress = ABS_RETURN(start, FLASH_START_BANK2);
		desc.NbPages = (DONWLOAD_END_ADDR - desc.PageAddress) / FLASH_PAGE_SIZE;
		if (HAL_FLASHEx_Erase(&desc, &pageerror) != HAL_OK)
		{
			result = FLASHIF_ERASEKO;
		}
	}

	HAL_FLASH_Lock();
    DEBUG_PRINTF("Erase flash error code %u\r\n", result);
	return result;
}

uint32_t FLASH_If_EraseOtaInfo()
{
	FLASH_EraseInitTypeDef desc;
	uint32_t result = FLASHIF_OK;
	uint32_t pageerror;


	HAL_FLASH_Unlock();

	desc.PageAddress = OTA_INFO_START_ADDR;
	desc.TypeErase = FLASH_TYPEERASE_PAGES;

	desc.NbPages = 1;
	if (HAL_FLASHEx_Erase(&desc, &pageerror) != HAL_OK)
	{
		result = FLASHIF_ERASEKO;
	}
	
	HAL_FLASH_Lock();

	return result;
}

/* Public functions ---------------------------------------------------------*/
/**
  * @brief  This function writes a data buffer in flash (data are 32-bit aligned).
  * @note   After writing data buffer, the flash content is checked.
  * @param  destination: start address for target location
  * @param  p_source: pointer on buffer with data to write
  * @param  length: length of data buffer (unit is 32-bit word)
  * @retval uint32_t 0: Data successfully written to Flash memory
  *         1: Error occurred while writing data in Flash memory
  *         2: Written Data in flash memory is different from expected one
  */
uint32_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t length)
{
	uint32_t status = FLASHIF_OK;
	uint32_t *p_actual = p_source; /* Temporary pointer to data that will be written in a half-page space */
	uint32_t i = 0;
    
    FLASH_If_Init();
    
	HAL_FLASH_Unlock();

	while (p_actual < (uint32_t*)(p_source + length))
	{    
		LL_IWDG_ReloadCounter(IWDG);
		/* Write the buffer to the memory */
		if (HAL_FLASHEx_HalfPageProgram(destination, p_actual ) == HAL_OK) /* No error occurred while writing data in Flash memory */
		{
			/* Check if flash content matches memBuffer */
			for (i = 0; i < WORDS_IN_HALF_PAGE; i++)
			{
				if ((*(uint32_t*)(destination + 4 * i)) != p_actual[i])
				{
					/* flash content doesn't match memBuffer */
					status = FLASHIF_WRITINGCTRL_ERROR;
					break;
				}
			}

			/* Increment the memory pointers */
			destination += FLASH_HALF_PAGE_SIZE;
			p_actual += WORDS_IN_HALF_PAGE;
		}
		else
		{
			status = FLASHIF_WRITING_ERROR;
		}

		if (status != FLASHIF_OK)
		{
			break;
		}
	}

	HAL_FLASH_Lock();
    
    if (status != FLASHIF_OK)
    {
        DEBUG_PRINTF("Flash write error %d\r\n", status);
    }
    
	return status;
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  DEBUG_ERROR("Error handle\r\n");
  __disable_irq();
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	DEBUG_ERROR("Assert failed %s, line %u\r\n", file, line);
	while (1);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
