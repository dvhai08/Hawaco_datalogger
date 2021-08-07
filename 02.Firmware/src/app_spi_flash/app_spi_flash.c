#include "app_spi_flash.h"
#include "app_debug.h"
#include <string.h>
#include "spi.h"
#include "main.h"
#include "umm_malloc.h"
#include "sys_ctx.h"
#include "utilities.h"

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
static bool m_flash_inited = false;
uint32_t m_resend_data_in_flash_addr = SPI_FLASH_PAGE_SIZE; // sector 0
static uint32_t m_wr_addr = SPI_FLASH_PAGE_SIZE;            // sector 0
uint32_t m_last_write_data_addr = SPI_FLASH_PAGE_SIZE;


void app_spi_flash_initialize(void)
{
    sys_ctx_t *ctx = sys_ctx();
    ctx->peripheral_running.name.flash_running = 1;
    
    uint16_t flash_test_status = 0;
    //	HAL_SPI_Initialize();
    app_spi_flash_wakeup();
    if (flash_self_test())
    {
        m_flash_inited = true;
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
            DEBUG_PRINTF("UNKNOWNN: %u", m_flash_version);
            m_flash_inited = false;
            break;
        }
        DEBUG_RAW("\r\n");
    }
    else
    {
        m_flash_inited = false;
        DEBUG_ERROR("Flash init failed\r\n");
        return;
    }

    if (!flash_check_first_run()) //Neu lan dau khoi dong
    {
        flash_test_status = flash_test();

        DEBUG_PRINTF("Test R/W Flash: ");
        if (flash_test_status)
        {
            DEBUG_RAW("[FAIL], N = %u\r\n", flash_test_status);
            m_flash_inited = false;
        }
        else
        {
            DEBUG_RAW("[OK]\r\n");
            m_flash_inited = true;
        }
    }
    else
    {
//        DEBUG_VERBOSE("Check first run ok\r\n");
    }

    if (m_flash_inited)
    {
        bool flash_status;
        uint32_t addr = app_flash_estimate_next_write_addr(&flash_status);
        DEBUG_PRINTF("Estimate write addr 0x%08X\r\n", addr);
        
        app_spi_flash_estimate_current_read_addr(&flash_status);
        if (flash_status)
        {
            DEBUG_PRINTF("Flash : need retransmission data to server\r\n");
            DEBUG_PRINTF("Retransmission data in flash at addr 0x%08X\r\n", m_resend_data_in_flash_addr);
        }
    }
}

