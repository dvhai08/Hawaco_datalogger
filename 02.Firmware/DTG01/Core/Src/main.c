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
#include "crc.h"
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
#include "jig.h"
#include "umm_malloc.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define WAKEUP_RESET_WDT_IN_LOW_POWER_MODE            37000     // ( ~18s)
#define DEBUG_LOW_POWER                                 0
#define DISABLE_GPIO_ENTER_LOW_POWER_MODE               0
#define TEST_POWER_ALWAYS_TURN_OFF_GSM                  0
#define TEST_OUTPUT_4_20MA                              0
#define TEST_INPUT_4_20_MA                              0
#define TEST_RS485                                      0
#define TEST_BACKUP_REGISTER                            0
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
static void info_task(void *arg);
volatile uint32_t led_blink_delay = 0;
void sys_config_low_power_mode(void);
volatile uint32_t m_delay_consider_wakeup = 0;
extern volatile uint32_t measure_input_turn_on_in_4_20ma_power;
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
    app_eeprom_init();
	MX_CRC_Init();
	app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    sys_ctx_t *system = sys_ctx();
#if TEST_OUTPUT_4_20MA
	eeprom_cfg->io_enable.name.output_4_20ma_enable = 1;
    system->status.is_enter_test_mode = 1;
#endif
//#if TEST_INPUT_4_20_MA
//    eeprom_cfg->io_enable.name.input_4_20ma_enable = 1;
//    system->status.is_enter_test_mode = 1;
//#endif

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
//    __HAL_DBGMCU_FREEZE_IWDG();     // stop watchdog in debug mode
    ENABLE_INPUT_4_20MA_POWER(0);
    RS485_POWER_EN(0);
    for (uint32_t i = 0; i < 4; i++)
    {
        LED1(1);
        sys_delay_ms(20);
        LED1(0);
        sys_delay_ms(20);
    }
    LED1(0);
	
    system->peripheral_running.name.flash_running = 1;
    system->peripheral_running.name.rs485_running = 1;
	app_cli_start();
	app_bkup_init();
    app_eeprom_init();
    app_spi_flash_initialize();
	measure_input_initialize();
	control_ouput_init();
//	adc_start();
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
#endif    

#if TEST_INPUT_4_20_MA
    cfg->io_enable.name.input_4_20ma_enable = 1;
    system->status.is_enter_test_mode = 1;
#endif
	char *info = umm_malloc(128);
	char *p = info;
	rtc_date_time_t time;
	app_rtc_get_time(&time);
	
	p += sprintf(p, "[%04u/%02u/%02u %02u:%02u] ",
					time.year + 2000,
					time.month,
					time.day,
					time.hour,
					time.minute);
								
    DEBUG_INFO("Build %s %s, version %s\r\n", __DATE__, __TIME__, VERSION_CONTROL_FW);
	DEBUG_INFO("Now is %s\r\n", info);
	umm_free(info);
	
	jig_start();
