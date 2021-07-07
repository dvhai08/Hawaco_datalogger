#include "flash_if.h"
#include "main.h"
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

uint32_t flash_if_erase(uint32_t start, uint32_t end)
{
    flash_if_init();

    FLASH_EraseInitTypeDef desc;
    uint32_t result = FLASH_IF_OK;
    uint32_t page_error;

    HAL_FLASH_Unlock();

    desc.PageAddress = start;
    desc.TypeErase = FLASH_TYPEERASE_PAGES;
    __disable_irq();
    /* NOTE: Following implementation expects the IAP code address to be < Application address */
    if (start < FLASH_START_BANK2)
    {
        if (end >= FLASH_START_BANK2)
        {
            desc.NbPages = (FLASH_START_BANK2 - start) / FLASH_PAGE_SIZE;
            if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
            {
                result = FLASH_IF_ERASE_KO;
            }
        }
        else
        {
            desc.NbPages = (end - start) / FLASH_PAGE_SIZE;
            if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
            {
                result = FLASH_IF_ERASE_KO;
            }
        }
    }

    if (result == FLASH_IF_OK)
    {
        if (end >= FLASH_START_BANK2)
        {
            desc.PageAddress = ABS_RETURN(start, FLASH_START_BANK2);
            desc.NbPages = (end - desc.PageAddress) / FLASH_PAGE_SIZE;
            if (HAL_FLASHEx_Erase(&desc, &page_error) != HAL_OK)
            {
                result = FLASH_IF_ERASE_KO;
            }
        }
    }

    HAL_FLASH_Lock();
    __enable_irq();
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

    flash_if_init();

    HAL_FLASH_Unlock();
    
    for (uint32_t i = 0; i < length; i++)
    {
        LL_IWDG_ReloadCounter(IWDG);
        /* Write the buffer to the memory */
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, p_source[i]) == HAL_OK) /* No error occurred while writing data in Flash memory */
        {
//            /* Check if flash content matches memBuffer */
//            for (i = 0; i < 1; i++)
//            {
//                if ((*(uint32_t *)(destination + 4 * i)) != p_source[i])
//                {
//                    /* flash content doesn't match memBuffer */
//                    status = FLASH_IF_WRITING_CTRL_ERROR;
//                    break;
//                }
//            }

            /* Increment the memory pointers */
            destination += 4;
        }
        else
        {
            status = FLASH_IF_WRITING_ERROR;
        }

        if (status != FLASH_IF_OK)
        {
            break;
        }
    }

    HAL_FLASH_Lock();

//    if (status != FLASH_IF_OK)
//    {
//        DEBUG_PRINTF("Flash write error %d\r\n", status);
//    }

    return status;
}
