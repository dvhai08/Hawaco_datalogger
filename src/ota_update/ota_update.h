#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define OTA_UPDATE_DEFAULT_HEADER_DATA  "DTG02"
#define OTA_UPDATE_DEFAULT_HEADER_SIZE  16

/**
 * @brief		Start ota update process
 * @param[in]	expected_size : Firmware size, included header signature
 * @retval 		TRUE : Operation success
 *         		FALSE : Operation failed
 */
bool ota_update_start(uint32_t expected_size);

/**
 * @brief		Write data to flash
 * @param[in]	data : Data write to flash
 * @param[in]	length : Size of data
 * @note 		Flash write address will automatic increase inside function, size of date must multiply by 128
 * @retval		TRUE : Operation success
 *				FALSE : Operation failed
 */
bool ota_update_write_next(uint8_t *data, uint32_t length);

/**
 * @brief Finish ota process
 * @param[in] status TRUE : All data downloaded success
 *                   FALSE : A problem occurs
 */
void ota_update_finish(bool status);

/**
 * @brief		Check ota update status
 * @retval		TRUE : OTA is running
 * 				FALSE : OTA is not running
 */
bool ota_update_is_running(void);

#endif /* OTA_UPDATE_H */
