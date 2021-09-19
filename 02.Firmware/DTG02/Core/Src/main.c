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
#include "utilities.h"
#include "jig.h"
#include "umm_malloc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define WAKEUP_RESET_WDT_IN_LOW_POWER_MODE            37000    // ( ~16s)
#define DEBUG_LOW_POWER                                 0
#define DISABLE_GPIO_ENTER_LOW_POWER_MODE               0
#define TEST_POWER_ALWAYS_TURN_OFF_GSM                  0
#define TEST_OUTPUT_4_20MA                              0
#define TEST_RS485                                      0
#define TEST_INPUT_4_20_MA                              0
#define TEST_BACKUP_REGISTER                            0
#define TEST_DEVICE_NEVER_SLEEP							0
#define TEST_CRC32										0
#define CLI_ENABLE                                      1
#define GSM_ENABLE										1
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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
volatile uint8_t g_umm_heap[UMM_MALLOC_CFG_HEAP_SIZE];
static volatile uint32_t m_delay_afer_wakeup_from_deep_sleep_to_measure_data;
static void info_task(void *arg);
volatile uint32_t led_blink_delay = 0;
void sys_config_low_power_mode(void);
extern volatile uint32_t measure_input_turn_on_in_4_20ma_power;
volatile pulse_irq_t recheck_input_pulse[MEASURE_NUMBER_OF_WATER_METER_INPUT];
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

	hardware_manager_get_reset_reason();
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		recheck_input_pulse[i].tick = 0;
	}
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
	MX_CRC_Init();
	app_eeprom_init();
	app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    sys_ctx_t *system = sys_ctx();
#if TEST_OUTPUT_4_20MA || TEST_RS485
	eeprom_cfg->io_enable.name.output_4_20ma_enable = 1;
    system->status.is_enter_test_mode = 1;
#endif
//#if TEST_INPUT_4_20_MA
//    eeprom_cfg->io_enable.name.input_4_20ma_enable = 1;
//    system->status.is_enter_test_mode = 1;
//#endif

#if 1
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_IWDG_Init();
  MX_RTC_Init();
  MX_ADC_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_LPUART1_UART_Init();
  MX_SPI2_Init();
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
        
//	DEBUG_RAW(RTT_CTRL_CLEAR);
    gpio_config_input_as_wakeup_source();
    system->peripheral_running.name.flash_running = 1;
    system->peripheral_running.name.rs485_running = 1;
#if CLI_ENABLE
	app_cli_start();
#endif
	app_bkup_init();
    app_spi_flash_initialize();
	measure_input_initialize();
	control_ouput_init();
//	adc_start();
	gsm_init_hw();
	BUZZER(0);
#if USE_SYNC_DRV
	app_sync_config_t config;
	config.get_ms = sys_get_ms;
	config.polling_interval_ms = 1;
	app_sync_sytem_init(&config);
	
	app_sync_register_callback(gsm_mnr_task, 1000, SYNC_DRV_REPEATED, SYNC_DRV_SCOPE_IN_LOOP);
	app_sync_register_callback(info_task, 1000, SYNC_DRV_REPEATED, SYNC_DRV_SCOPE_IN_LOOP);
#endif

#if TEST_OUTPUT_4_20MA
	eeprom_cfg->io_enable.name.output_4_20ma_enable = 1;
	eeprom_cfg->output_4_20ma = 10;
	eeprom_cfg->io_enable.name.output_4_20ma_timeout_100ms = 100;
	control_output_dac_enable(1000000);
    system->status.is_enter_test_mode = 1;
    eeprom_cfg->io_enable.name.input_4_20ma_enable = 1;
#endif     
#if TEST_INPUT_4_20_MA
    eeprom_cfg->io_enable.name.input_4_20ma_enable = 1;
    system->status.is_enter_test_mode = 1;
#endif

    DEBUG_INFO("Build %s %s, version %s\r\n", __DATE__, __TIME__, VERSION_CONTROL_FW);
	jig_start();
    static uint32_t button_factory_timeout = 0;
    bool do_factory_reset = false;
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
#if CLI_ENABLE
	app_cli_poll();
#endif 
#if USE_SYNC_DRV
	app_sync_polling_task();
#else
      static volatile uint32_t m_last_tick = 0;
      if (sys_get_ms() - m_last_tick >= (uint32_t)1000)
      {
          m_last_tick = sys_get_ms();
          gsm_mnr_task(NULL);
          info_task(NULL);
      }
