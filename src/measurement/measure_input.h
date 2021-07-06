#ifndef MEASURE_INTPUT_H
#define MEASURE_INTPUT_H

#include "hardware.h"

#include <stdint.h>
#include <stdbool.h>

#define	MEASURE_INPUT_ADC_DMA_UNIT		4

#define	ADCMEM_V20mV	0		/* Chan ADC_IN4 */
#define	ADCMEM_VSYS		1		/* Chan ADC_IN6 */
#define ADCMEM_VTEM             2
#define ADCMEM_VINTREF          3

#define MEASURE_INPUT_PORT_0		0

#ifdef DTG02
#define NUMBER_OF_INPUT_4_20MA						4
#define MEASURE_NUMBER_OF_WATER_METER_INPUT			2
#define	NUMBER_OF_INPUT_ON_OFF						4
#define NUMBER_OF_OUT_ON_OFF						4
#define MEASURE_INPUT_PORT_1		                1
#else
#define NUMBER_OF_INPUT_4_20MA						1
#define NUMBER_OF_OUTPUT_4_20MA                     1
#define MEASURE_NUMBER_OF_WATER_METER_INPUT			1		
#endif
#define MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN         0
#define MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN         1
#define MEASUREMENT_MAX_MSQ_IN_RAM                  24

#define MEASUREMENT_QUEUE_STATE_IDLE                0
#define MEASUREMENT_QUEUE_STATE_PENDING             1       // Dang cho de doc
#define MEASUREMENT_QUEUE_STATE_PROCESSING          2       // Da doc nhung dang xu li, chua release thanh free

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
#ifdef DTG02
	uint8_t output_on_off[4];
	uint8_t input_on_off[NUMBER_OF_INPUT_ON_OFF];
#endif
	uint8_t input_4_20mA[NUMBER_OF_INPUT_4_20MA];
	uint8_t vbat_percent;
	uint8_t vbat_raw;
	uint8_t output_4_20mA;
	measure_input_water_meter_input_t water_pulse_counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
    int32_t temperature;
    int32_t temperature_error;
} measure_input_perpheral_data_t;

typedef struct
{
    uint32_t measure_timestamp;
    uint8_t vbat_percent;
    uint16_t vbat_mv;
    uint32_t counter0_f;
    uint32_t counter0_r;
    uint32_t counter1_f;
    uint32_t counter1_r;
    uint8_t input_4_20ma[NUMBER_OF_INPUT_4_20MA];
    uint8_t csq_percent;
    uint8_t state;
    uint8_t temperature;
} measurement_msg_queue_t;

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
measurement_msg_queue_t *measure_input_get_data_in_queue(void);

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
