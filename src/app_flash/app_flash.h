#ifndef APP_FLASH_H
#define APP_FLASH_H

#define APP_FLASH_VALID_DATA_KEY                    0x12345678                
#define APP_FLASH_NB_OF_METER_INPUT                 2
#define APP_FLASH_NB_OFF_4_20MA_INPUT               4
#define APP_FLASH_RS485_MAX_SIZE                    32
#define APP_FLASH_SIZE						        (1024*1024)
#define APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG  0xFFFFFFFF
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t pwm_f;     // forward
    uint32_t dir_r;     // reserve
} app_flash_measurement_water_input_data_t;

typedef struct
{
    app_flash_measurement_water_input_data_t meter_input[APP_FLASH_NB_OF_METER_INPUT];
    uint8_t rs485[APP_FLASH_RS485_MAX_SIZE];
    uint8_t temp;
    uint16_t vbat;
    uint8_t input_4_20mA[APP_FLASH_NB_OFF_4_20MA_INPUT];		// 1.3mA =>> 13
    uint32_t timestamp;
    uint32_t system_code;
    uint32_t write_index;
    uint32_t valid_flag;
    uint32_t resend_to_server_flag;
} app_flash_data_t;


/**
 * @brief       Initialize flash
 */
void app_flash_initialize(void);


/**
 * @brief       Get data from 
* @param[in]   flash_full : Flash full or not
 */
uint32_t app_flash_estimate_next_write_addr(bool *flash_full);

/**
 * @brief       Dump all valid data
 */
uint32_t app_flash_dump_all_data(void);

/**
 * @brief       Erase all data in flash
 */
void app_flash_erase(void);

/**
 * @brief       Get the last data
 */
app_flash_data_t *app_flash_lastest_data(void);

/**
 * @brief       Check flash ok status
 */
bool app_flash_is_ok(void);

/**
 * @brief       Wakeup the flash
 */
void app_flash_wakeup(void);

/**
 * @brief       Power down then flash
 */
void app_flash_shutdown(void);

/**
 * @brief       Get spi flash error status
 * @retval      TRUE SPI flash error
 *              FALSE SPI ok
 */
bool app_flash_is_error(void);

/**
 * @brief       Estimate current read address
 * @param[out]  found_error : Found error status
 */
uint32_t app_flash_estimate_current_read_addr(bool *found_error);

/**
 * @brief       Flash stress write test
 * @param[in]   nb_of_write_times Number of write times
 */
void app_flash_stress_test(uint32_t nb_of_write_times);

/**
 * @brief       Flash read all test data
 * @param[in]   nb_of_write_times Number of write times
 */
void app_flash_read_test(void);

#endif /* APP_FLASH_H */
