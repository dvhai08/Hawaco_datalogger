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
#include "app_spi_flash.h"
#include "ota_update.h"
#include "flash_if.h"
#include "version_control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define WAKEUP_RESET_WDT_IN_LOW_POWER_MODE            23000     // ( ~18s)
#define DEBUG_LOW_POWER                                 0
#define DISABLE_GPIO_ENTER_LOW_POWER_MODE               0
#define TEST_POWER_ALWAYS_TURN_OFF_GSM                  0
#define TEST_OUTPUT_4_20MA                              0
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
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
uint8_t g_umm_heap[UMM_MALLOC_CFG_HEAP_SIZE];
static volatile uint32_t m_delay_afer_wakeup_from_deep_sleep_to_measure_data;
static void task_feed_wdt(void *arg);
static void gsm_mnr_task(void *arg);
static void info_task(void *arg);
volatile uint32_t led_blink_delay = 0;
void sys_config_low_power_mode(void);
#warning "Please handle sensor msg full"
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

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
    __enable_irq();
#if 1
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_IWDG_Init();
  MX_RTC_Init();
  MX_ADC_Init();
  MX_USART1_UART_Init();
  MX_LPUART1_UART_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
#endif
//	HAL_ADC
    __HAL_DBGMCU_FREEZE_IWDG();     // stop watchdog in debug mode
    
	DEBUG_RAW(RTT_CTRL_CLEAR);
    sys_ctx_t *system = sys_ctx();
    system->peripheral_running.name.flash_running = 1;
    system->peripheral_running.name.rs485_running = 1;
	app_cli_start();
	app_bkup_init();
    app_eeprom_init();
    app_spi_flash_initialize();
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

    ota_flash_cfg_t *ota_cfg = ota_update_get_config();
#if TEST_OUTPUT_4_20MA
	cfg->io_enable.name.output_4_20ma_enable = 1;
	cfg->io_enable.name.output_4_20ma_value = 10;
	cfg->io_enable.name.output_4_20ma_timeout_100ms = 100;
	control_output_dac_enable(1000000);
    system->status.is_enter_test_mode = 1;
    cfg->io_enable.name.input_4_20ma_enable = 1;
#endif    
    DEBUG_PRINTF("Build %s %s, version %s\r\nOTA flag 0x%08X, info %s\r\n", __DATE__, __TIME__, 
                                                                            VERSION_CONTROL_FW,
                                                                            ota_cfg->flag, (uint8_t*)ota_cfg->reserve);
  LED1(1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#if GSM_ENABLE
	gsm_hw_layer_run();
#endif
      
#if TEST_POWER_ALWAYS_TURN_OFF_GSM
    if (!gsm_data_layer_is_module_sleeping())
    {
        gsm_change_state(GSM_STATE_SLEEP);
    }
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
    
    if (!cfg->io_enable.name.rs485_en)
    {
        system->peripheral_running.name.rs485_running = 0;
        usart_lpusart_485_control(0);
    }
    
    if (!cfg->io_enable.name.input_4_20ma_enable)
    {
        ENABLE_INOUT_4_20MA_POWER(0);
    }
    
    if (gsm_data_layer_is_module_sleeping())
    {
        system->peripheral_running.name.gsm_running = 0;
        if (system->peripheral_running.name.flash_running)
        {
            app_spi_flash_shutdown();
            spi_deinit();
            system->peripheral_running.name.flash_running = 0;
        }
        
        usart1_control(false);
        GSM_PWR_EN(0);
        GSM_PWR_RESET(0);
        GSM_PWR_KEY(0);
    }
    else
    {
        system->peripheral_running.name.gsm_running = 1;
    }
        
    if (ota_update_is_running())
    {
        system->status.sleep_time_s = 0;
        system->status.disconnect_timeout_s = 0;
    }
    
	
	if (cfg->io_enable.name.rs485_en)
	{
		RS485_EN(cfg->io_enable.name.rs485_en);
	}
    
    if (system->peripheral_running.value == 0)
    {
        adc_stop();
        if (system->status.is_enter_test_mode == 0)
        {
            sys_config_low_power_mode();
        }
    }
	
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
  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
                              |RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
    sys_ctx_t *ctx = sys_ctx();
    if (gsm_data_layer_is_module_sleeping())
    {
        ctx->status.sleep_time_s++;
        ctx->status.disconnect_timeout_s = 0;
    }
    else
    {
        ctx->status.disconnect_timeout_s++;
    }
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
        uint32_t clk = HAL_RCC_GetSysClockFreq() / 1000000;
		DEBUG_PRINTF("CLK %uMhz, bat_mv %u-%u%, vin-24 %umV, 4-20mA in %u.%u, temp %u\r\n",
                    clk,
					adc->bat_mv, adc->bat_percent, 
					adc->vin_24,
					adc->in_4_20ma_in[0]/10, adc->in_4_20ma_in[0]%10,
					adc->temp);
	}
}

void sys_turn_on_led(uint32_t delay_ms)
{
    led_blink_delay = delay_ms;
}

static bool m_wakeup_timer_run = false;
void sys_config_low_power_mode(void)
{
    // estimate RTC wakeup time
    if (m_wakeup_timer_run == false)
    {
        sys_ctx_t *ctx = sys_ctx();
        
#if DEBUG_LOW_POWER
        __DBGMCU_CLK_ENABLE() ; // (RCC->APB2ENR |= (RCC_APB2ENR_DBGMCUEN))
        HAL_EnableDBGStopMode();  //  SET_BIT(DBGMCU->CR, DBGMCU_CR_DBG_STOP);
#endif
        
#ifdef WDT_ENABLE
	LL_IWDG_ReloadCounter(IWDG);
#endif
        
        uint32_t counter_before_sleep = app_rtc_get_counter();
        DEBUG_VERBOSE("Before sleep - counter %u\r\n", counter_before_sleep);
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WAKEUP_RESET_WDT_IN_LOW_POWER_MODE, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
        {
            Error_Handler();
        }
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        
        /* Disable GPIOs clock */
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOA);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOB);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOC);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOH);

        HAL_SuspendTick();
        
//          GPIO_InitTypeDef GPIO_InitStructure;
        __HAL_RCC_PWR_CLK_ENABLE();
        
        /* Enable Ultra low power mode */
        HAL_PWREx_EnableUltraLowPower();

        /* Enable the fast wake up from Ultra low power mode */
        HAL_PWREx_EnableFastWakeUp();
        

        
        /* Select MSI as system clock source after Wake Up from Stop mode */
        __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);

          /* Enter Stop Mode */
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        
#if 1   
        /* Enable GPIOs clock */
        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOH);
#endif        
        uint32_t counter_after_sleep = app_rtc_get_counter();
        uint32_t diff = counter_after_sleep-counter_before_sleep;
        uwTick += diff*1000;
        DEBUG_VERBOSE("Afer sleep - counter %u, diff %u\r\n", counter_after_sleep, diff);
                
        ctx->status.sleep_time_s += diff;
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        m_wakeup_timer_run = false;
        DEBUG_PRINTF("Wake, sleep time %us\r\n", ctx->status.sleep_time_s);
        HAL_ResumeTick();
        SystemClock_Config();
#ifdef WDT_ENABLE
        LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    else
    {
        DEBUG_PRINTF("RTC timer still running\r\n");
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    m_wakeup_timer_run = false;
    DEBUG_INFO("Wakeup timer event callback\r\n");
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
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
    NVIC_SystemReset();
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
    NVIC_SystemReset();
	while (1);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
