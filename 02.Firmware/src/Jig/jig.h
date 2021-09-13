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

/**
 * @brief		Check cmd sync data from host
 * @retval		TRUE jig update cmd data found, device need to send data to server or rs485 port
 *              FALSE jig update cmd not found
 */
bool jig_found_cmd_sync_data_to_host(void);


/**
 * @brief		Check cmd change server
 * @retval		TRUE jig new server data found, device need to send data to server or rs485 port
 *              FALSE jig new server data not found
 */
bool jig_found_cmd_change_server(char **server_ptr, uint32_t *server_name_len);

/**
 * @brief		Check cmd change factory server
 * @retval		TRUE jig new server data found, device need to send data to server or rs485 port
 *              FALSE jig new server data not found
 */
bool jig_found_cmd_set_default_server_server(char **ptr, uint32_t *size);


/**
 * @brief		Check cmd get current config
 * @retval		TRUE jig new cmd data found, device need to send data to rs485 port
 *              FALSE jig new cmd data not found
 */
bool jig_found_cmd_get_config(void);

/**
 * @brief		Release JIG memory
 */
void jig_release_memory(void);

#endif /* ZIG_H */
