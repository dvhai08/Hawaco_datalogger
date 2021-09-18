#include "app_spi_flash.h"
#include "app_debug.h"
#include <string.h>
#include "spi.h"
#include "main.h"
#include "umm_malloc.h"
#include "sys_ctx.h"
#include "utilities.h"
#include "hardware.h"
#include "main.h"
#include "gsm.h"
#include "usart.h"
#include "app_eeprom.h"
#include "version_control.h"

#define VERIFY_FLASH 0
#define DEBUG_FLASH 0

#define FL164K 1       // 8 MB
#define FL127S 2       // 16 MB
#define FL256S 3       // 32 MB
#define GD256C 4       // 32MB
#define W25Q256JV 5    // 32MB
#define W25Q80DLZPIG 6 // 8Mbit
#define W25Q128 7      // 8Mbit
#define W25Q32 8       // 32Mbit

#define READ_DATA_CMD 0x03
#define FAST_READ_DATA_CMD 0x0B
#define RDID_CMD 0x9F
#define READ_ID_CMD 0x90

#define WREN_CMD 0x06
#define WRDI_CMD 0x04

#define SE_CMD 0x20  //Sector Erase - 4KB
#define BE_CMD 0xD8  //Block Erase - 64KB
#define CE_CMD 0xC7  //0x60 Chip Erase
#define WRR_CMD 0x01 //Ghi vao thanh ghi trang thai
#define PP_CMD 0x02
#define RDSR_CMD 0x05 /* Read status register */
#define CLSR_CMD 0x30 /* Dua Flash ve trang thai mac dinh */

#define SPI_DUMMY 0x00

/* Dinh nghia cac lenh cho dia chi do rong 4-byte */
#define READ_DATA_CMD4 0x13      //Read
#define FAST_READ_DATA_CMD4 0x0C //FastRead
#define PP_CMD4 0x12             //Page Program
#define SE_CMD4 0x21             //Sector Erase - 4KB - 32bit addrress
#define BE_CMD4 0xDC             //Block Erase - 64KB - 32bits address

/* Mot so lenh danh rieng cho Flash GigaDevice */
#define EN4B_MODE_CMD 0xB7 //Vao che do 32bits dia chi
#define EX4B_MODE_CMD 0xE9 //Thoat che do 32bits dia chi
#define RDSR1_CMD 0x05     //Read status register 1
#define RDSR2_CMD 0x35     //Read status register 2
#define RDSR3_CMD 0x15     //Read status register 3

#define WRSR1_CMD 0x01 //Write status register 1
#define WRSR2_CMD 0x31 //Write status register 2
#define WRSR3_CMD 0x11 //Write status register 3

/* Winbond */
#define WB_RESET_STEP0_CMD 0x66
#define WB_RESET_STEP1_CMD 0x99
#define WB_POWER_DOWN_CMD 0xB9
#define WB_WAKEUP_CMD 0xAB

#define FLASH_CHECK_FIRST_RUN (uint32_t)0x0000

#define SPI_FLASH_PAGE_SIZE 256
#define SPI_FLASH_SECTOR_SIZE 4096
#define SPI_FLASH_MAX_SECTOR (APP_SPI_FLASH_SIZE / SPI_FLASH_SECTOR_SIZE)
#define SPI_FLASH_SHUTDOWN_ENABLE   1
#define HAL_SPI_Initialize() while (0)

static uint8_t flash_self_test(void);
static uint8_t flash_check_first_run(void);
uint8_t flash_test(void);
void flash_read_bytes(uint32_t addr, uint8_t *buffer, uint16_t length);
void flash_erase_sector_4k(uint32_t sector_count);
void flash_write_bytes(uint32_t addr, uint8_t *buffer, uint32_t length);

static uint8_t m_flash_version;
static bool m_flash_is_good = false;
uint32_t m_resend_data_in_flash_addr = SPI_FLASH_PAGE_SIZE; // sector 0
static uint32_t m_wr_addr = SPI_FLASH_PAGE_SIZE;            // Current write addr,= sector 0
uint32_t m_last_write_data_addr = SPI_FLASH_PAGE_SIZE;		// Last write addr


void app_spi_flash_initialize(void)
{
    sys_ctx_t *ctx = sys_ctx();
    ctx->peripheral_running.name.flash_running = 1;
    
    uint16_t flash_test_status = 0;
    //	HAL_SPI_Initialize();
    app_spi_flash_wakeup();
    if (flash_self_test())
    {
        m_flash_is_good = true;
        DEBUG_VERBOSE("Flash self test[OK]\r\nFlash type: ");
        switch (m_flash_version)
        {
        case FL164K: //8MB
            DEBUG_RAW("FL164K. ");
            break;
        case FL127S: //16MB
            DEBUG_RAW("FL127S. ");
            break;
        case FL256S: //32MB
            DEBUG_RAW("FL256S. ");
            break;
        case GD256C: //32MB
            DEBUG_RAW("GD256C. ");
            break;
        case W25Q256JV: //32MB
            DEBUG_RAW("W25Q256JV. ");
            break;
        case W25Q80DLZPIG:
            DEBUG_RAW("W25Q80DLZPIG. ");
            break;
        case W25Q128:
            DEBUG_RAW("W25Q128. ");
            break;
        case W25Q32:
            DEBUG_RAW("W25Q32FV");
            break;
        default:
            DEBUG_RAW("UNKNOWNN: %u", m_flash_version);
            m_flash_is_good = false;
            break;
        }
        DEBUG_RAW("\r\n");
    }
    else
    {
        m_flash_is_good = false;
        DEBUG_ERROR("Flash init failed\r\n");
        return;
    }

    if (!flash_check_first_run())
    {
        flash_test_status = flash_test();

        DEBUG_INFO("Test R/W Flash: ");
        if (flash_test_status)
        {
            DEBUG_RAW("[FAIL], N = %u\r\n", flash_test_status);
            m_flash_is_good = false;
        }
        else
        {
            DEBUG_RAW("[OK]\r\n");
            m_flash_is_good = true;
        }
    }
    else
    {
//        DEBUG_VERBOSE("Check first run ok\r\n");
    }

    if (m_flash_is_good)
    {
        bool flash_status;
        uint32_t addr = app_flash_estimate_next_write_addr(&flash_status);
		// If flash full =>> erase first block and set write addr = first_page
		if (flash_status)
		{
			m_wr_addr = SPI_FLASH_PAGE_SIZE;
			addr = m_wr_addr;
			DEBUG_WARN("Flash full, erase first block\r\n");
			flash_erase_sector_4k(0);
			uint8_t tmp_buff[1] = {0xA5};
			flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
		}
	
        DEBUG_INFO("Estimate write addr 0x%08X\r\n", addr);
        
        app_spi_flash_estimate_current_read_addr(&flash_status, true);
        if (flash_status)
        {
            DEBUG_INFO("Retransmission data in flash at addr 0x%08X\r\n", m_resend_data_in_flash_addr);
        }
    }
}

bool app_spi_flash_is_ok(void)
{
    return m_flash_is_good;
}

