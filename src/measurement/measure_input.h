#ifndef __MEASUREMENT_H__
#define __MEASUREMENT_H__

#include "hardware.h"

#define	MEASURE_INPUT_ADC_DMA_UNIT		4

#define	ADCMEM_V20mV	0		/* Chan ADC_IN4 */
#define	ADCMEM_VSYS		1		/* Chan ADC_IN6 */
#define ADCMEM_VTEM             2
#define ADCMEM_VINTREF          3

#define MEASURE_INPUT_PORT_0		0
#define MEASURE_INPUT_PORT_1		1

#define MEASURE_NUMBER_OF_WATER_METER_INPUT			2

typedef struct
{
	uint32_t port;
	uint32_t dir_level;
	uint32_t pwm_level;
	uint32_t line_break_detect;
} measure_input_water_meter_input_t;

typedef struct
{
	uint8_t output_on_off[4];
	uint8_t input_4_20mA[4];
	uint8_t input_on_off[4];
	uint8_t vbat_percent;
	uint8_t vbat_raw;
	uint8_t output_4_20mA;
	measure_input_water_meter_input_t water_pulse_counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
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
 * @brief       Poll measure input
 */

void MeasureTick1000ms(void);

/**
 * @brief       Read current measurement input data
 * @param[in]	data New uart data
 */
measure_input_perpheral_data_t *measure_input_current_data(void);

#endif // __MEASUREMENT_H__
