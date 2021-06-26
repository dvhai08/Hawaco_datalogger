#include "ota_update.h"
//#include "nrf.h"
//#include "nrf_nvmc.h"
#include "app_debug.h"
#include "app_md5.h"
#include "string.h"
#include "stm32l0xx_hal.h"

#define OTA_MAX_SIZE (80*1024)
#define MD5_LEN 16

static uint32_t m_expected_size = 0;
static bool m_found_header = false;
static uint32_t m_current_write_size = 0;
static bool verify_checksum(uint32_t begin_addr, uint32_t length);
static bool m_ota_is_running = false;

extern uint32_t FLASH_If_Erase(uint32_t start);
extern uint32_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t length);
extern void FLASH_If_Init(void);

bool ota_update_is_running(void)
{
	return m_ota_is_running;
}

bool ota_update_start(uint32_t expected_size)
{
    m_expected_size = expected_size - OTA_UPDATE_DEFAULT_HEADER_SIZE;
    m_current_write_size = 0;

    if (expected_size > OTA_MAX_SIZE)
    {
        DEBUG_PRINTF("Firmware size too large %ubytes, max allowed %ubytes", expected_size, OTA_MAX_SIZE);
        return false;
    }
    uint32_t nb_of_page = (expected_size + FLASH_PAGE_SIZE-1) / FLASH_PAGE_SIZE;

    DEBUG_PRINTF("Erase %d pages, from addr 0x%08X\r\n", nb_of_page, DONWLOAD_START_ADDR);
	FLASH_If_Erase(DONWLOAD_START_ADDR);

    m_found_header = false;
	m_ota_is_running = true;
    FLASH_If_Init();
    return true;
}

bool ota_update_write_next(uint32_t *data, uint32_t nb_of_word)
{
    // TODO write data to flash
    // ASSERT(length > 16)
	
	m_ota_is_running = true;
    if (m_found_header == false)
    {
        if (strcmp((char*)data, OTA_UPDATE_DEFAULT_HEADER_DATA))
        {
            DEBUG_PRINTF("Wrong firmware header\r\n");
            return false;
        }
        m_found_header = true;
        nb_of_word -= OTA_UPDATE_DEFAULT_HEADER_SIZE/sizeof(uint32_t);
        data += OTA_UPDATE_DEFAULT_HEADER_SIZE/sizeof(uint32_t);
        DEBUG_PRINTF("Found header\r\n");
    }

    if (m_found_header && ((m_current_write_size + nb_of_word*sizeof(uint32_t)) <= m_expected_size))
    {
		FLASH_If_Write(DONWLOAD_START_ADDR + m_current_write_size, data, nb_of_word);
        m_current_write_size += nb_of_word*sizeof(uint32_t);
    }
    else
    {
        return false;
    }
    
    return true;
}

void ota_update_finish(bool status)
{
    m_current_write_size = 0;
    m_found_header = false;

    if (status)
    {
        // TODO write boot information
        if (verify_checksum(DONWLOAD_START_ADDR, m_expected_size))
        {
            DEBUG_PRINTF("Valid checksum\r\n");
        }
        else
        {
            DEBUG_PRINTF("Invalid checksum\r\n");
        }
    }
    else
    {
        DEBUG_PRINTF("OTA update failed\r\n");
    }
	
	m_ota_is_running = false;
    NVIC_SystemReset();
}

static bool verify_checksum(uint32_t begin_addr, uint32_t length)
{
    ///*
    // * @note: check md5 code in the very last 16 bytes at end of file
    // */
    app_md5_ctx md5_cxt;
    uint8_t md5_compare[MD5_LEN];
    uint32_t checksum_addr;

    app_md5_init(&md5_cxt);
    app_md5_update(&md5_cxt, (uint8_t *)begin_addr, length - MD5_LEN);
    app_md5_final(md5_compare, &md5_cxt);
    checksum_addr = begin_addr + length - MD5_LEN;
 
    if (memcmp(md5_compare, (uint8_t *)checksum_addr, MD5_LEN) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}