static void flash_write_control(uint8_t enable, uint32_t addr)
{
    SPI_EXT_FLASH_CS(0);
    if (enable)
    {
        spi_flash_transmit(WREN_CMD);
    }
    else
    {
        spi_flash_transmit(WRDI_CMD);
    }
    SPI_EXT_FLASH_CS(1);
}

static void wait_write_in_process(uint32_t addr)
{
    uint32_t timeout = 160000; //Adjust depend on system clock
    uint8_t status = 0;
    uint8_t cmd;

    /* Read status register */
    SPI_EXT_FLASH_CS(0);
    spi_flash_transmit(RDSR_CMD);

    while (timeout)
    {
        timeout--;
        cmd = SPI_DUMMY;
        spi_flash_transmit_receive(&cmd, &status, 1);

        if ((status & 1) == 0)
            break;
    }

    if (timeout == 0)
    {
        DEBUG_ERROR("[%s-%d] error\r\n", __FUNCTION__, __LINE__);
    }
    SPI_EXT_FLASH_CS(1);
}

static void flash_write_page(uint32_t addr, uint8_t *buffer, uint16_t length)
{
//    DEBUG_VERBOSE("Flash write page addr 0x%08X, size %u\r\n", addr, length);
    uint32_t i = 0;
    uint32_t old_addr = addr;

    flash_write_control(1, addr);
    SPI_EXT_FLASH_CS(0);

    uint8_t cmd;
    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Send write cmd */
        cmd = PP_CMD4;
        spi_flash_transmit(cmd);

        //        /* Gui 4 byte dia chi */
        cmd = (addr >> 24) & 0xFF;
        spi_flash_transmit(cmd);
    }
    else
    {
        /* Send write cmd */
        cmd = PP_CMD;
        spi_flash_transmit(cmd);
    }

    /* Send 3 bytes address */
    cmd = (addr >> 16) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = (addr >> 8) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = addr & 0xFF;
    spi_flash_transmit(cmd);

    /* Send data to flash */
    for (i = 0; i < length; i++)
    {
        spi_flash_transmit(buffer[i]);
    }
    SPI_EXT_FLASH_CS(1);
    sys_delay_ms(1);
    wait_write_in_process(old_addr);

#if VERIFY_FLASH
    bool found_error = false;
    for (i = 0; i < length; i++) // Debug only
    {
        uint8_t tmp;
        flash_read_bytes(old_addr + i, (uint8_t *)&tmp, 1);
        if (memcmp(&tmp, buffer + i, 1))
        {
            found_error = true;
            DEBUG_ERROR("Flash write error at addr 0x%08X, readback 0x%02X, expect 0x%02X\r\n", old_addr + i, tmp, *(buffer + i));
            break;
        }
        else
        {
            //            DEBUG_ERROR("Flash write success at addr 0x%08X, readback 0x%02X, expect 0x%02X\r\n", old_addr + i, tmp, *(buffer + i));
        }
    }
    if (found_error == false)
    {
        DEBUG_INFO("Page write success\r\n");
    }
//    else
//    {
//        uint32_t remain = SPI_FLASH_PAGE_SIZE - i - 1;
//        i++;
//        for (uint32_t j = 0; j < remain; j++) // Debug only
//        {
//            uint8_t tmp;
//            flash_read_bytes(addr + i + j, (uint8_t *)&tmp, 1);
//            if (tmp != 0xFF)
//            {
//                DEBUG_PRINTF("Page not empty\r\n");
//                break;
//            }
//        }
//    }
#endif
}

/*****************************************************************************/
/*!
 * @brief	:  PageSize = 256 (Flash 8,16,32MB); 512 (Flash >= 64MB)
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void flash_write_bytes(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    /* Split data into page size (256) */
    uint32_t offset_addr = 0;
    uint32_t length_need_to_write = 0;
    uint32_t nb_bytes_written = 0;
#if DEBUG_FLASH
    DEBUG_INFO("Flash write %u bytes, from addr 0x%08X\r\n", length, addr);
#endif

    while (length)
    {
        offset_addr = addr % SPI_FLASH_PAGE_SIZE;

        if (offset_addr > 0)
        {
            if (offset_addr + length > SPI_FLASH_PAGE_SIZE)
            {
                length_need_to_write = SPI_FLASH_PAGE_SIZE - offset_addr;
            }
            else
            {
                length_need_to_write = length;
            }
        }
        else
        {
            if (length > SPI_FLASH_PAGE_SIZE)
            {
                length_need_to_write = SPI_FLASH_PAGE_SIZE;
            }
            else
            {
                length_need_to_write = length;
            }
        }

        length -= length_need_to_write;

        flash_write_page(addr, &buffer[nb_bytes_written], length_need_to_write);

        nb_bytes_written += length_need_to_write;

        addr += length_need_to_write;
    }
}

void flash_read_bytes(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    SPI_EXT_FLASH_CS(0);
    uint8_t cmd;
    uint32_t next_sector_offset = 0;
    if (addr + length > APP_SPI_FLASH_SIZE)
    {
        next_sector_offset = (addr + length) - APP_SPI_FLASH_SIZE + SPI_FLASH_PAGE_SIZE; // first page size is reserve for initialization process
        length = APP_SPI_FLASH_SIZE - addr;
    }
    while (1)
    {
        if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
        {
            /* Send read cmd*/
            cmd = READ_DATA_CMD4;
            spi_flash_transmit(cmd);
            cmd = (addr >> 24) & 0xFF;
            spi_flash_transmit(cmd);
        }
        else
        {
            /* Send read cmd*/
            cmd = READ_DATA_CMD;
            spi_flash_transmit(cmd);
        }

        /* Send 3 bytes address */
        cmd = (addr >> 16) & 0xFF;
        spi_flash_transmit(cmd);

        cmd = (addr >> 8) & 0xFF;
        spi_flash_transmit(cmd);

        cmd = addr & 0xFF;
        spi_flash_transmit(cmd);

        spi_flash_receive(buffer, length);

        if (next_sector_offset == 0)
        {
            break;
        }
        else
        {
            buffer += length;
            addr = next_sector_offset;
            next_sector_offset = 0;
            continue;
        }
    }
    SPI_EXT_FLASH_CS(1);
}

void flash_erase_sector_4k(uint32_t sector_count)
{
#if DEBUG_FLASH
    DEBUG_INFO("Erase sector[%u] 4k\r\n", sector_count);
#endif
    uint32_t addr = 0;
    uint32_t old_addr = 0;
    uint8_t cmd;
    addr = sector_count * SPI_FLASH_SECTOR_SIZE; //Sector 4KB
    old_addr = addr;

    flash_write_control(1, addr);
    sys_delay_ms(2);

    SPI_EXT_FLASH_CS(0);
    sys_delay_ms(1);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh */
        cmd = SE_CMD4;
        //        FLASH_SPI_SendByte(SE_CMD4);
        spi_flash_transmit(cmd);

        /* Send 4 bytes address */
        cmd = (addr >> 24) & 0xFF;
        spi_flash_transmit(cmd);
    }
    else
    {
        /* Gui lenh */
        cmd = SE_CMD;
        //        FLASH_SPI_SendByte(SE_CMD);
        spi_flash_transmit(cmd);
    }

    /* Gui 3 bytes dia chi */
    cmd = (addr >> 16) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = (addr >> 8) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = addr & 0xFF;
    spi_flash_transmit(cmd);

    SPI_EXT_FLASH_CS(1);
    sys_delay_ms(2);
    wait_write_in_process(old_addr);
