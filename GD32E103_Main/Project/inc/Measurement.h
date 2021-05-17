#ifndef __MEASUREMENT_H__
#define __MEASUREMENT_H__

#include "Hardware.h"

/************************* Analog  ***************************/
#define	ADCMEM_MAXUNIT		4

#define	ADCMEM_V20mV	0		/* Chan ADC_IN4 */
#define	ADCMEM_VSYS		1		/* Chan ADC_IN6 */
#define ADCMEM_VTEM             2
#define ADCMEM_VINTREF          3

//#define	ADCMEM_VCELL	2		/* Chan ADC_IN20 */
//#define	ADCMEM_VBAT		2		/* Chan ADC_IN5 */
//#define	ADCMEM_VLED		3		/* Chan ADC_IN6 */

void Measure_Init(void);
void Measure_Tick(void);
void Measure_PulseTick(void);
void MeasureTick1000ms(void);

#endif // __MEASUREMENT_H__
