#ifndef APP_SPI_FLASH_H
#define APP_SPI_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

typedef union
{
	struct
	{
		uint8_t output_on_off_0 : 1;
		uint8_t output_on_off_1 : 1;
		uint8_t output_on_off_2 : 1;
		uint8_t output_on_off_3 : 1;
		uint8_t input_on_off_0 : 1;
		uint8_t input_on_off_1 : 1;
		uint8_t input_on_off_2 : 1;
		uint8_t input_on_off_3 : 1;
	} __attribute__((packed)) name;
	uint8_t value;
} __attribute__((packed)) app_spi_flash_on_off_data_t;

typedef struct
{
    uint32_t valid_flag;
    measure_input_counter_t meter_input[APP_FLASH_NB_OF_METER_INPUT];		// so nuoc
    measure_input_modbus_register_t rs485[RS485_MAX_SLAVE_ON_BUS];			// 485
    uint8_t temp;	// nhiet do
    uint16_t vbat_mv;		// dien ap pin
    uint8_t vbat_precent;
    float input_4_20mA[APP_FLASH_NB_OFF_4_20MA_INPUT];		// 4-20mA
	float output_4_20mA[NUMBER_OF_OUTPUT_4_20MA];
    app_spi_flash_on_off_data_t on_off;
	uint32_t timestamp;		//
	
    uint32_t resend_to_server_flag;
	uint32_t crc;
} __attribute__((packed)) app_spi_flash_data_t;


/**
 * @brief       Initialize spi flash
 */
void app_spi_flash_initialize(void);


/**
 * @brief       Estimate write address
 * @param[in]   flash_full : Flash full or not
 */
uint32_t app_flash_estimate_next_write_addr(bool *flash_full);

/**
 * @brief       Dump all valid data
 */
uint32_t app_spi_flash_dump_all_data(void);

/**
 * @brief       Erase all data in flash
 */
void app_spi_flash_erase_all(void);

/**
 * @brief       Get the last data
 * @param[in]   last_data Pointer will hold last write data
 * @retval      TRUE Operation success
 *              FALSE Operation failed
 */
bool app_spi_flash_get_lastest_data(app_spi_flash_data_t *last_data);

/**
 * @brief       Check flash ok status
 */
bool app_spi_flash_is_ok(void);

/**
 * @brief       Wakeup the flash
 */
void app_spi_flash_wakeup(void);

/**
 * @brief       Power down then flash
 */
void app_spi_flash_shutdown(void);

/**
 * @brief       Estimate current read address
 * @param[out]  found_error : Found error status
 * @param[in]	scan_all_flash TRUE Scan all flash
 *							   FALSE Only scan from read_pointer to write_pointer
 */
uint32_t app_spi_flash_estimate_current_read_addr(bool *found_error, bool scan_all_flash);

/**
 * @brief       Write data to flash
 * @param[in]   wr_data New data
 */
void app_spi_flash_write_data(app_spi_flash_data_t *wr_data);

/**
 * @brief       Check sector is empty or not
 * @param[in]   sector Sector count
 */
bool app_spi_flash_check_empty_sector(uint32_t sector_count);
    
/**
 * @brief       Flash stress write test
 * @param[in]   nb_of_write_times Number of write times
 */
void app_spi_flash_stress_test(uint32_t nb_of_write_times);

/**
 * @brief       Flash read all retransmission pending data
 * @param[in]   nb_of_write_times Number of write times
 */
void app_spi_flash_retransmission_data_test(void);

/**
 * @brief       Test write behavior if flash full
 */
void app_spi_flash_skip_to_end_flash_test(void);

/**
 * @brief       Flash read retransmission pending data
 * @param[in]   addr Address of data
 * @param[out]  rd_data Output data
 * @param[in]	only_error TRUE We only need message was not transmiss to server in the past
 *						   FALSE We get all data
 * @retval		TRUE Found message
				FALSE Not found message
 */
bool app_spi_flash_get_stored_data(uint32_t addr, app_spi_flash_data_t *rd_data, bool only_error);

/**
 * @brief       Dump all valid data to rs485 port
 * @retval		Number of valid packet
 */
uint32_t app_spi_flash_dump_to_485(void);

#endif /* APP_SPI_FLASH_H */