#if VERIFY_FLASH
    if (app_spi_flash_check_empty_sector(sector_count))
    {
        DEBUG_INFO("Erase sector %u success\r\n", sector_count);
    }
    else
    {
        DEBUG_INFO("Erase sector %u error\r\n", sector_count);
    }
#endif
}

void flash_erase_block_64K(uint16_t sector_count)
{
    uint32_t addr = 0;
    uint32_t old_addr = 0;
    uint8_t cmd;

    addr = sector_count * 65536; //Sector 64KB
    old_addr = addr;

    flash_write_control(1, addr);

    SPI_EXT_FLASH_CS(0);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        cmd = BE_CMD4;
        spi_flash_transmit(cmd);

        /* Send 4 bytes address */
        cmd = (addr >> 24) & 0xFF;
        spi_flash_transmit(cmd);
    }
    else
    {
        cmd = BE_CMD;
        spi_flash_transmit(cmd);
    }

    /* Send 3 bytes address */
    cmd = (addr >> 16) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = (addr >> 8) & 0xFF;
    spi_flash_transmit(cmd);

    cmd = addr & 0xFF;
    spi_flash_transmit(cmd);

    SPI_EXT_FLASH_CS(1);
    wait_write_in_process(old_addr);
}

uint8_t flash_self_test(void)
{
    uint8_t manufacture_id = 0xFF;
    uint8_t device_id = 0xFF;
    uint8_t reg_status = 0;
    uint8_t loop_count;
    uint8_t cmd;
    //Retry init 3 times
    for (loop_count = 0; loop_count < 3; loop_count++)
    {
        SPI_EXT_FLASH_CS(0);
        cmd = READ_ID_CMD;
        spi_flash_transmit(cmd);

        /* 3 byte address */
        cmd = 0;
        spi_flash_transmit(0x00);
        spi_flash_transmit(0x00);
        spi_flash_transmit(0x00);

        cmd = 0xFF;
        spi_flash_transmit_receive(&cmd, &manufacture_id, 1);
        spi_flash_transmit_receive(&cmd, &device_id, 1);

        SPI_EXT_FLASH_CS(1);

        DEBUG_INFO("device id: 0x%02X, manufacture id: 0x%02X\r\n", device_id, manufacture_id);
        if (manufacture_id == 0x01 && device_id == 0x16) /* FL164K - 64Mb */
            m_flash_version = FL164K;
        else if (manufacture_id == 0x01 && device_id == 0x17) /* FL127S - 1 chan CS 128Mb */
            m_flash_version = FL127S;
        else if (manufacture_id == 0x01 && device_id == 0x18) /* FL256S - 1 chan CS 256Mb */
            m_flash_version = FL256S;
        else if (manufacture_id == 0xEF && device_id == 0x17) /* FL256S - 1 chan CS 256Mb */
        {
            DEBUG_INFO("W25Q128\r\n");
            m_flash_version = W25Q128;
        }
        else if (manufacture_id == 0xEF && device_id == 0x15)
        {
            DEBUG_INFO("W25Q32FV\r\n");
            m_flash_version = W25Q32;
        }
        else if (manufacture_id == 0xC8 && device_id == 0x18) /* GD256C - GigaDevice 256Mb */
        {
            m_flash_version = GD256C;

            //Vao che do 4-Byte addr
            SPI_EXT_FLASH_CS(0);
            cmd = EN4B_MODE_CMD;
            spi_flash_transmit(cmd);
            SPI_EXT_FLASH_CS(1);

            sys_delay_ms(10);
            //Doc thanh ghi status 2, bit ADS - 5
            SPI_EXT_FLASH_CS(0);
            cmd = RDSR2_CMD;
            spi_flash_transmit(cmd);

            cmd = SPI_DUMMY;
            spi_flash_transmit_receive(&cmd, &reg_status, 1);
            SPI_EXT_FLASH_CS(1);

            DEBUG_INFO("status register: %02X\r\n", reg_status);
            if (reg_status & 0x20)
            {
                DEBUG_INFO("Address mode : 32 bit\r\n");
            }
            else
            {
                sys_delay_ms(500);
                continue;
            }
        }
        else if (manufacture_id == 0xEF) /* W25Q256JV - Winbond 256Mb */
        {
            if (device_id == 0x18)
            {
                DEBUG_INFO("Winbond W25Q256JV\r\n");
                m_flash_version = W25Q256JV;
                //Vao che do 4-Byte addr
                SPI_EXT_FLASH_CS(0);
                cmd = EN4B_MODE_CMD;
                spi_flash_transmit(cmd);
                SPI_EXT_FLASH_CS(1);

                sys_delay_ms(10);
                //Doc thanh ghi status 3, bit ADS  (S16) - bit 0
                SPI_EXT_FLASH_CS(0);
                cmd = RDSR3_CMD;
                spi_flash_transmit(cmd);

                cmd = SPI_DUMMY;
                spi_flash_transmit_receive(&cmd, &reg_status, 1);

                DEBUG_INFO("status register: %02X\r\n", reg_status);
                if (reg_status & 0x01)
                {
                    DEBUG_INFO("Address mode : 32 bit\r\n");
                }
                SPI_EXT_FLASH_CS(1);
            }
            else if (device_id == 0x13)
            {
                DEBUG_INFO("Winbond W25Q80DLZPIG\r\n");
                m_flash_version = W25Q80DLZPIG;
            }
        }
        else
        {
            DEBUG_ERROR("Unknown device\r\n");
            return 0;
        }

        return 1;
    }
    return 0;
}

uint8_t flash_test(void)
{
    uint16_t flash_read_status = 0, i;
    uint8_t buffer_test[10];

    memset(buffer_test, 0xAA, sizeof(buffer_test));

    /* Ghi vao Flash 10 bytes */
    flash_write_bytes(FLASH_CHECK_FIRST_RUN + 15, buffer_test, 10);

    /* Doc lai va kiem tra */
    memset(buffer_test, 0, 10);
    flash_read_bytes(FLASH_CHECK_FIRST_RUN + 15, buffer_test, 10);

    for (i = 0; i < 10; i++)
    {
        if (buffer_test[i] != 0xAA)
        {
            flash_read_status++;
        }
    }
    return flash_read_status;
}

