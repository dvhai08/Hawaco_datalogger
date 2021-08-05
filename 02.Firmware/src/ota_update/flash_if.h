#ifndef __FLASH_IF_H
#define __FLASH_IF_H
#include <stdint.h>

typedef enum 
{
	FLASH_IF_OK = 0,
	FLASH_IF_ERASE_KO,
	FLASH_IF_WRITING_CTRL_ERROR,
	FLASH_IF_WRITING_ERROR,
	FLASH_IF_PROTECTION_ERRROR
} flash_if_error_t;


/**
  * @brief                  Unlocks Flash for write access
  * @param                  None
  * @retval                 None
  */
void flash_if_init(void);

/**
  * @brief  				This function does an erase of all user flash area
  * @param  				start: start of user flash area
  * @retval 				FLASH_IF_OK : user flash area successfully erased
  *         				FLASH_IF_ERASE_KO : error occurred
  */
uint32_t flash_if_erase(uint32_t start);


/**
  * @brief                  This function writes a data buffer in flash (data are 32-bit aligned).
  * @note                   After writing data buffer, the flash content is checked.
  * @param                  destination: start address for target location
  * @param                  p_source: pointer on buffer with data to write
  * @param                  length: length of data buffer (unit is 32-bit word)
  * @retval                 uint32_t 0: Data successfully written to Flash memory
  *                         1: Error occurred while writing data in Flash memory
  *                         2: Written Data in flash memory is different from expected one
  */
uint32_t flash_if_write(uint32_t destination, uint32_t *p_source, uint32_t length);

/**
  * @brief  				This function does an erase of all ota info flash area
  * @param  				start: start of ota info flash area
  * @retval 				FLASH_IF_OK : ota info flash area successfully erased
  *         				FLASH_IF_ERASE_KO : error occurred
  */
uint32_t flash_if_erase_ota_info_page(void);

/**
  * @brief  				Write new ota info
  * @param  				data: New ota config
  * @param  				size: Size of data
  * @retval                 uint32_t 0: Data successfully written to Flash memory
  *                         1: Error occurred while writing data in Flash memory
  *                         2: Written Data in flash memory is different from expected one
  */
uint32_t flash_if_write_ota_info_page(uint32_t *data, uint32_t size);

#endif /* __FLASH_IF_H */

