#include "app_flash.h"
#include <string.h>
#include "gd32e23x.h"

#define INFO_ADDR           (0x0800FC00)

void app_flash_get_mac_addr(uint8_t *mac)
{
    // Mac = 48 bit MSB of device unique id
    uint32_t id = *((uint32_t*)0x1FFFF7AC);
    memcpy(mac + 2, &id, 4);
    id = *((uint32_t*)0x1FFFF7B0);
    memcpy(mac, &id + 2, 2);
}

bool app_flash_infor_is_exist_in_flash(void)
{
    app_flash_network_info_t *data = (app_flash_network_info_t*)INFO_ADDR;
    
    if (data->flag == APP_FLASH_FLAG_WRITTEN)
    {
        return true;
    }
    
    return false;
}

void app_flash_read_info(app_flash_network_info_t *info)
{
    if (info)
    {
        memcpy(info, (app_flash_network_info_t*)INFO_ADDR, sizeof(app_flash_network_info_t));
    }
}

void app_flash_store_info(app_flash_network_info_t *info)
{
    if (info)
    {
        /* Step 1 Erase flash */
        uint32_t erase_counter;

        // Unlock the flash program/erase controller
        fmc_unlock();
        // Clear all pending flags
        fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
     
        // Erase the flash pages
        fmc_page_erase(INFO_ADDR);
        fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
        
        uint32_t address = INFO_ADDR;
        uint32_t *wr_data = (uint32_t*)info;
        
        for (uint32_t i = 0; i < (sizeof(app_flash_network_info_t)/sizeof(uint32_t)); i++)
        {
            fmc_word_program(address, wr_data[i]);
            address += 4U;
            fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
        }
    
        /* lock the main FMC after the erase operation */
        fmc_lock();
    }
}
