#include "ota_update.h"
#include "app_debug.h"
#include "app_md5.h"
#include "string.h"
#include "stm32l0xx_hal.h"
#include "flash_if.h"

#define OTA_MAX_SIZE (APPLICATION_SIZE)
#define MD5_LEN 16
#define OTA_REMAIN_MAX_LENGTH   (WORDS_IN_HALF_PAGE*4)

typedef struct
{
    uint8_t data[OTA_REMAIN_MAX_LENGTH];
    uint32_t size;
} __align(4) ota_bytes_remain_t;

static uint32_t m_expected_size = 0;
static bool m_found_header = false;
static uint32_t m_current_write_size = 0;
static bool verify_checksum(uint32_t begin_addr, uint32_t length);
static bool m_ota_is_running = false;
ota_bytes_remain_t m_ota_remain;
extern uint32_t flash_if_erase(uint32_t start);
extern uint32_t flash_if_write(uint32_t destination, uint32_t *p_source, uint32_t length);
volatile int32_t ota_update_timeout_ms = -1;

bool ota_update_is_running(void)
{
	return m_ota_is_running;
}

bool ota_update_start(uint32_t expected_size)
{
    m_expected_size = expected_size - OTA_UPDATE_DEFAULT_HEADER_SIZE;
    m_current_write_size = 0;
    memset(&m_ota_remain, 0, sizeof(m_ota_remain));
    DEBUG_PRINTF("Firmware size %u bytes\r\n", expected_size);
    if (expected_size > OTA_MAX_SIZE)
    {
        DEBUG_PRINTF("Firmware size too large %ubytes, max allowed %ubytes", expected_size, OTA_MAX_SIZE);
        return false;
    }
    uint32_t nb_of_page = (expected_size + FLASH_PAGE_SIZE-1) / FLASH_PAGE_SIZE;

    DEBUG_PRINTF("Erase %d pages, from addr 0x%08X\r\n", nb_of_page, DONWLOAD_START_ADDR);
	flash_if_init();
    
    flash_if_erase(DONWLOAD_START_ADDR);

    m_found_header = false;
	m_ota_is_running = true;
    ota_update_timeout_ms =  100000;
    return true;
}

bool ota_update_write_next(uint8_t *data, uint32_t length)
{
    // TODO write data to flash
    // ASSERT(length > 16)
	m_ota_is_running = true;
    if (m_found_header == false 
		&& m_current_write_size < (strlen(OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE) + strlen(OTA_UPDATE_DEFAULT_HEADER_DATA_HARDWARE)))
    {
		char *p = strstr((char*)data, OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE);
        if (!p)
        {
            DEBUG_PRINTF("Wrong firmware header\r\n");
            return false;
        }
		
		p += strlen(OTA_UPDATE_DEFAULT_HEADER_DATA_FIRMWARE);
		if (!strstr(p, OTA_UPDATE_DEFAULT_HEADER_DATA_HARDWARE))
        {
            DEBUG_PRINTF("Wrong firmware hardware\r\n");
            return false;
        }
		
        m_found_header = true;
        length -= OTA_UPDATE_DEFAULT_HEADER_SIZE;
        data += OTA_UPDATE_DEFAULT_HEADER_SIZE;
        DEBUG_PRINTF("Found header\r\n");
    }
    
    if (!m_found_header)
    {
        DEBUG_ERROR("Not found firmware header\r\n");
        return false;
    }
    
    while (length)
    {
        uint32_t bytes_need_copy = OTA_REMAIN_MAX_LENGTH - m_ota_remain.size;

        if (bytes_need_copy > length)
        {
            bytes_need_copy = length;
        }
        
        memcpy(&m_ota_remain.data[m_ota_remain.size], data, bytes_need_copy);
        length -= bytes_need_copy;
        data += bytes_need_copy;
        m_ota_remain.size += bytes_need_copy;
        
        if (m_ota_remain.size == OTA_REMAIN_MAX_LENGTH)
        {
            DEBUG_VERBOSE("Write %u bytes, at 0x%08X\r\n", m_ota_remain.size, DONWLOAD_START_ADDR + m_current_write_size);
            flash_if_write(DONWLOAD_START_ADDR + m_current_write_size, (uint32_t*)&m_ota_remain.data[0], m_ota_remain.size/sizeof(uint32_t));      
            m_current_write_size += m_ota_remain.size;
            m_ota_remain.size = 0;
        }
        else
        {
            break;
        }
    }
    DEBUG_PRINTF("Total write size %u\r\n", m_current_write_size);
    if (m_current_write_size >= m_expected_size)
    {
        DEBUG_INFO("All data received\r\n");
    }
    
    return true;
}

void ota_update_finish(bool status)
{
    m_found_header = false;
    if (status)
    {
        // TODO write boot information
        if (m_ota_remain.size)
        {
            DEBUG_PRINTF("Write final %u bytes, total %u bytes\r\n", m_ota_remain.size, m_current_write_size + m_ota_remain.size);
            flash_if_write(DONWLOAD_START_ADDR + m_current_write_size, (uint32_t*)&m_ota_remain.data[0], m_ota_remain.size/sizeof(uint32_t));   
            m_ota_remain.size = 0;
        }
    
        if (verify_checksum(DONWLOAD_START_ADDR, m_expected_size))
        {
            DEBUG_PRINTF("Valid checksum\r\n");
            ota_flash_cfg_t new_cfg;
            new_cfg.flag = OTA_FLAG_UPDATE_NEW_FW;
            new_cfg.firmware_size = m_expected_size;
            new_cfg.reserve[0] = 0;
            flash_if_write_ota_info_page((uint32_t*)&new_cfg, sizeof(ota_flash_cfg_t)/sizeof(uint32_t));
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
	
    m_current_write_size = 0;
    m_ota_remain.size = 0;
	m_ota_is_running = false;
    NVIC_SystemReset();
    while(1);
}

static bool verify_checksum(uint32_t begin_addr, uint32_t length)
{
    ///*
    // * @note: check md5 code in the very last 16 bytes at end of file
    // */
    app_md5_ctx md5_cxt;
    uint8_t md5_result[MD5_LEN];
    uint32_t checksum_addr;

    app_md5_init(&md5_cxt);
    app_md5_update(&md5_cxt, (uint8_t *)begin_addr, length - MD5_LEN);
    app_md5_final(md5_result, &md5_cxt);
    checksum_addr = begin_addr + length - MD5_LEN;
 
    if (memcmp(md5_result, (uint8_t *)checksum_addr, MD5_LEN) == 0)
    {
        return true;
    }
    else
    {
		for (uint32_t i = 0; i < MD5_LEN; i++)
		{
			DEBUG_RAW("%02X", md5_result[i]);
		}
        return false;
    }
}

ota_flash_cfg_t *ota_update_get_config(void)
{
    return (ota_flash_cfg_t*)OTA_INFO_START_ADDR;
}