//uint32_t spi_flash_packet_size = sizeof(app_spi_flash_data_t);
//DEBUG_INFO("Size of spi flash %u\r\n", spi_flash_packet_size);
	
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
		eeprom_cfg->io_enable.name.output_4_20ma_enable = 1;
		if (eeprom_cfg->output_4_20ma < 1.0f)
			eeprom_cfg->output_4_20ma = 10;
		cfg->io_enable.name.output_4_20ma_timeout_100ms = 100;
		control_output_dac_enable(1000000);
		cfg->io_enable.name.rs485_en = 1;
		ENABLE_INPUT_4_20MA_POWER(1);
        RS485_POWER_EN(1);
        usart_lpusart_485_control(1);
	}
    
    if (!cfg->io_enable.name.rs485_en
		&& system->status.is_enter_test_mode == 0
		&& system->status.timeout_wait_message_sync_data == 0)
    {
        system->peripheral_running.name.rs485_running = 0;
        usart_lpusart_485_control(0);
    }
    
    if (!cfg->io_enable.name.input_4_20ma_0_enable)
    {
        ENABLE_INPUT_4_20MA_POWER(0);
        system->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
        measure_input_turn_on_in_4_20ma_power = 0;
    }
    
    if (gsm_data_layer_is_module_sleeping())
    {
		system->status.disconnect_timeout_s = 0;
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
        system->status.disconnect_timeout_s = 0;
    }
    
    
    if (system->peripheral_running.value == 0)
    {
        adc_stop();
        if (system->status.is_enter_test_mode == 0 
            && m_delay_consider_wakeup == 0
            && (LL_GPIO_IsInputPinSet(ADC_24V_GPIO_Port, ADC_24V_Pin) == 0)
			&& system->status.timeout_wait_message_sync_data == 0)
        {
			jig_release_memory();
			RS485_POWER_EN(0);
			ENABLE_INPUT_4_20MA_POWER(0);
			system->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
			measure_input_turn_on_in_4_20ma_power = 0;
            sys_config_low_power_mode();
        }
    }
	
	if (system->status.timeout_wait_message_sync_data)
	{
		RS485_POWER_EN(1);
		usart_lpusart_485_control(1);
		if (jig_found_cmd_sync_data_to_host())
		{
			// Step 1 : Wakeup spi
			if (!system->peripheral_running.name.flash_running)
			{
				spi_init();
				app_spi_flash_wakeup();
				system->peripheral_running.name.flash_running = 1;
			}
		
			app_spi_flash_dump_to_485();
			
			// Step 2 : Shutdown spi
			app_spi_flash_shutdown();
            spi_deinit();
            system->peripheral_running.name.flash_running = 0;
		}
	}
	else
	{
		usart_lpusart_485_control(0);
	}
	
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
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
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMLOW);
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
	return uwTick;
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


