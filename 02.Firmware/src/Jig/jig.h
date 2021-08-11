#ifndef JIG_H
#define JIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	char *tx_ptr;
	char *rx_ptr;
	uint32_t rx_idx;
} jig_rs485_buffer_t;

/**
 * @brief		Start jig test
 */
void jig_start(void);

/**
 * @brief		Insert jig rx data
 * @param[in]	data New data bytes
 */
void jig_uart_insert(uint8_t data);

/**
 * @brief		Check jig in dectected or not
 * @retval		TRUE	jig detect, system is in test mode
 * @retval		FALSE	jig not detect
 */
bool jig_is_in_test_mode(void);

#endif /* ZIG_H */