void app_spi_flash_erase_all(void)
{
    uint8_t status = 0xFF;
    uint8_t cmd;
    DEBUG_INFO("Erase all flash\r\n");
	
	app_spi_flash_wakeup();

    flash_write_control(1, 0);
    SPI_EXT_FLASH_CS(0);

    cmd = CE_CMD;
    spi_flash_transmit(cmd);
    SPI_EXT_FLASH_CS(1);

    /* Doc thanh ghi status */
    sys_delay_ms(100);
    SPI_EXT_FLASH_CS(0);
    cmd = RDSR_CMD;
    spi_flash_transmit(cmd);

    uint32_t begin_tick = sys_get_ms();
    while (1)
    {
        cmd = SPI_DUMMY;
        spi_flash_transmit_receive(&cmd, &status, 1);
        if ((status & 1) == 0)
            break;
        sys_delay_ms(1000);
    }
    SPI_EXT_FLASH_CS(1);

#if 0
//#if VERIFY_FLASH
    bool found_error = false;
    uint32_t old_addr = 0;
    uint8_t buffer[1] = {0xFF};
    for (uint32_t i = 0; i < APP_SPI_FLASH_SIZE; i++)      // Debug only
    {
        uint8_t tmp;
        flash_read_bytes(old_addr + i, (uint8_t*)&tmp, 1);
        if (memcmp(&tmp, buffer, 1))
        {
            found_error = true;
            DEBUG_ERROR("Flash write error at addr 0x%08X, readback 0x%02X, expect 0xFF\r\n", old_addr + i, tmp);
            break;
        }
    }
    if (found_error == false)
    {
        DEBUG_INFO("Erase success\r\n");
    }
#endif

    DEBUG_INFO("Erase [DONE] in %ums\r\n", sys_get_ms() - begin_tick);
}

static uint8_t flash_check_first_run(void)
{
    uint8_t tmp = 0;
    uint8_t tmp_buff[2];

    flash_read_bytes(FLASH_CHECK_FIRST_RUN, &tmp, 1);

    if (tmp != 0xA5)
    {
        DEBUG_INFO("Erase flash\r\n");
        app_spi_flash_erase_all();

        tmp_buff[0] = 0xA5;
        flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
        return 0;
    }
    else
    {
//        DEBUG_VERBOSE("Check Byte : 0x%X\r\n", tmp);
    }

    return 1;
}

uint32_t app_flash_estimate_next_write_addr(bool *flash_full)
{
    *flash_full = false;
    bool cont = true;
    while (cont)
    {
        uint32_t size_struct = sizeof(app_spi_flash_data_t);
        app_spi_flash_data_t *tmp = umm_calloc(1, size_struct);
        if (tmp == NULL)
        {
            NVIC_SystemReset();
        }
        // DEBUG_INFO("Current write addr 0x%08X\r\n", m_wr_addr);
        flash_read_bytes(m_wr_addr, (uint8_t *)tmp, size_struct);
        if (tmp->valid_flag == APP_FLASH_VALID_DATA_KEY)
        {
            m_last_write_data_addr = m_wr_addr;
            m_wr_addr += size_struct;
            if (m_wr_addr >= (APP_SPI_FLASH_SIZE - 2 * size_struct))
            {
                *flash_full = true;
                cont = false;
            }
        }
        else
        {
            app_spi_flash_data_t *empty = umm_calloc(1, size_struct);
            if (empty == NULL)
            {
                NVIC_SystemReset();
            }
            memset(empty, 0xFF, size_struct);
            if (memcmp(tmp, empty, size_struct) == 0)
            {
                cont = false;
            }
            else    // not empty page, erase next page
            {
                m_last_write_data_addr = m_wr_addr;
                uint32_t current_page = m_last_write_data_addr/SPI_FLASH_SECTOR_SIZE;
                uint32_t next_page;
                m_wr_addr += size_struct;
                if (m_wr_addr >= (APP_SPI_FLASH_SIZE - 2 * size_struct))
                {
                    next_page = 0;      // Erase page 0
                    *flash_full = true;
                    cont = false;
                }
                else
                {
                    next_page = m_wr_addr / SPI_FLASH_SECTOR_SIZE;
                    if (next_page != current_page)
                    {
                        DEBUG_ERROR("Erase sector 4K %u\r\n", next_page);
                        flash_erase_sector_4k(next_page);
                        cont = false;
                    }
                }
            }
            umm_free(empty);
        }
        umm_free(tmp);
    }
    
    DEBUG_INFO("Write addr 0x%08X\r\n", m_wr_addr);
    return m_wr_addr;
}

uint32_t find_retransmission_message(uint32_t begin_addr, uint32_t end_addr)
{
    uint32_t size_struct = sizeof(app_spi_flash_data_t);
    app_spi_flash_data_t *tmp = umm_calloc(1, size_struct);
    if (tmp == NULL)
    {
        NVIC_SystemReset();
    };

    while (1)
    {
        flash_read_bytes(begin_addr, (uint8_t *)tmp, size_struct);
        if (tmp->valid_flag == APP_FLASH_VALID_DATA_KEY)
        {
            uint32_t crc = utilities_calculate_crc32((uint8_t *)tmp, size_struct - CRC32_SIZE);		// 4 mean size of CRC32
            if (tmp->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG
                && tmp->crc == crc)
            {
                umm_free(tmp);
                return begin_addr;
            }
            else
            {
                begin_addr += size_struct;
            }
        }
        else
        {
            begin_addr += size_struct;
        }

        if (begin_addr >= end_addr) // No more error found
        {
            umm_free(tmp);
            return 0;
        }
    }
}


uint32_t app_spi_flash_estimate_current_read_addr(bool *found_error, bool scan_all_flash)
{
    uint32_t tmp_addr;
    uint32_t size_struct = sizeof(app_spi_flash_data_t);
    app_spi_flash_data_t *tmp = umm_calloc(1, size_struct);
    if (tmp == NULL)
    {
        NVIC_SystemReset();
    };
    
    *found_error = false;
    DEBUG_INFO("Resend addr 0x%08X, write addr 0x%08X\r\n", m_resend_data_in_flash_addr, m_wr_addr);
    if (m_resend_data_in_flash_addr == m_wr_addr) // Neu read = write =>> check current page status
    {
        flash_read_bytes(m_wr_addr, (uint8_t *)tmp, size_struct);
		
		// Neu packet error
		uint32_t crc = utilities_calculate_crc32((uint8_t *)tmp, size_struct - CRC32_SIZE);		// 4 mean size of CRC32
        if (tmp->valid_flag == APP_FLASH_VALID_DATA_KEY 
            && (tmp->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG)
			&& tmp->crc == crc)
        {
            m_resend_data_in_flash_addr = m_wr_addr;
            *found_error = true;
        }
    }
    else if (m_wr_addr > m_resend_data_in_flash_addr) // Neu write address > read address =>> Scan from read_addr to write_addr
    {
		// Find any message which need to send to server
		tmp_addr = find_retransmission_message(m_resend_data_in_flash_addr, m_wr_addr);
		if (tmp_addr != 0)
		{
			m_resend_data_in_flash_addr = tmp_addr;
			*found_error = true;
		}
		else if (scan_all_flash)
		{
			// If we didnot seen error message from m_resend_data_in_flash_addr to m_wr_addr
			// Scan once more 2 sector
			// =>> if scan data from 0 -> write pointer 
			// -> we not found anythings
			// Use full when flash overflow
            uint32_t next_addr = m_wr_addr + 2*SPI_FLASH_SECTOR_SIZE;
            if (next_addr > APP_SPI_FLASH_SIZE)
            {
                next_addr = APP_SPI_FLASH_SIZE;
            }
			tmp_addr = find_retransmission_message(m_wr_addr, next_addr);
			if (tmp_addr != 0)
			{
				DEBUG_INFO("Found flash overflow\r\n");
				m_resend_data_in_flash_addr = tmp_addr;
				*found_error = true;
			}
		}
    }
    else // Write addr < read addr =>> buffer full =>> Scan 2 step
         // Step 1 : scan from read addr to max addr
         // Step 2 : scan from SPI_FLASH_PAGE_SIZE to write addr
    {
        tmp_addr = find_retransmission_message(m_resend_data_in_flash_addr, m_wr_addr);
        if (tmp_addr != 0)
        {
            m_resend_data_in_flash_addr = tmp_addr;
            *found_error = true;
        }
        else
        {
            tmp_addr = find_retransmission_message(SPI_FLASH_PAGE_SIZE, m_wr_addr);
            if (tmp_addr != 0)
            {
                *found_error = true;
                m_resend_data_in_flash_addr = tmp_addr;
            }
            else
            {
                m_resend_data_in_flash_addr = m_wr_addr;
            }
        }
    }
    
    umm_free(tmp);
    return m_resend_data_in_flash_addr;
}