#endif
	if (led_blink_delay)
	{
		LED1(1);
#ifdef DTG01
		LED2(1);
#endif
	}
	else
	{
		LED1(0);
#ifdef DTG01
		LED2(0);
#endif
	}	
	if (system->status.is_enter_test_mode)
	{
		eeprom_cfg->io_enable.name.output_4_20ma_enable = 1;
		if (eeprom_cfg->output_4_20ma < 1.0f)
			eeprom_cfg->output_4_20ma = 10;
		eeprom_cfg->io_enable.name.output_4_20ma_timeout_100ms = 100;
		control_output_dac_enable(1000000);
		eeprom_cfg->io_enable.name.rs485_en = 1;
		ENABLE_INPUT_4_20MA_POWER(1);
        RS485_POWER_EN(1);
        usart_lpusart_485_control(1);
	}
    
    if (!eeprom_cfg->io_enable.name.rs485_en
        && system->status.is_enter_test_mode == 0
		&& system->status.timeout_wait_message_sync_data == 0)
    {
        system->peripheral_running.name.rs485_running = 0;
        usart_lpusart_485_control(0);
    }

    
#ifdef DTG01
    if (!eeprom_cfg->io_enable.name.input_4_20ma_enable)
#else
	    if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable == 0
			 && eeprom_cfg->io_enable.name.input_4_20ma_1_enable == 0
			 && eeprom_cfg->io_enable.name.input_4_20ma_2_enable == 0
			 && eeprom_cfg->io_enable.name.input_4_20ma_3_enable == 0)
