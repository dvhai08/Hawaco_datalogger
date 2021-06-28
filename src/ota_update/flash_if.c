#include "flash_if.h"
#include "main.h"
#include "app_debug.h"
#include "ota_update.h"

#define ABS_RETURN(x,y)               (((x) < (y)) ? (y) : (x))


void flash_if_init(void)
{
  /* Unlock the Program memory */
  HAL_FLASH_Unlock();

  /* Clear all FLASH flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR |
                         FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | FLASH_FLAG_FWWERR |
                         FLASH_FLAG_NOTZEROERR);
  /* Unlock the Program memory */
  HAL_FLASH_Lock();
}

uint32_t flash_if_erase(uint32_t start)
{
    DEBUG_PRINTF("Flash erase at addr 0x%08X\r\n", start);

    flash_if_init();

    FLASH_EraseInitTypeDef desc;
    uint32_t result = FLASH_IF_OK;
    uint32_t page_error;

    HAL_FLASH_Unlock();

    desc.PageAddress = start;
    desc.TypeErase = FLASH_TYPEERASE_PAGES;

    /* NOTE: Following implementation expects the IAP code address to be < Application address */
    if (start < FLASH_START_BANK2)
    {
        desc.NbPages = (FLASH_START_BANK2 - start) / FLASH_PAGE_SIZE;
        if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
        {
            result = FLASH_IF_ERASE_KO;
        }
    }

    if (result == FLASH_IF_OK)
    {
        desc.PageAddress = ABS_RETURN(start, FLASH_START_BANK2);
        desc.NbPages = (DONWLOAD_END_ADDR - desc.PageAddress) / FLASH_PAGE_SIZE;
        if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
        {
            result = FLASH_IF_ERASE_KO;
        }
    }

    HAL_FLASH_Lock();
    DEBUG_PRINTF("Erase flash error code %u\r\n", result);
    return result;
}

uint32_t flash_if_erase_ota_info_page()
{
    flash_if_init();
    
    FLASH_EraseInitTypeDef desc;
    uint32_t result = FLASH_IF_OK;
    uint32_t page_error;

    HAL_FLASH_Unlock();

    desc.PageAddress = OTA_INFO_START_ADDR;
    desc.TypeErase = FLASH_TYPEERASE_PAGES;

    desc.NbPages = 1;
    if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
    {
        DEBUG_ERROR("OTA erase error\r\n");
        result = FLASH_IF_ERASE_KO;
    }

    HAL_FLASH_Lock();

    return result;
}

uint32_t flash_if_write_ota_info_page(uint32_t *data, uint32_t nb_of_word)
{
    flash_if_erase_ota_info_page();
    flash_if_write(OTA_INFO_START_ADDR, data, nb_of_word);
    return 0;
}


uint32_t flash_if_write(uint32_t destination, uint32_t *p_source, uint32_t length)
{
    uint32_t status = FLASH_IF_OK;
    uint32_t *p_actual = p_source; /* Temporary pointer to data that will be written in a half-page space */
    uint32_t i = 0;

    flash_if_init();

    HAL_FLASH_Unlock();

    while (p_actual < (uint32_t *)(p_source + length))
    {
        LL_IWDG_ReloadCounter(IWDG);
        /* Write the buffer to the memory */
        if (HAL_FLASHEx_HalfPageProgram(destination, p_actual) == HAL_OK) /* No error occurred while writing data in Flash memory */
        {
            /* Check if flash content matches memBuffer */
            for (i = 0; i < WORDS_IN_HALF_PAGE; i++)
            {
                if ((*(uint32_t *)(destination + 4 * i)) != p_actual[i])
                {
                    /* flash content doesn't match memBuffer */
                    DEBUG_ERROR("FLASH_IF_WRITING_CTRL_ERROR\r\n");
                    status = FLASH_IF_WRITING_CTRL_ERROR;
                    break;
                }
            }

            /* Increment the memory pointers */
            destination += FLASH_HALF_PAGE_SIZE;
            p_actual += WORDS_IN_HALF_PAGE;
        }
        else
        {
            DEBUG_ERROR("Writing error\r\n");
            status = FLASH_IF_WRITING_ERROR;
        }

        if (status != FLASH_IF_OK)
        {
            DEBUG_ERROR("Write flash error\r\n");
            break;
        }
    }

    HAL_FLASH_Lock();

    if (status != FLASH_IF_OK)
    {
        DEBUG_PRINTF("Flash write error %d\r\n", status);
    }

    return status;
}