uint32_t app_spi_flash_dump_all_data(void)
{
    uint8_t read[5];
    flash_read_bytes(0, read, 4);
    read[4] = 0;
//    DEBUG_VERBOSE("Read data %s\r\n", (char *)read);
    return 0;
}

void app_spi_flash_wakeup(void)
{
#if SPI_FLASH_SHUTDOWN_ENABLE
    for (uint8_t i = 0; i < 3; i++)
    {
        SPI_EXT_FLASH_CS(0);
        uint8_t cmd = WB_WAKEUP_CMD;
        spi_flash_transmit(cmd);
        SPI_EXT_FLASH_CS(1);
        for (uint32_t i = 0; i < 16 * 3; i++) // 3us
        {
            __nop();
        }
    }
#endif
    sys_ctx_t *ctx = sys_ctx();
    ctx->peripheral_running.name.flash_running = 1;
}

void app_spi_flash_shutdown(void)
{
#if SPI_FLASH_SHUTDOWN_ENABLE
    for (uint8_t i = 0; i < 1; i++)
    {
        SPI_EXT_FLASH_CS(0);
        uint8_t cmd = WB_POWER_DOWN_CMD;
        spi_flash_transmit(cmd);
        SPI_EXT_FLASH_CS(1);
    }
#endif
}

bool app_spi_flash_check_empty_sector(uint32_t sector)
{
    bool retval = true;
    uint32_t addr = sector * SPI_FLASH_SECTOR_SIZE;
    for (uint32_t i = 0; i < SPI_FLASH_SECTOR_SIZE;) // Debug only
    {
        uint32_t tmp;
        flash_read_bytes(addr + i, (uint8_t *)&tmp, 4);
        if (tmp != 0xFFFFFFFF)
        {
            retval = false;
            break;
        }
        i += 4;
    }
    if (retval)
    {
//        DEBUG_VERBOSE("We need erase next sector %u\r\n", sector);
    }
    return retval;
}



void app_spi_flash_write_data(app_spi_flash_data_t *wr_data)
{
    DEBUG_INFO("Flash write new data\r\n");
    if (!m_flash_is_good)
    {
//        DEBUG_ERROR("Flash init error, ignore write msg\r\n");
        return;
    }
    app_spi_flash_data_t rd_data;
//    wr_data->header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
//    while (1)
    {
        bool flash_full;
        uint32_t addr = app_flash_estimate_next_write_addr(&flash_full);

		
		// Flash full, erase first block and set write_addr = first page
		if (flash_full)
		{
			m_wr_addr = SPI_FLASH_PAGE_SIZE;
			addr = m_wr_addr;
			DEBUG_WARN("Flash full, erase first block\r\n");
			flash_erase_sector_4k(0);
			uint8_t tmp_buff[1] = {0xA5};
			flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
		}
		
        // Check the next write address overlap to the next sector
        // =>> Erase next sector if needed
        uint32_t expect_write_sector = addr/SPI_FLASH_SECTOR_SIZE;
        uint32_t expect_next_sector = (addr+sizeof(app_spi_flash_data_t))/SPI_FLASH_SECTOR_SIZE;
        if (expect_next_sector != expect_write_sector)
        {
            DEBUG_INFO("Consider erase next sector\r\n");
            // Consider write next page
            if (!app_spi_flash_check_empty_sector(expect_next_sector))
            {
                flash_erase_sector_4k(expect_next_sector);
            }
            else
            {
                DEBUG_INFO("We dont need to erase next sector\r\n");
            }
        }
        
		wr_data->crc = utilities_calculate_crc32((uint8_t *)wr_data, sizeof(app_spi_flash_data_t) - CRC32_SIZE);		// 4 mean size of CRC32
        flash_write_bytes(addr, (uint8_t *)wr_data, sizeof(app_spi_flash_data_t));
        flash_read_bytes(addr, (uint8_t *)&rd_data, sizeof(app_spi_flash_data_t));
        if (memcmp(wr_data, &rd_data, sizeof(app_spi_flash_data_t)))
        {
            DEBUG_ERROR("Write flash error at addr 0x%08X\r\n", addr);
        }
        else
        {
            DEBUG_INFO("Flash write data success at addr 0x%08X\r\n", addr);
        }
    }
}

