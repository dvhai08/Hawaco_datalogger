#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define OTA_FLAG_UPDATE_NEW_FW		0x12345678
#define OTA_FLAG_NO_NEW_FIRMWARE	0x19785384

#define BOOTLOADER_SIZE				(32*1024)
#define APPLICATION_SIZE			(152*1024)
#define OTA_INFO_SIZE               (128)
#define DOWNLOAD_SIZE				APPLICATION_SIZE

#define BOOTLOADER_START_ADDR		0x08000000
#define BOOTLOADER_END_ADDR			(BOOTLOADER_START_ADDR + BOOTLOADER_SIZE-1)

#define APPLICATION_START_ADDR		(BOOTLOADER_END_ADDR + 1)
#define APPLICATION_END_ADDR		(APPLICATION_START_ADDR + APPLICATION_SIZE-1)

#define DONWLOAD_START_ADDR         APPLICATION_START_ADDR
#define DONWLOAD_END_ADDR           APPLICATION_END_ADDR

#define OTA_INFO_START_ADDR			(DONWLOAD_END_ADDR+1)
#define OTA_INFO_END_ADDR			(OTA_INFO_START_ADDR + OTA_INFO_SIZE-1)

#define WORDS_IN_HALF_PAGE            ((uint32_t)16)
#define FLASH_HALF_PAGE_SIZE          ((uint32_t)WORDS_IN_HALF_PAGE*4)
#ifdef DTG01
#define OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE  "DTG01"
#define OTA_UPDATE_DEFAULT_HEADER_DATA_HARDWARE  "0.0.1"
#endif
#ifdef DTG02
#define OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE  "DTG02"
#define OTA_UPDATE_DEFAULT_HEADER_DATA_HARDWARE  "0.0.1"
#endif
#ifdef DTG02V2
#define OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE  "G2V2"
#define OTA_UPDATE_DEFAULT_HEADER_DATA_HARDWARE  "0.0.2"
#endif
#define OTA_UPDATE_DEFAULT_HEADER_SIZE  16

#define FLASH_END_BANK1               ((uint32_t)0x08017FFF)
#define FLASH_START_BANK2             ((uint32_t)0x08018000)

#define OTA_VERSION                   1


/**
 *	Bootloader		16KB
 *  Application		80KB
 *  Download		80KB
 *  OTA info		128 bytes
 */

typedef struct
{
	uint32_t flag;
	uint32_t firmware_size;
	uint32_t reserve[30];
} ota_flash_cfg_t;

/*!
 * @brief		Start ota update process
 * @param[in]	expected_size : Firmware size, included header signature
 * @retval 		TRUE : Operation success
 *         		FALSE : Operation failed
 */
bool ota_update_start(uint32_t expected_size);

/*!
 * @brief		Write data to flash
 * @param[in]	data : Data write to flash
 * @param[in]	length : Size of data in bytes
 * @note 		Flash write address will automatic increase inside function
 * @retval		TRUE : Operation success
 *				FALSE : Operation failed
 */
bool ota_update_write_next(uint8_t *data, uint32_t length);

/*!
 * @brief Finish ota process
 * @param[in] status TRUE : All data downloaded success
 *                   FALSE : A problem occurs
 */
void ota_update_finish(bool status);

/*!
 * @brief		Check ota update status
 * @retval		TRUE : OTA is running
 * 				FALSE : OTA is not running
 */
bool ota_update_is_running(void);

/*!
 * @brief		Get current ota update
 * @retval		OTA config in flash
 */
ota_flash_cfg_t *ota_update_get_config(void);

/*!
 * @brief		Set ota image size
 * @param[in]	size : Size of image
 */
void ota_update_set_expected_size(uint32_t size);

/*!
 * @brief		Write all remain data into flash
 */
bool ota_update_commit_flash(void);

#endif /* OTA_UPDATE_H */