bool app_spi_flash_is_ok(void)
{
    return m_flash_inited;
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
    DEBUG_VERBOSE("Flash write page addr 0x%08X, size %u\r\n", addr, length);
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
        DEBUG_PRINTF("Page write success\r\n");
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
/**
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
    DEBUG_PRINTF("Flash write %u bytes, from addr 0x%08X\r\n", length, addr);
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
    DEBUG_PRINTF("Erase sector[%u] 4k\r\n", sector_count);
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
        DEBUG_PRINTF("Erase sector %u success\r\n", sector_count);
    }
    else
    {
        DEBUG_PRINTF("Erase sector %u error\r\n", sector_count);
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

        DEBUG_PRINTF("device id: 0x%02X, manufacture id: 0x%02X\r\n", device_id, manufacture_id);
        if (manufacture_id == 0x01 && device_id == 0x16) /* FL164K - 64Mb */
            m_flash_version = FL164K;
        else if (manufacture_id == 0x01 && device_id == 0x17) /* FL127S - 1 chan CS 128Mb */
            m_flash_version = FL127S;
        else if (manufacture_id == 0x01 && device_id == 0x18) /* FL256S - 1 chan CS 256Mb */
            m_flash_version = FL256S;
        else if (manufacture_id == 0xEF && device_id == 0x17) /* FL256S - 1 chan CS 256Mb */
        {
            DEBUG_PRINTF("W25Q128\r\n");
            m_flash_version = W25Q128;
        }
        else if (manufacture_id == 0xEF && device_id == 0x15)
        {
            DEBUG_PRINTF("W25Q32FV\r\n");
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

            DEBUG_PRINTF("status register: %02X\r\n", reg_status);
            if (reg_status & 0x20)
            {
                DEBUG_PRINTF("Address mode : 32 bit\r\n");
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
                DEBUG_PRINTF("Winbond W25Q256JV\r\n");
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

                DEBUG_PRINTF("status register: %02X\r\n", reg_status);
                if (reg_status & 0x01)
                {
                    DEBUG_PRINTF("Address mode : 32 bit\r\n");
                }
                SPI_EXT_FLASH_CS(1);
            }
            else if (device_id == 0x13)
            {
                DEBUG_PRINTF("Winbond W25Q80DLZPIG\r\n");
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
        DEBUG_PRINTF("Erase success\r\n");
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
        DEBUG_VERBOSE("Check Byte : 0x%X\r\n", tmp);
    }

    return 1;
}

uint32_t app_flash_estimate_next_write_addr(bool *flash_full)
{
    *flash_full = false;
    bool cont = true;
    while (cont)
    {
        app_spi_flash_data_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        flash_read_bytes(m_wr_addr, (uint8_t *)&tmp, sizeof(tmp));
        if (tmp.valid_flag == APP_FLASH_VALID_DATA_KEY)
        {
            m_last_write_data_addr = m_wr_addr;
            m_wr_addr += sizeof(tmp);
            if (m_wr_addr >= (APP_SPI_FLASH_SIZE - 2 * sizeof(app_spi_flash_data_t)))
            {
                *flash_full = true;
                cont = false;
            }
        }
        else
        {
            app_spi_flash_data_t empty;
            memset(&empty, 0xFF, sizeof(app_spi_flash_data_t));
            if (memcmp(&tmp, &empty, sizeof(app_spi_flash_data_t)) == 0)
            {
                cont = false;
            }
            else
            {
                m_last_write_data_addr = m_wr_addr;
                m_wr_addr += sizeof(tmp);
                if (m_wr_addr >= (APP_SPI_FLASH_SIZE - 2 * sizeof(app_spi_flash_data_t)))
                {
                    *flash_full = true;
                    cont = false;
                }
            }
        }
    }

    if (*flash_full)
    {
        m_wr_addr = SPI_FLASH_PAGE_SIZE;
        DEBUG_WARN("Flash full, erase first block\r\n");
        flash_erase_sector_4k(0);
        uint8_t tmp_buff[1] = {0xA5};
        flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
    }

    // Erase next page
    DEBUG_VERBOSE("Write addr 0x%08X\r\n", m_wr_addr);
    return m_wr_addr;
}

uint32_t find_retransmition_messgae(uint32_t begin_addr, uint32_t end_addr)
{
    app_spi_flash_data_t tmp;
    memset(&tmp, 0, sizeof(tmp));

    while (1)
    {
        flash_read_bytes(begin_addr, (uint8_t *)&tmp, sizeof(tmp));
        if (tmp.valid_flag == APP_FLASH_VALID_DATA_KEY)
        {
            if (tmp.resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG)
            {
                return begin_addr;
            }
            else
            {
                begin_addr += sizeof(app_spi_flash_data_t);
            }
        }
        else
        {
            begin_addr += sizeof(app_spi_flash_data_t);
        }

        if (begin_addr >= end_addr) // No more error found
        {
            return 0;
        }
    }
}

uint32_t app_spi_flash_estimate_current_read_addr(bool *found_error)
{
    uint32_t tmp_addr;
    app_spi_flash_data_t tmp;
    *found_error = false;

    if (m_resend_data_in_flash_addr == m_wr_addr) // Neu read = write =>> check current page status
    {
        flash_read_bytes(m_wr_addr, (uint8_t *)&tmp, sizeof(tmp));
		
		// Neu packet error
		uint32_t crc = utilities_calculate_crc32((uint8_t *)&tmp, sizeof(app_spi_flash_data_t) - CRC32_SIZE);		// 4 mean size of CRC32
        if (tmp.valid_flag == APP_FLASH_VALID_DATA_KEY 
            && (tmp.resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG)
			&& tmp.crc == crc)
        {
            m_resend_data_in_flash_addr = m_wr_addr;
            *found_error = true;
        }
    }
    else if (m_wr_addr > m_resend_data_in_flash_addr) // Neu write address > read address =>> Scan from read_addr to write_addr
    {
        tmp_addr = find_retransmition_messgae(m_resend_data_in_flash_addr, m_wr_addr);
        if (tmp_addr != 0)
        {
            m_resend_data_in_flash_addr = tmp_addr;
            *found_error = true;
        }
    }
    else // Write addr < read addr =>> buffer full =>> Scan 2 step
         // Step 1 : scan from read addr to max addr
         // Step 2 : scan from SPI_FLASH_PAGE_SIZE to write addr
    {
        tmp_addr = find_retransmition_messgae(m_resend_data_in_flash_addr, m_wr_addr);
        if (tmp_addr != 0)
        {
            m_resend_data_in_flash_addr = tmp_addr;
            *found_error = true;
        }
        else
        {
            tmp_addr = find_retransmition_messgae(SPI_FLASH_PAGE_SIZE, m_wr_addr);
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

    return m_resend_data_in_flash_addr;
}

uint32_t app_spi_flash_dump_all_data(void)
{
    uint8_t read[5];
    flash_read_bytes(0, read, 4);
    read[4] = 0;
    DEBUG_VERBOSE("Read data %s\r\n", (char *)read);
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
        DEBUG_VERBOSE("We need erase next sector %u\r\n", sector);
    }
    return retval;
}



void app_spi_flash_write_data(app_spi_flash_data_t *wr_data)
{
    DEBUG_VERBOSE("Flash write new data\r\n");
    if (!m_flash_inited)
    {
        DEBUG_ERROR("Flash init error, ignore write msg\r\n");
        return;
    }
    app_spi_flash_data_t rd_data;
//    wr_data->header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
//    while (1)
    {
        bool flash_full;
        uint32_t addr = app_flash_estimate_next_write_addr(&flash_full);
        
        // Check the next write address overlap to the next sector
        // =>> Erase next sector if needed
        uint32_t expect_write_sector = addr/SPI_FLASH_SECTOR_SIZE;
        uint32_t expect_next_sector = (addr+sizeof(app_spi_flash_data_t))/SPI_FLASH_SECTOR_SIZE;
        if (expect_next_sector != expect_write_sector)
        {
            DEBUG_VERBOSE("Consider erase next sector\r\n");
            // Consider write next page
            if (!app_spi_flash_check_empty_sector(expect_next_sector))
            {
                flash_erase_sector_4k(expect_next_sector);
            }
            else
            {
                DEBUG_VERBOSE("We dont need to erase next sector\r\n");
            }
        }
        
        if (flash_full)
        {
            DEBUG_WARN("Flash full\r\n");
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
            DEBUG_VERBOSE("Flash write data success\r\n");
        }
    }
}

#if DEBUG_FLASH
app_flash_data_t *dbg_ptr;
#endif
bool app_flash_mask_retransmiton_is_valid(uint32_t read_addr, app_spi_flash_data_t *rd_data)
{
    flash_read_bytes(read_addr, (uint8_t *)rd_data, sizeof(app_spi_flash_data_t));
	uint32_t crc = utilities_calculate_crc32((uint8_t *)rd_data, sizeof(app_spi_flash_data_t) - CRC32_SIZE);		// 4 mean size of CRC32
    if (rd_data->valid_flag == APP_FLASH_VALID_DATA_KEY 
        && rd_data->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG
        && rd_data->timestamp
		&& rd_data->crc == crc)
    {
        rd_data->resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG; // mark no need to retransmission

        uint8_t *page_data = umm_malloc(SPI_FLASH_SECTOR_SIZE); // Readback page data, maybe stack overflow
        if (page_data == NULL)
        {
            DEBUG_ERROR("Malloc flash page size, no memory\r\n");
            NVIC_SystemReset();
        }

        uint32_t current_sector_addr = read_addr - read_addr % 4096;
        uint32_t current_sector = current_sector_addr / SPI_FLASH_SECTOR_SIZE;

        uint32_t next_addr = read_addr + sizeof(app_spi_flash_data_t);
        uint32_t next_sector = next_addr / SPI_FLASH_SECTOR_SIZE;

        uint32_t size_remain_write_to_next_sector = 0;

        if (next_sector != current_sector) // Data overlap to the next sector
        {
            size_remain_write_to_next_sector = next_addr - next_sector * SPI_FLASH_SECTOR_SIZE;
        }

        flash_read_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);

        if (size_remain_write_to_next_sector == 0)
        {
#if DEBUG_FLASH
            dbg_ptr = (app_flash_data_t *)(page_data + (read_addr - current_sector_addr));
#endif
            memcpy(page_data + (read_addr - current_sector_addr), (uint8_t *)&rd_data, sizeof(app_spi_flash_data_t)); // Copy new rd_data to page_data
            flash_erase_sector_4k(current_sector_addr / SPI_FLASH_SECTOR_SIZE);                                   // Rewrite page data
            flash_write_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);
        }
        else
        {
            uint32_t sector_offset = 0;
            // Write current sector
            memcpy(page_data + (read_addr - current_sector_addr),
                   (uint8_t *)rd_data,
                   sizeof(app_spi_flash_data_t) - size_remain_write_to_next_sector);

            flash_erase_sector_4k(current_sector); // Rewrite page data
            flash_write_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE);

            // Read next sector and write remain data to new page
            if (next_sector >= SPI_FLASH_MAX_SECTOR)
            {
                DEBUG_WARN("We running to the end of flash\r\n");
                next_sector = 0;
                sector_offset = SPI_FLASH_PAGE_SIZE; // data from addr 0x0000->PAGESIZE is reserve for init process
            }
            flash_read_bytes(next_sector * SPI_FLASH_SECTOR_SIZE, page_data, SPI_FLASH_SECTOR_SIZE);
            memcpy(page_data + sector_offset, (uint8_t *)rd_data + size_remain_write_to_next_sector, size_remain_write_to_next_sector);
            flash_write_bytes(next_sector * SPI_FLASH_SECTOR_SIZE, page_data, SPI_FLASH_SECTOR_SIZE);
        }
#if DEBUG_FLASH
        flash_read_bytes(current_sector_addr, page_data, SPI_FLASH_SECTOR_SIZE); // Debug only
#endif
        flash_read_bytes(read_addr, (uint8_t *)&rd_data, sizeof(app_spi_flash_data_t)); // Readback and compare
        if (rd_data->resend_to_server_flag != APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG)
        {
            DEBUG_ERROR("Remark flash error\r\n");
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

void app_spi_flash_stress_test(uint32_t nb_of_write_times)
{
    DEBUG_INFO("Flash write %u times\r\n", nb_of_write_times);
    app_spi_flash_data_t wr_data;
    memset(&wr_data, 0, sizeof(wr_data));
    while (1)
    {
        bool flash_full;
        uint32_t addr = app_flash_estimate_next_write_addr(&flash_full);
        if (!flash_full)
        {
            wr_data.meter_input[0].forward = 3;
            wr_data.meter_input[0].reserve = 4;
#ifdef DTG02
            wr_data.meter_input[1].forward = 5;
            wr_data.meter_input[1].reserve = 6;
#endif
//            wr_data.header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
            wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
            if (nb_of_write_times % 2 == 0)
            {
                wr_data.resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG;
            }
            else
            {
                wr_data.resend_to_server_flag = 0;
            }
            app_spi_flash_write_data(&wr_data);
            if (nb_of_write_times-- == 0)
            {
                break;
            }
        }
        else
        {
            DEBUG_INFO("Exit flash test\r\n");
            break;
        }
    }
}

//void app_flash_mark_addr_need_retransmission(uint32_t addr)
//{
//    app_flash_mask_retransmiton_is_valid(addr);
//}

void app_spi_flash_retransmission_data_test(void)
{
    bool retransmition = true;
    while (retransmition)
    {
        uint32_t addr = app_spi_flash_estimate_current_read_addr(&retransmition);
        app_spi_flash_data_t rd_data;
        if (retransmition)
        {
            DEBUG_VERBOSE("Retransmition addr 0x%08X\r\n", addr);
            app_flash_mask_retransmiton_is_valid(addr, &rd_data);
        }
//        else
//        {
//            DEBUG_VERBOSE("No retransmission data found\r\n");
//        }
    }
}

void app_spi_flash_skip_to_end_flash_test(void)
{
    m_wr_addr = APP_SPI_FLASH_SIZE - 1;
}

bool app_spi_flash_get_retransmission_data(uint32_t addr, app_spi_flash_data_t *rd_data)
{
    return app_flash_mask_retransmiton_is_valid(addr, rd_data);
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