#if DEBUG_FLASH
app_flash_data_t *dbg_ptr;
#endif
bool app_flash_get_data(uint32_t read_addr, app_spi_flash_data_t *rd_data, bool only_err)
{
    uint32_t struct_size = sizeof(app_spi_flash_data_t);
    flash_read_bytes(read_addr, (uint8_t *)rd_data, struct_size);
	uint32_t crc = utilities_calculate_crc32((uint8_t *)rd_data, struct_size - CRC32_SIZE);		// 4 mean size of CRC32
	
	/* Check if
		1. Key is valid
		2. Message need to resend to server, or flag only_error = false. Only error = false mean we want to read all message
		3. Timestamp is valid
		4. CRC is valid
	*/	
    if (rd_data->valid_flag == APP_FLASH_VALID_DATA_KEY 
        && (rd_data->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG || only_err == false)
        // && rd_data->timestamp
		&& only_err)
    {
        if (rd_data->crc != crc)
        {
            DEBUG_ERROR("Invalid CRC\r\n");
            return false;
        }
            
        rd_data->resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG; // mark no need to retransmission
		
		/* Alloc memory for 1 page flash data */
        uint8_t *page_data = umm_malloc(SPI_FLASH_SECTOR_SIZE); // Readback page data, maybe stack overflow
        if (page_data == NULL)
        {
            DEBUG_ERROR("Malloc flash page size, no memory\r\n");
            NVIC_SystemReset();
        }
		
		/* Calculate current sector address and sector count*/
        uint32_t current_sector_addr = read_addr - read_addr % 4096;
        uint32_t current_sector_count = current_sector_addr / SPI_FLASH_SECTOR_SIZE;
		
		/* Calculate next sector addr and sector count */
        uint32_t next_addr = read_addr + struct_size;
        uint32_t next_sector = next_addr / SPI_FLASH_SECTOR_SIZE;

        uint32_t size_remain_write_to_next_sector = 0;
		
		/* If next sector != current sector =>> Data in in apart of 2 page =>> Estimate number of bytes in second sector */
        if (next_sector != current_sector_count)
        {
            size_remain_write_to_next_sector = next_addr - next_sector * SPI_FLASH_SECTOR_SIZE;
        }
		
		/* Read data of current sector */
        flash_read_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);
		
		/* Data in fit in one page, write data and exit */
        if (size_remain_write_to_next_sector == 0)
        {
#if DEBUG_FLASH
            dbg_ptr = (app_flash_data_t *)(page_data + (read_addr - current_sector_addr));
#endif
            memcpy(page_data + (read_addr - current_sector_addr), (uint8_t *)rd_data, struct_size); // Copy new rd_data to page_data
            flash_erase_sector_4k(current_sector_addr / SPI_FLASH_SECTOR_SIZE);                                   // Rewrite page data
            flash_write_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);
        }
        else
        {
			/* Doan nay ko biet viet tieng anh kieu gi 
				1. Khi ma kich truoc cua struct nam tren 2 page =>> Neu ta xoa page di thi se bi mat data o struct ke tiep
					=>> Ta phai copy lai data cua page, sau do ghi lai data struct dau tien, giu lai data cua struct tiep theo
			*/
			
            uint32_t sector_offset = 0;
            uint32_t write_data_in_current_page_size = struct_size - size_remain_write_to_next_sector;
            DEBUG_INFO("0 - Copy %u bytes to offset %u\r\n", write_data_in_current_page_size, read_addr - current_sector_addr);
            
            // 1. Copy data cua page hien tai
            // Write a part of struct, which fit in current sector
            memcpy(page_data + (read_addr - current_sector_addr),
                   (uint8_t *)rd_data,
                   write_data_in_current_page_size);

            flash_erase_sector_4k(current_sector_count); // Rewrite page data
            flash_write_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);

            // Read next sector and write remain data of struct to new page
            if (next_sector >= SPI_FLASH_MAX_SECTOR)
            {
                DEBUG_WARN("We running to the end of flash\r\n");
                next_sector = 0;
                sector_offset = SPI_FLASH_PAGE_SIZE; // data from addr 0x0000->PAGESIZE is reserve for init process
            }
            
            // Copy data in next page
            flash_read_bytes(next_sector * SPI_FLASH_SECTOR_SIZE, page_data, SPI_FLASH_SECTOR_SIZE);
            memcpy(page_data + sector_offset, (uint8_t *)rd_data + write_data_in_current_page_size, size_remain_write_to_next_sector);
            
            
            DEBUG_INFO("1 - Copy %u bytes at offset %u\r\n", write_data_in_current_page_size, sector_offset);
            if (size_remain_write_to_next_sector)
            {
                DEBUG_WARN("Erase sector %u\r\n", next_sector * SPI_FLASH_SECTOR_SIZE);
                flash_erase_sector_4k(next_sector); 
                flash_write_bytes(next_sector * SPI_FLASH_SECTOR_SIZE, page_data, SPI_FLASH_SECTOR_SIZE);
            }
        }
#if DEBUG_FLASH
        flash_read_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE); // Debug only
#endif
		
		/* Readback data and compare */
        flash_read_bytes(read_addr, (uint8_t *)rd_data, struct_size);
        if (rd_data->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG)
        {
            DEBUG_ERROR("Remark flash error\r\n");
            umm_free(page_data);
            return false;
        }
        else
        {
            DEBUG_INFO("Remark flash success at addr 0x%08X\r\n", read_addr);
        }
        umm_free(page_data);
        return true;
    }

    return false;
}


void app_spi_flash_skip_to_end_flash_test(void)
{
    m_wr_addr = APP_SPI_FLASH_SIZE - 1;
}

bool app_spi_flash_get_stored_data(uint32_t addr, app_spi_flash_data_t *rd_data, bool only_error)
{
    return app_flash_get_data(addr, rd_data, only_error);
}

bool app_spi_flash_get_lastest_data(app_spi_flash_data_t *last_data)
{
    flash_read_bytes(m_last_write_data_addr, (uint8_t *)last_data, sizeof(app_spi_flash_data_t));
    if (last_data->valid_flag  == APP_FLASH_VALID_DATA_KEY)
    {
        return true;
    }
    return false;
}


/**
 * DTG01
	{
		"TotalPacket": 12345,               // S? d? li?u s? g?i
		"CurrentPacket": 12345,             // Th? t? packet dang g?i hi?n t?i
		"Error": "cam_bien_xung_dut",       // �? debug, kh�ng c?n x? l�
		"Timestamp": 1629200614,
		"ID": "G1-860262050125363",
		"Input1": 124511,                   // Ki?u int
		"Inputl_J3_1":	0.01,				// Ki?u float
		"BatteryLevel": 80,
		"Vbat": 4101,                      // �? debug, kh�ng c?n x? l�


		"SlaveID1" : 3,                         // �?a ch? slave
		"Register1_1": 64,
		"Unit1_1":"m3/s",

		"Register1_2": 339,
		"Unit1_2":"jun",

		"Register1_3": 64,                      // C�c tru?ng Register_x_y c� th? l� s? int ho?c float, N?u modbus d?c l?i th� gi� tr? l� FFFF c�i n�y do server y�u c?u, ki?u int hay float cung l� server y�u c?u
		"Unit1_3":"m3/s",

		"Register1_4": 12.3,	
		"Unit1_4":"jun",                        // �on vi tinh 
					
		"SlaveID2" : 4,                         // �ia chi salve
		"Register2_1": 0.0000,                  // Luu �, n�u server k c�i d?t modbus th� c�c tru?ng n�y s? ko c� ? json
		"Unit2_1":"kg",

		"Register2_2": 0,
		"Unit2_2":"1it",
		
		"Register2_3": 0.0000,
		"Unit2_3":"kg",
		
		"Register2_4": 0,
		"Unit2_4":"lit",

		"Temperature": 26,                  // Nhi?t d?
		"SIM": 452018100001935,             // Sim imei
		"Uptime": 7,                        // �? debug, kh�ng c?n x? l�
		"FW": "0.0.5",                      // Firmware version
		"HW": "0.0.1"                       // Hardware version
	}
 */
 
