#ifndef CONTROL_OUTPUT_H
#define CONTROL_OUTPUT_H

#include "hardware.h"

/*!
 * @brief       Init output peripheral 
 */
void control_ouput_init(void);

/*!
 * @brief       Control output task
 */
void control_ouput_task(void);


/*!
 * @brief		Control DAC output
 * @param[in]	timeout_ms	Timeout in miliseconds
 */
void control_output_dac_enable(uint32_t timeout_ms);

/*!
 * @brief       Start measure 4-20mA
 */
void control_output_start_measure(void);

#endif // CONTROL_OUTPUT_H