static void info_task(void *arg)
{	
	static uint32_t i = 0;
    sys_ctx_t *system = sys_ctx();
    
	if (system->status.is_enter_test_mode)
	{
#if TEST_RS485
		usart_lpusart_485_control(1);
		sys_delay_ms(100);
		uint8_t result;
		uint16_t input_result[8];
		
		modbus_master_reset(1000);
		uint32_t before = sys_get_ms();
		 // read input registers function test
		 // slave address 0x01, two consecutive addresses are register 0x2
		result = modbus_master_read_input_register(8, 106, 8);
		if (result == 0x00)
		{
			DEBUG_INFO("Modbus read success in %ums\r\n", sys_get_ms() - before);
		
			for (uint32_t i = 0; i < 8; i++)
			{
				input_result[i] = modbus_master_get_response_buffer(i);
				DEBUG_RAW("(30%u-%u),", 106 + i, input_result[i]);
			}
			DEBUG_RAW("\r\n");
		}
		else
		{
			DEBUG_ERROR("Modbus failed\r\n");
		}
		usart_lpusart_485_control(0);
#endif
        i = 5;
	}
    if (i++ > 4)
	{
		i = 0;
		adc_input_value_t *adc = adc_get_input_result();
        rtc_date_time_t time;
        app_rtc_get_time(&time);
        char tmp[48];
        char *p = tmp;
        for (uint32_t i = 0; i < 4; i++)
        {
            p += sprintf(p, "(%.2f),", adc->in_4_20ma_in[i]);
        }
#ifdef DTG02
        for (uint32_t i = 0; i < 4; i++)
        {
            p += sprintf(p, "IN%u-%u,", i+1, measure_input_current_data()->input_on_off[i]);
        }
#endif
		if (gsm_data_layer_is_module_sleeping())
		{
			DEBUG_INFO("vdda %umv, bat_mv %u-%u, vin-24 %umV, 4-20mA %s temp %u\r\n",
						adc->vdda_mv,
						adc->bat_mv, adc->bat_percent, 
						adc->vin_24,
						tmp,
						adc->temp);
		}
#if TEST_CRC32
		uint32_t crc;
		static const char *feed_str0 ="12345";
		static const char *feed_str1 ="12349876";
		crc = utilities_calculate_crc32((uint8_t*)feed_str0, strlen(feed_str0));
		DEBUG_INFO("CRC0 0x%08X\r\n", crc);
		crc = utilities_calculate_crc32((uint8_t*)feed_str1, strlen(feed_str1));
		DEBUG_INFO("CRC1 0x%08X\r\n", crc);
#endif
		static sys_ctx_error_critical_t m_last_critical_err;
		sys_ctx_t *ctx = sys_ctx();
		app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
		
		// If rs485 was not enable =>> clear rs485 error code
		if (!eeprom_cfg->io_enable.name.rs485_en)
		{
			ctx->error_not_critical.detail.rs485_err = 0;
		}
		if (m_last_critical_err.value != ctx->error_critical.value)
		{
			m_last_critical_err.value = ctx->error_critical.value;
			
			if (!eeprom_cfg->io_enable.name.warning
				&& (strlen((char*)eeprom_cfg->phone) > 8)
				&& m_last_critical_err.value
				&& (ctx->status.total_sms_in_24_hour < eeprom_cfg->max_sms_1_day))
			{
				char msg[156];
				char *p = msg;
				rtc_date_time_t time;
				app_rtc_get_time(&time);
				
				p += sprintf(p, "[%04u/%02u/%02u %02u:%02u] ",
								time.year + 2000,
								time.month,
								time.day,
								time.hour,
								time.minute);
				if (m_last_critical_err.detail.sensor_out_of_range)
				{
					p += sprintf(p, "%s", "He thong bi loi luu luong");
				}
				ctx->status.total_sms_in_24_hour++;
				gsm_send_sms((char*)eeprom_cfg->phone, msg);
			}
		}

		// Reset sms limit
		static uint32_t scan;
		if (scan++ > 10)
		{
			scan = 0;
			if (ctx->status.total_sms_in_24_hour)
			{
				uint32_t counter = app_rtc_get_counter();
				counter = counter % 86400;
				if (counter < 300)
				{
					ctx->status.total_sms_in_24_hour = 0;
				}
			}
		}
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
//        DEBUG_VERBOSE("Before sleep - counter %u\r\n", counter_before_sleep);
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, WAKEUP_RESET_WDT_IN_LOW_POWER_MODE, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
        {
            Error_Handler();
        }
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        
        sys_enable_power_plug_detect();
        
        /* Disable GPIOs clock */
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOA);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOB);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOC);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOH);

        /* Disable SysTick Interrupt */
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
        
//          GPIO_InitTypeDef GPIO_InitStructure;
        __HAL_RCC_PWR_CLK_ENABLE();
        
        /* Enable the Ultra Low Power mode */
        SET_BIT(PWR->CR, PWR_CR_ULP);

        /* Enable the fast wake up */
        SET_BIT(PWR->CR, PWR_CR_FWU);
        

        
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
//        DEBUG_VERBOSE("Afer sleep - counter %u, diff %u\r\n", counter_after_sleep, diff);
                
//        ctx->status.sleep_time_s += diff;
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        m_wakeup_timer_run = false;
        DEBUG_VERBOSE("Wake\r\n");
        
        /* Enable SysTick Interrupt */
        SysTick->CTRL  |= SysTick_CTRL_TICKINT_Msk;
        
        SystemClock_Config();
#ifdef WDT_ENABLE
        LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    else
    {
//        DEBUG_PRINTF("RTC timer still running\r\n");
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    m_wakeup_timer_run = false;
//    DEBUG_VERBOSE("Wakeup timer event callback\r\n");
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
}

void sys_enable_power_plug_detect(void)
{
    LL_GPIO_SetPinPull(ADC_24V_GPIO_Port, ADC_24V_Pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(ADC_24V_GPIO_Port, ADC_24V_Pin, LL_GPIO_MODE_INPUT);
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
    
    EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_6;
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
    LL_EXTI_Init(&EXTI_InitStruct);
    
    NVIC_SetPriority(EXTI4_15_IRQn, 0);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}

void sys_disable_power_plug_config(void)
{
    LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_6);
    NVIC_DisableIRQ(EXTI4_15_IRQn);
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