uint32_t app_spi_flash_dump_to_485(void)
{
	if (!m_flash_is_good)
	{
		return false;
	}
	RS485_POWER_EN(1);
	usart_lpusart_485_control(true);
	sys_delay_ms(100);
	
	usart_lpusart_485_send((uint8_t*)"Hawaco.Datalogger.PingMessage", strlen("Hawaco.Datalogger.PingMessage"));
	// Estimate total message
	uint32_t addr = SPI_FLASH_PAGE_SIZE;
	uint32_t total = 0, packet_num = 0;
	uint32_t struct_size = sizeof(app_spi_flash_data_t);
	
	app_spi_flash_data_t *rd = (app_spi_flash_data_t*)umm_malloc(struct_size);

	for (;;)
	{
		LED1_TOGGLE();
		flash_read_bytes(addr, (uint8_t*)rd, struct_size);
		uint32_t crc = utilities_calculate_crc32((uint8_t *)rd, sizeof(app_spi_flash_data_t) - CRC32_SIZE);		// 4 mean size of CRC32
        if (rd->valid_flag == APP_FLASH_VALID_DATA_KEY 
			&& rd->crc == crc)
		{
			total++;
		}
		
		LL_IWDG_ReloadCounter(IWDG);
		
		addr += struct_size;
		if (addr >= (APP_SPI_FLASH_SIZE - SPI_FLASH_PAGE_SIZE))
		{
			break;
		}
	}
	
	DEBUG_INFO("Found total %u message\r\n", total);
	
	addr = SPI_FLASH_PAGE_SIZE;
    uint32_t len = 0;
    
	if (total)
	{
#ifdef DTG01
		char *ptr = umm_malloc(512);
#else
		char *ptr = umm_malloc(1024+256);
#endif
		while (1)
		{
			len = 0;
			flash_read_bytes(addr, (uint8_t*)rd, struct_size);
			uint32_t crc = utilities_calculate_crc32((uint8_t *)rd, sizeof(app_spi_flash_data_t) - CRC32_SIZE);		// 4 mean size of CRC32
			if (rd->valid_flag == APP_FLASH_VALID_DATA_KEY 
				&& rd->crc == crc)
			{
				packet_num++;
				// Build message
				len += sprintf(ptr+len, "{\"TotalPacket\":%u,\"CurrentPacket\":%u,", total, packet_num);
				len += sprintf(ptr+len, "\"Timestamp\":%u,", rd->timestamp);
				len += sprintf((char *)(ptr + len), "\"BatteryLevel\":%u,", rd->vbat_precent);	
				len += sprintf((char *)(ptr + len), "\"Temperature\":%d,", rd->temp);
				
#ifdef DTG02
				len += sprintf((char *)(ptr + len), "\"ID\":\"G2-%s\",", gsm_get_module_imei());
				// Counter
				for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
				{
					// Build input pulse counter
					if (rd->counter[i].k == 0)
					{
						rd->counter[i].k = 1;
					}
					len += sprintf((char *)(ptr + len), "\"Input1_J%u\":%u,",
												i+1,
												rd->counter[i].real_counter/rd->counter[i].k + rd->counter[i].indicator);
					len += sprintf((char *)(ptr + len), "\"Input1_J%u_D\":%u,",
														i+1,
														rd->counter[i].reserve_counter/rd->counter[i].k /* + rd->counter[i].indicator */);
				}
				
				// Build input 4-20ma
				len += sprintf((char *)(ptr + len), "\"Input1_J3_1\":%.3f,", 
														rd->input_4_20mA[0]); // dau vao 4-20mA 0
				len += sprintf((char *)(ptr + len), "\"Input1_J3_2\":%.3f,", 
														rd->input_4_20mA[1]); // dau vao 4-20mA 0
				len += sprintf((char *)(ptr + len), "\"Input1_J3_3\":%.3f,", 
														rd->input_4_20mA[2]); // dau vao 4-20mA 0
				len += sprintf((char *)(ptr + len), "\"Input1_J3_4\":%.3f,", 
													rd->input_4_20mA[3]); // dau vao 4-20mA 0
				
				// Build input on/off digital
				len += sprintf((char *)(ptr + len), "\"Input1_J9_%u\":%u,", 
																		1,
																		rd->on_off.name.input_on_off_0);

				len += sprintf((char *)(ptr + len), "\"Input1_J9_%u\":%u,", 
														2,
														rd->on_off.name.input_on_off_1);
				len += sprintf((char *)(ptr + len), "\"Input1_J9_%u\":%u,", 
														3,
														rd->on_off.name.input_on_off_2);
				len += sprintf((char *)(ptr + len), "\"Input1_J9_%u\":%u,", 
														4,
														rd->on_off.name.input_on_off_3);
								
				// Build output on/off
				len += sprintf((char *)(ptr + len), "\"Output%u\":%u,", 
																	1,
																	rd->on_off.name.output_on_off_0);  //dau ra on/off 
				len += sprintf((char *)(ptr + len), "\"Output%u\":%u,", 
																	2,
																	rd->on_off.name.output_on_off_1);  //dau ra on/off 	
				len += sprintf((char *)(ptr + len), "\"Output%u\":%u,", 
																	3,
																	rd->on_off.name.output_on_off_2);  //dau ra on/off 
				len += sprintf((char *)(ptr + len), "\"Output%u\":%u,", 
																	4,
																	rd->on_off.name.output_on_off_3);  //dau ra on/off 
				
#else
				if (rd->counter[0].k == 0)
				{
					rd->counter[0].k = 1;
				}
				len += sprintf((char *)(ptr + len), "\"ID\":\"G1-%s\",", gsm_get_module_imei());
				len += sprintf((char *)(ptr + len), "\"Input1_J1\":%u,", rd->counter[0].real_counter/rd->counter[0].k + rd->counter[0].indicator);
				len += sprintf((char *)(ptr + len), "\"Input1_J1_D\":%u,", rd->counter[0].reserve_counter/rd->counter[0].k + rd->counter[0].indicator);
				len += sprintf((char *)(ptr + len), "\"Input1_R\":%u,", rd->counter[0].reserve_counter);
				len += sprintf((char *)(ptr + len), "\"Inputl_J3_1\":%.3f,", rd->input_4_20mA[0]);
#endif
				
				// output 4-20mA
				len += sprintf((char *)(ptr + len), "\"Output4_20\":%.3f,", rd->output_4_20mA[0]);   // dau ra 4-20mA 0
				
				//485			
				for (uint32_t index = 0; index < RS485_MAX_SLAVE_ON_BUS; index++)
				{
					len += sprintf((char *)(ptr + len), "\"SlaveID%u\":%u,", index+1, rd->rs485[index].slave_addr);
					for (uint32_t sub_idx = 0; sub_idx < RS485_MAX_SUB_REGISTER; sub_idx++)
					{
						if (rd->rs485[index].sub_register[sub_idx].read_ok
							&& rd->rs485[index].sub_register[sub_idx].data_type.name.valid)
						{
							len += sprintf((char *)(ptr + len), "\"Register%u_%u\":", index+1, sub_idx+1);
							if (rd->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT16 
								|| rd->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT32)
							{
								len += sprintf((char *)(ptr + len), "%u,", rd->rs485[index].sub_register[sub_idx].value);
							}
							else
							{
								len += sprintf((char *)(ptr + len), "%.4f,", (float)rd->rs485[index].sub_register[sub_idx].value);
							}
						}
						else if (rd->rs485[index].sub_register[sub_idx].data_type.name.valid)
						{
							if (strlen((char*)rd->rs485[index].sub_register[sub_idx].unit))
							{
								len += sprintf((char *)(ptr + len), "\"Unit%u_%u\":\"%s\",", index+1, sub_idx+1, rd->rs485[index].sub_register[sub_idx].unit);
							}
							len += sprintf((char *)(ptr + len), "\"Register%u_%u\":%s,", index+1, sub_idx+1, "-1");
						}	
					}
				}
				
				if (ptr[len-1] == ',')
				{
					ptr[len-1] = 0;
					len--;
				}
				len += sprintf(ptr+len, "%s", "}");
				usart_lpusart_485_send((uint8_t*)ptr, len);
				ptr[len] = 0;
				DEBUG_INFO("%s", ptr);
			}
//			else
//			{
//				DEBUG_WARN("Invalid CRC\r\n");
//			}				
			
			LL_IWDG_ReloadCounter(IWDG);
			
			LED1_TOGGLE();
#ifdef DTG01
			LED2_TOGGLE();
#endif
			addr += struct_size;
			if (addr >= (APP_SPI_FLASH_SIZE - SPI_FLASH_PAGE_SIZE))
			{
				break;
			}
		}
		umm_free(ptr);
	}
	
	umm_free(rd);
	
    char *config = umm_malloc(1024);     // TODO check malloc result
    len = 0;
    
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    len += snprintf((char *)(config + len), APP_EEPROM_MAX_SERVER_ADDR_LENGTH, "{\"Server0\":\"%s\",", eeprom_cfg->server_addr[0]);
    len += snprintf((char *)(config + len), APP_EEPROM_MAX_SERVER_ADDR_LENGTH, "\"Server1\":\"%s\",", eeprom_cfg->server_addr[1]);
    len += sprintf((char *)(config + len), "\"CycleSendWeb\":%u,", eeprom_cfg->send_to_server_interval_ms/1000/60);
    len += sprintf((char *)(config + len), "\"DelaySendToServer\":%u,", eeprom_cfg->send_to_server_delay_s);
    len += sprintf((char *)(config + len), "\"Cyclewakeup\":%u,", eeprom_cfg->measure_interval_ms/1000/60);
    len += sprintf((char *)(config + len), "\"MaxSmsOneday\":%u,", eeprom_cfg->max_sms_1_day);
    len += sprintf((char *)(config + len), "\"Phone\":\"%s\",", eeprom_cfg->phone);
    len += sprintf((char *)(config + len), "\"PollConfig\":%u,", eeprom_cfg->poll_config_interval_hour);
    len += sprintf((char *)(config + len), "\"DirLevel\":%u,", eeprom_cfg->dir_level);
    for (uint32_t i = 0; i < APP_EEPROM_METER_MODE_MAX_ELEMENT; i++)
    {
        len += sprintf((char *)(config + len), "\"K%u\":%u,", i+1, eeprom_cfg->k[i]);
        len += sprintf((char *)(config + len), "\"M%u\":%u,", i+1, eeprom_cfg->meter_mode[i]);
        len += sprintf((char *)(config + len), "\"MeterIndicator%u\":%u,", i+1, eeprom_cfg->offset[i]);
    }
    
    len += sprintf((char *)(config + len), "\"OutputIO_0\":%u,", eeprom_cfg->io_enable.name.output0);
    len += sprintf((char *)(config + len), "\"OutputIO_1\":%u,", eeprom_cfg->io_enable.name.output1);
    len += sprintf((char *)(config + len), "\"OutputIO_2\":%u,", eeprom_cfg->io_enable.name.output2);
    len += sprintf((char *)(config + len), "\"OutputIO_3\":%u,", eeprom_cfg->io_enable.name.output3);
    len += sprintf((char *)(config + len), "\"Input0\":%u,", eeprom_cfg->io_enable.name.input0);
    len += sprintf((char *)(config + len), "\"Input1\":%u,", eeprom_cfg->io_enable.name.input1);
    len += sprintf((char *)(config + len), "\"Input2\":%u,", eeprom_cfg->io_enable.name.input2);
    len += sprintf((char *)(config + len), "\"Input3\":%u,", eeprom_cfg->io_enable.name.input3);
    
//    len += sprintf((char *)(config + len), "\"SOS\":%u,", eeprom_cfg->io_enable.name.sos);
    len += sprintf((char *)(config + len), "\"Warning\":%u,", eeprom_cfg->io_enable.name.warning);
    len += sprintf((char *)(config + len), "\"485_EN\":%u,", eeprom_cfg->io_enable.name.rs485_en);
    if (eeprom_cfg->io_enable.name.rs485_en)
    {
        for (uint32_t slave_idx = 0; slave_idx < RS485_MAX_SLAVE_ON_BUS; slave_idx++)
		{
			for (uint32_t sub_register_index = 0; sub_register_index < RS485_MAX_SUB_REGISTER; sub_register_index++)
			{
				if (eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].data_type.name.valid == 0)
				{
					uint32_t slave_addr = eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].read_ok = 0;
					continue;
				}
                len += sprintf((char *)(config + len), "\"485_%u_Slave\":%u,", slave_idx, eeprom_cfg->rs485[slave_idx].slave_addr);
                len += sprintf((char *)(config + len), "\"485_%u_Reg\":%u,", slave_idx, eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].register_addr);
                len += snprintf((char *)(config + len), 6, "\"485_%u_Unit\":\"%s\",", slave_idx, (char*)eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].unit);
			}
		}
    }
    
    len += sprintf((char *)(config + len), "\"input4_20mA_0\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_0_enable);
#ifndef DTG01
    len += sprintf((char *)(config + len), "\"input4_20mA_1\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_1_enable);
    len += sprintf((char *)(config + len), "\"input4_20mA_2\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_2_enable);
    len += sprintf((char *)(config + len), "\"input4_20mA_3\":%u,", eeprom_cfg->io_enable.name.input_4_20ma_3_enable);
#endif
    len += sprintf((char *)(config + len), "\"Output4_20mA_En\":%u,", eeprom_cfg->io_enable.name.output_4_20ma_enable);
    if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
    {
        len += sprintf((char *)(config + len), "\"Output4_20mA_Val\":%.3f,", eeprom_cfg->output_4_20ma);
    }
    
    len += sprintf((char *)(config + len), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
    len += sprintf((char *)(config + len), "\"HW\":\"%s\",", VERSION_CONTROL_HW);
    len += sprintf((char *)(config + len), "\"FactoryServer\":\"%s\",", app_eeprom_read_factory_data()->server);
    
    len--;      // Skip ','
    len += sprintf((char *)(config + len), "%s", "}");     
    usart_lpusart_485_send((uint8_t*)config, len);
    
    umm_free(config);
    
	usart_lpusart_485_control(false);
	RS485_POWER_EN(0);

	LED1(0);
#ifdef DTG01
	LED2(0);
#endif
	return total;
}



