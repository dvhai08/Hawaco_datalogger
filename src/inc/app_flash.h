#ifndef APP_FLASH_H
#define APP_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#define APP_FLASH_FLAG_WRITTEN          0xAA

typedef struct
{
    uint16_t network_id;
    uint8_t device_id;
    uint8_t flag;
} __attribute__ ((packed)) app_flash_network_info_t;

/**
 * @brief               Get device mac_address
 */
void app_flash_get_mac_addr(uint8_t *mac);

/**
 * @brief               Store network information into flash
 * @param[in]           info Network information
 */
void app_flash_store_info(app_flash_network_info_t *info);

/**
 * @brief               Get network information from flash
 * @param[out]          info Network information
 */
void app_flash_read_info(app_flash_network_info_t *info);

/**
 * @brief               Get network information from flash
 * @retval              TRUE Network information stored in flash
 *                      FALSE Network information is not stored in flash
 */
bool app_flash_infor_is_exist_in_flash(void);

#endif /* APP_FLASH_H */
