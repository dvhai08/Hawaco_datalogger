#ifndef APP_SPI_FLASH_H
#define APP_SPI_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#define APP_FLASH_VALID_DATA_KEY                    0x12345678               
#ifdef DTG01
#define APP_FLASH_NB_OFF_4_20MA_INPUT               2
#define APP_FLASH_NB_OF_METER_INPUT                 1
#else
#define APP_FLASH_NB_OFF_4_20MA_INPUT               4
#define APP_FLASH_NB_OF_METER_INPUT                 2
#endif
#define APP_FLASH_RS485_MAX_SIZE                    16
#define APP_SPI_FLASH_SIZE						    (1024*1024)
#define APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG  0xA5A5A5A5
#define APP_FLASH_DATA_HEADER_KEY                   0x9813567A

typedef struct
{
    uint8_t slave_addr;
    uint8_t nb_of_register;
    uint16_t register_index;
	uint8_t symbol[6];		// m3/s, kg...
    uint8_t data[APP_FLASH_RS485_MAX_SIZE];
} __attribute__((packed)) app_spi_flash_rs485_data_t;

typedef struct
{
    uint32_t pwm_f;     // forward
    uint32_t dir_r;     // reserve
} __attribute__((packed)) app_spi_flash_measurement_water_input_data_t;

typedef struct
{
    uint32_t valid_flag;
    app_spi_flash_measurement_water_input_data_t meter_input[APP_FLASH_NB_OF_METER_INPUT];
    app_spi_flash_rs485_data_t rs485[2];
    uint8_t temp;
    uint16_t vbat_mv;
    uint8_t vbat_precent;
    float input_4_20mA[APP_FLASH_NB_OFF_4_20MA_INPUT];
    uint32_t timestamp;
    uint32_t resend_to_server_flag;
	uint16_t crc;
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
 */
uint32_t app_spi_flash_estimate_current_read_addr(bool *found_error);

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
 * @param[in]   rd_data Output data
 */
bool app_spi_flash_get_retransmission_data(uint32_t addr, app_spi_flash_data_t *rd_data);

#endif /* APP_SPI_FLASH_H */