#endif
    {
        ENABLE_INPUT_4_20MA_POWER(0);
        system->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
        measure_input_turn_on_in_4_20ma_power = 0;
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
        system->status.disconnect_timeout_s = 0;
    }
    
	if (system->status.timeout_wait_message_sync_data)
	{
		RS485_POWER_EN(1);
		usart_lpusart_485_control(1);
        if (LL_GPIO_IsInputPinSet(OPTOIN2_GPIO_Port, OPTOIN2_Pin) == 0)
        {
            uint32_t now = sys_get_ms();
            static uint32_t m_last_blink = 0;
            if (button_factory_timeout == 0)
            {
                button_factory_timeout = now;
            }
            if (now - button_factory_timeout >= (uint32_t)7000) // button hold for 5s =>> factory reset
            {
                DEBUG_INFO("Factory reset server\r\n");
                if (strcmp(DEFAULT_SERVER_ADDR, (char*)app_eeprom_read_factory_data()->server))
                {
                    app_eeprom_factory_data_t *pre_data = app_eeprom_read_factory_data();
                    app_eeprom_factory_data_t new_data;
                    
                    memcpy(&new_data, pre_data, sizeof(app_eeprom_factory_data_t));
                    memset(new_data.server, 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
                    memcpy(new_data.server, DEFAULT_SERVER_ADDR, strlen(DEFAULT_SERVER_ADDR));
                    app_eeprom_save_factory_data(&new_data);
                 
                    memset(eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
                    memset(eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
                    
                    memcpy((char*)eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], DEFAULT_SERVER_ADDR, strlen(DEFAULT_SERVER_ADDR));
                    app_eeprom_save_config();		// Store current config into eeprom                 
                }
                usart_lpusart_485_send((uint8_t*)"Factory reset server ", strlen("Factory reset server "));
                usart_lpusart_485_send((uint8_t*)DEFAULT_SERVER_ADDR, strlen(DEFAULT_SERVER_ADDR));
                button_factory_timeout = now;
                do_factory_reset = true;
            }
            
                        
            if (now - m_last_blink >= (uint32_t)100)
            {
                m_last_blink = now;
                if (do_factory_reset)
                {
                    LL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
                }
            }
        }
        else
        {
            button_factory_timeout = sys_get_ms();
            do_factory_reset = false;
        }
        

        char *server;
        uint32_t server_len;
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
        else if (jig_found_cmd_change_server(&server, &server_len))
        {
            server[server_len] = 0;
            usart_lpusart_485_send((uint8_t*)"Set new server ", strlen("Set new server "));
            usart_lpusart_485_send((uint8_t*)server, strlen(server));
            memset(eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
            memset(eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
            memcpy((char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX], server, server_len);
            app_eeprom_save_config();	
            jig_release_memory();
        }
        else if (jig_found_cmd_set_default_server_server(&server, &server_len))
        {
            server[server_len] = 0;
            usart_lpusart_485_send((uint8_t*)"Set default server ", strlen("Set default server "));
            usart_lpusart_485_send((uint8_t*)server, strlen(server));
            app_eeprom_factory_data_t *pre_data = app_eeprom_read_factory_data();
            app_eeprom_factory_data_t new_data;
            
            memcpy(&new_data, pre_data, sizeof(app_eeprom_factory_data_t));
            memset(new_data.server, 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
            memcpy(new_data.server, server, server_len);
            app_eeprom_save_factory_data(&new_data);
            
            jig_release_memory();
        }
        else if (jig_found_cmd_get_config())
        {
            char *config = umm_malloc(1024);     // TODO check malloc result
            uint32_t len = 0;
            
            app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
            len += snprintf((char *)(config + len), APP_EEPROM_MAX_SERVER_ADDR_LENGTH, "{\"Server0\":\"%s\",", eeprom_cfg->server_addr[0]);
            len += snprintf((char *)(config + len), APP_EEPROM_MAX_SERVER_ADDR_LENGTH, "\"Server1\":\"%s\",", eeprom_cfg->server_addr[1]);
            len += sprintf((char *)(config + len), "\"CycleSendWeb\":%u,", eeprom_cfg->send_to_server_interval_ms/1000/60);
            len += sprintf((char *)(config + len), "\"DelaySendToServer\":%u,", eeprom_cfg->send_to_server_delay_s);
            len += sprintf((char *)(config + len), "\"Cyclewakeup\":%u,", eeprom_cfg->measure_interval_ms/1000/60);
            len += sprintf((char *)(config + len), "\"MaxSmsOneday\":%u,", eeprom_cfg->max_sms_1_day);
            len += sprintf((char *)(config + len), "\"Phone\":\"%s\",", eeprom_cfg->phone);
            len += sprintf((char *)(config + len), "\"PollConfig\":%u,", eeprom_cfg->poll_config_interval_hour);
            len += sprintf((char *)(config + len), "\"DirLevel\":%u,", eeprom_cfg->dir_level);
            for (uint32_t i = 0; i < APP_EEPROM_METER_MODE_MAX_ELEMENT; i++)
            {
                len += sprintf((char *)(config + len), "\"K%u\":%u,", i+1, eeprom_cfg->k[i]);
                len += sprintf((char *)(config + len), "\"M%u\":%u,", i+1, eeprom_cfg->meter_mode[i]);
                len += sprintf((char *)(config + len), "\"MeterIndicator%u\":%u,", i+1, eeprom_cfg->offset[i]);
            }
            
            len += sprintf((char *)(config + len), "\"OutputIO_0\":%u,", eeprom_cfg->io_enable.name.output0);
            len += sprintf((char *)(config + len), "\"OutputIO_1\":%u,", eeprom_cfg->io_enable.name.output1);
            len += sprintf((char *)(config + len), "\"OutputIO_2\":%u,", eeprom_cfg->io_enable.name.output2);
            len += sprintf((char *)(config + len), "\"OutputIO_3\":%u,", eeprom_cfg->io_enable.name.output3);
            len += sprintf((char *)(config + len), "\"Input0\":%u,", eeprom_cfg->io_enable.name.input0);
            len += sprintf((char *)(config + len), "\"Input1\":%u,", eeprom_cfg->io_enable.name.input1);
            len += sprintf((char *)(config + len), "\"Input2\":%u,", eeprom_cfg->io_enable.name.input2);
            len += sprintf((char *)(config + len), "\"Input3\":%u,", eeprom_cfg->io_enable.name.input3);
            
        //    len += sprintf((char *)(config + len), "\"SOS\":%u,", eeprom_cfg->io_enable.name.sos);
            len += sprintf((char *)(config + len), "\"Warning\":%u,", eeprom_cfg->io_enable.name.warning);
            len += sprintf((char *)(config + len), "\"485_EN\":%u,", eeprom_cfg->io_enable.name.rs485_en);
            if (eeprom_cfg->io_enable.name.rs485_en)
            {
                for (uint32_t slave_idx = 0; slave_idx < RS485_MAX_SLAVE_ON_BUS; slave_idx++)
                {
                    for (uint32_t sub_register_index = 0; sub_register_index < RS485_MAX_SUB_REGISTER; sub_register_index++)
                    {
                        if (eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].data_type.name.valid == 0)
                        {
                            uint32_t slave_addr = eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].read_ok = 0;
                            continue;
                        }
                        len += sprintf((char *)(config + len), 
                                        "\"485_%u_Slave\":%u,", 
                                        slave_idx, 
                                        eeprom_cfg->rs485[slave_idx].slave_addr);
                        len += sprintf((char *)(config + len), 
                                                "\"485_%u_Reg\":%u,", 
                                                slave_idx, 
                                                eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].register_addr);
                        len += snprintf((char *)(config + len), 6, 
                                                "\"485_%u_Unit\":\"%s\",", 
                                                slave_idx, 
                                                (char*)eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].unit);
                    }
                }
            }
            
            len += sprintf((char *)(config + len), "\"input4_20mA_0\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_0_enable);
        #ifndef DTG01
            len += sprintf((char *)(config + len), "\"input4_20mA_1\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_1_enable);
            len += sprintf((char *)(config + len), "\"input4_20mA_2\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_2_enable);
            len += sprintf((char *)(config + len), "\"input4_20mA_3\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_3_enable);
        #endif
            len += sprintf((char *)(config + len), "\"Output4_20mA_En\":%u,", eeprom_cfg->io_enable.name.output_4_20ma_enable);
            if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
            {
                len += sprintf((char *)(config + len), "\"Output4_20mA_Val\":%.3f,", eeprom_cfg->output_4_20ma);
            }
            
            len += sprintf((char *)(config + len), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
            len += sprintf((char *)(config + len), "\"HW\":\"%s\",", VERSION_CONTROL_HW);
            len += sprintf((char *)(config + len), "\"FactoryServer\":\"%s\",", app_eeprom_read_factory_data()->server);
            
            len--;      // Skip ','
            len += sprintf((char *)(config + len), "%s", "}");     
            usart_lpusart_485_send((uint8_t*)config, len);
            
            umm_free(config);
    
            jig_release_memory();
        }
	}
	else
	{
		usart_lpusart_485_control(0);
	}
	
	    
    if (system->peripheral_running.value == 0)
    {
        adc_stop();
        if (system->status.is_enter_test_mode == 0
			&& recheck_input_pulse[0].tick == 0
			&& recheck_input_pulse[1].tick == 0
			&& system->status.timeout_wait_message_sync_data == 0)
        {
			
			#if TEST_DEVICE_NEVER_SLEEP == 0
			{
				RS485_POWER_EN(0);
				ENABLE_INPUT_4_20MA_POWER(0);
				system->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
				measure_input_turn_on_in_4_20ma_power = 0;
				LED1(0);
#ifdef DTG01
				LED2(0);
#endif
				sys_config_low_power_mode();
			}
			#endif
        }
    }
    BUZZER(0);
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
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
        for (uint32_t i = 0; i < 4; i++)
        {
            p += sprintf(p, "IN%u-%u,", i+1, measure_input_current_data()->input_on_off[i]);
        }
        
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
				&& m_last_critical_err.value)
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
				gsm_send_sms((char*)eeprom_cfg->phone, msg);
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
        SET_BIT(DBGMCU->CR, DBGMCU_CR_DBG_STOP);        // Enable debug stop mode
#endif
        
#ifdef WDT_ENABLE
        LL_IWDG_ReloadCounter(IWDG);
#endif
        
        uint32_t counter_before_sleep = app_rtc_get_counter();
        uint32_t sleep_time = ((measure_input_get_next_time_wakeup()+1)*32768)/16;
        if (sleep_time > WAKEUP_RESET_WDT_IN_LOW_POWER_MODE)
        {
            sleep_time = WAKEUP_RESET_WDT_IN_LOW_POWER_MODE;
        }        
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, sleep_time, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
        {
            Error_Handler();
        }
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        
        /* Disable GPIOs clock */
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOA);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOB);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOC);
        LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOH);
        
        // Suspend tick
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
        
//          GPIO_InitTypeDef GPIO_InitStructure;
        __HAL_RCC_PWR_CLK_ENABLE();
        
        /* Enable the Ultra Low Power mode */
        SET_BIT(PWR->CR, PWR_CR_ULP);

        /* Enable the fast wake up from Ultra low power mode */
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
                
//        ctx->status.sleep_time_s += diff;
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        m_wakeup_timer_run = false;
        DEBUG_VERBOSE("Wakeup\r\n");
        
        // Resume tick
        SysTick->CTRL  |= SysTick_CTRL_TICKINT_Msk;
        
        SystemClock_Config();
		LED1(1);
#ifdef DTG01
		LED2(1);
#endif
		sys_delay_ms(5);

#ifdef WDT_ENABLE
        LL_IWDG_ReloadCounter(IWDG);
#endif
    }
//    else
//    {
//        DEBUG_WARN("RTC timer still running\r\n");
//    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    m_wakeup_timer_run = false;
    HAL_RTCEx_DeactivateWakeUpTimer(hrtc);
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	DEBUG_ERROR("Assert failed %s, line %u\r\n", file, line);
    NVIC_SystemReset();
	while (1);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
