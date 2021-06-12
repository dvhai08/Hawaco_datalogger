#ifndef __MEASUREMENT_H__
#define __MEASUREMENT_H__

#include "hardware.h"

#define	MEASURE_INPUT_ADC_DMA_UNIT		4

#define	ADCMEM_V20mV	0		/* Chan ADC_IN4 */
#define	ADCMEM_VSYS		1		/* Chan ADC_IN6 */
#define ADCMEM_VTEM             2
#define ADCMEM_VINTREF          3

/**
 * @brief       Init measurement module 
 */
void measure_input_initialize(void);

/**
 * @brief       Poll input
 */
void measure_input_task(void);

void MeasureTick1000ms(void);

#endif // __MEASUREMENT_H__
