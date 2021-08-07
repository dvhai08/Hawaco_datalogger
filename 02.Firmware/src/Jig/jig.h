#ifndef JIG_H
#define JIG_H

#include <stdint.h>

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

#endif /* ZIG_H */
