#ifndef MEASURE_INTPUT_H
#define MEASURE_INTPUT_H

#include "hardware.h"
#include "app_spi_flash.h"
#include <stdint.h>
#include <stdbool.h>


typedef struct
{
	uint32_t port;
	uint32_t dir_level;
	uint32_t pwm_level;
	uint32_t line_break_detect;
    uint32_t new_data_type;
} measure_input_water_meter_input_t;

typedef struct
{
	uint32_t measure_timestamp;
    uint8_t output_on_off[NUMBER_OF_OUTPUT_ON_OFF];
// Input
	float input_4_20mA[NUMBER_OF_INPUT_4_20MA];
#if defined(DTG02) || defined(DTG02V2)
	uint8_t input_on_off[NUMBER_OF_INPUT_ON_OFF];
#endif
	
	// Oupput 	
	float output_4_20mA[NUMBER_OF_OUTPUT_4_20MA];
	
	// Battery
	uint8_t vbat_percent;
	uint16_t vbat_mv;
	float vin_mv;

//	measure_input_water_meter_input_t water_pulse_counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
	measure_input_counter_t counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
	
	// Temperature
    int32_t temperature;
    int32_t temperature_error;
	
	// RS485
    measure_input_modbus_register_t rs485[RS485_MAX_SLAVE_ON_BUS];
	// CSQ
	
	uint8_t csq_percent;
	
	uint8_t state;		// memory queue state
} measure_input_perpheral_data_t;


/**
 * @brief       Init measurement module 
 */
void measure_input_initialize(void);

/**
 * @brief       Poll input
 */
void measure_input_task(void);

///**
// * @brief       Get measure input counter
// */
//measure_input_water_meter_input_t *measure_input_get_backup_counter(void);

/**
 * @brief       RS485 new uart data
 * @param[in]	data New uart data
 */
void measure_input_rs485_uart_handler(uint8_t data);

/**
 * @brief       RS485 IDLE detecte
 */
void measure_input_rs485_idle_detect(void);


/**
 * @brief       Read current measurement input data
 * @param[in]	data New uart data
 */
measure_input_perpheral_data_t *measure_input_current_data(void);

/**
 * @brief       Reset counter in backup domain
 * @param[in]	index Pulse meter index
 */
void measure_input_reset_counter(uint8_t index);

/**
 * @brief       Reset indicator offset
 * @param[in]	index Pulse meter index
 * @param[in]	new_indicator Pulse indicator
 */
void measure_input_reset_indicator(uint8_t index, uint32_t new_indicator);

/**
 * @brief       Reset k offset
 * @param[in]	index k meter
 * @param[in]	new_k K divider
 */
void measure_input_reset_k(uint8_t index, uint32_t new_k);
	
/**
 * @brief       Measure input callback
 * @param[in]	input New input data
 */
void measure_input_pulse_irq(measure_input_water_meter_input_t *input);

/**
 *  @brief      Wakeup to start measure data
 */
void measure_input_measure_wakeup_to_get_data(void);

/**
 * @brief       Check device has new sensor data
 * @retval      TRUE : New sensor data availble
 *              FALSE : No new sensor data
 */
bool measure_input_sensor_data_availble(void);

/**
 * @brief       Get data in sensor message queue
 * @retval      Pointer to queue, NULL on no data availble
 */
measure_input_perpheral_data_t *measure_input_get_data_in_queue(void);

/**
 * @brief       Save all data in sensor message queue to flash 
 */
void measure_input_save_all_data_to_flash(void);

/**
 * @brief       Set delay time to enable input 4_20ma
 * @param[in]   ms Delay time in ms
 */
void measure_input_delay_delay_measure_input_4_20ma(uint32_t ms);

#endif /* MEASURE_INTPUT_H */
