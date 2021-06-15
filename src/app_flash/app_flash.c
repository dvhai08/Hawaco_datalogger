#include "app_flash.h"
#include "main.h"
#include "stm32l0xx_ll_gpio.h"

#define FL164K 1        // 8 MB
#define FL127S 2        // 16 MB
#define FL256S 3        // 32 MB
#define GD256C 4        // 32MB
#define W25Q256JV 5     // 32MB
#define W25Q80DLZPIG 6  // 8Mbit

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
#define PP_CMD4 0x12        //Page Program
#define SE_CMD4 0x21        //Sector Erase - 4KB - 32bit addrress
#define BE_CMD4 0xDC        //Block Erase - 64KB - 32bits address

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
#define WB_RESET_STEP0_CMD      0x66
#define WB_RESET_STEP1_CMD      0x99


#define FLASH_CHECK_FIRST_RUN (uint32_t)0x0000

static uint8_t flash_self_test(void);
static uint8_t flash_check_first_run(void);
uint8_t flash_test(void);
static uint8_t m_flash_version;

static void reset_flash(void)
{
    FLASH_SPI_SendByte(WB_RESET_STEP0_CMD);
    LASH_SPI_SendByte(WB_RESET_STEP1_CMD);
}

static inline void enable_ext_flash(bool enable)
{
    if (enable)
    {
        LL_GPIO_ResetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin);
    }
    else
    {
        LL_GPIO_SetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin);
    }
}

void InitFlash(void)
{
    uint16_t flash_test = 0;

    Init_MemorySPI(FLASH_SPI);

    if (flash_self_test())
    {
        DEBUG_PRINTF("Flashs selftest[OK]\r\nFlash type: ");
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
        default:
            DEBUG_PRINTF("UNKNOWN: %u", m_flash_version);
            break;
        }
        xSystem.status.DeviceState.Name.Flash = TRANGTHAI_OK;
    }
    else
    {
        xSystem.status.DeviceState.Name.Flash = TRANGTHAI_FAIL;
        DEBUG_PRINTF("Khoi tao Flash: [FAIL]");
        return;
    }

    if (!flash_check_first_run()) //Neu lan dau khoi dong
    {
        flash_test = flash_test();

        DEBUG_PRINTF("Test R/W Flash: ");
        if (flash_test)
        {
            DEBUG_RAW("[FAIL], N = %u\r\n", flash_test);
            xSystem.status.DeviceState.Name.Flash = TRANGTHAI_FAIL;
        }
        else
        {
            DEBUG_RAW("[OK]\r\n");
            xSystem.status.DeviceState.Name.Flash = TRANGTHAI_OK;
        }
    }
}

static uint8_t FLASH_SPI_SendByte(uint8_t byte)
{
    uint32_t timeout = 1000;

    while ((SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET) && timeout)
    {
        __WFI();
        timeout--;
    }

    if (timeout == 0)
    {
        DEBUG_PRINTF("Write SPI failed\r\n");
    }

    SPI_I2S_SendData(FLASH_SPI, byte);

    timeout = 1000;
    while ((SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET) && timeout)
    {
        __WFI();
        timeout--;
    }
    if (timeout == 0)
    {
        DEBUG_PRINTF("Read SPI failed\r\n");
    }

    return SPI_I2S_ReceiveData(FLASH_SPI);
}

static void flash_write_control(uint8_t enable, uint32_t addr)
{
    if (enable)
    {
        enable_ext_flash(1);
        FLASH_SPI_SendByte(WREN_CMD);
        enable_ext_flash(0);
    }
    else
    {
        enable_ext_flash(1);
        FLASH_SPI_SendByte(WRDI_CMD);
        enable_ext_flash(0);
    }
}

static void wait_write_in_process(uint32_t addr)
{
    uint32_t timeout = 500000; //Adjust depend on system clock
    uint8_t status = 0;

    /* Doc thanh ghi */
    enable_ext_flash(1);
    FLASH_SPI_SendByte(RDSR_CMD);

    while (timeout)
    {
        timeout--;
        if (timeout % 100 == 0)
            ResetWatchdog();

        status = FLASH_SPI_SendByte(SPI_DUMMY);
        if ((status & 1) == 0)
            break;
    }
    enable_ext_flash(0);
}

static void flash_write_page(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    uint16_t i = 0;
    uint32_t OldAddress = addr;

    flash_write_control(1, addr);
    enable_ext_flash(1);

    ResetWatchdog();
    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh ghi */
        FLASH_SPI_SendByte(PP_CMD4);

        /* Gui 4 byte dia chi */
        FLASH_SPI_SendByte((addr >> 24) & 0xFF);
    }
    else
    {
        /* Gui lenh ghi */
        FLASH_SPI_SendByte(PP_CMD);
    }

    /* Gui 3 byte dia chi */
    FLASH_SPI_SendByte((addr >> 16) & 0xFF);
    FLASH_SPI_SendByte((addr >> 8) & 0xFF);
    FLASH_SPI_SendByte(addr & 0xFF);

    /* Gui du lieu */
    for (i = 0; i < length; i++)
        FLASH_SPI_SendByte(buffer[i]);

    enable_ext_flash(0);
    Delayms(1); //Khong cho lenh nay ghi k thanh cong!!
    wait_write_in_process(OldAddress);
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
void flash_write_bytes(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    /* Phan bo du lieu thanh tung page */
    uint16_t offset_addr = 0;
    uint16_t length_need_to_write = 0;
    uint16_t nb_bytes_written = 0;

    while (length)
    {
        offset_addr = addr % 256;

        if (offset_addr > 0)
        {
            if (offset_addr + length > 256)
            {
                length_need_to_write = 256 - offset_addr;
            }
            else
            {
                length_need_to_write = length;
            }
        }
        else
        {
            if (length > 256)
            {
                length_need_to_write = 256;
            }
            else
            {
                length_need_to_write = length;
            }
        }

        length -= length_need_to_write;

        flash_write_page(addr, &buffer[nb_bytes_written], length_need_to_write);

        /* Cap nhat gia tri */
        nb_bytes_written += length_need_to_write;

        addr += length_need_to_write;

        ResetWatchdog();
    }
}

void flash_read_bytes(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    uint16_t i = 0;

    enable_ext_flash(1);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh doc */
        FLASH_SPI_SendByte(READ_DATA_CMD4);

        /* Gui 4 byte dia chi */
        FLASH_SPI_SendByte((addr >> 24) & 0xFF);
    }
    else
    {
        /* Gui lenh doc */
        FLASH_SPI_SendByte(READ_DATA_CMD);
    }

    /* Gui 3 byte dia chi */
    FLASH_SPI_SendByte((addr >> 16) & 0xFF);
    FLASH_SPI_SendByte((addr >> 8) & 0xFF);
    FLASH_SPI_SendByte(addr & 0xFF);

    /* Lay du lieu */
    for (i = 0; i < length; i++)
        buffer[i] = FLASH_SPI_SendByte(SPI_DUMMY);

    enable_ext_flash(0);
}

/*****************************************************************************/
/**
* @brief	:  Sector Size = 4KB. Chi dung cho chip Flash: S25FL116K, S25FL132K, S25FL164K
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void FlashEraseSector4K(uint16_t SectorCount)
{
    uint32_t addr = 0;
    uint32_t OldAddress = 0;

    addr = SectorCount * 4096; //Sector 4KB
    OldAddress = addr;

    flash_write_control(1, addr);
    Delayms(1);

    enable_ext_flash(1);
    Delayms(1);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh */
        FLASH_SPI_SendByte(SE_CMD4);

        /* Gui 4 bytes dia chi */
        FLASH_SPI_SendByte((addr >> 24) & 0xFF);
    }
    else
    {
        /* Gui lenh */
        FLASH_SPI_SendByte(SE_CMD);
    }

    /* Gui 3 bytes dia chi */
    FLASH_SPI_SendByte((addr >> 16) & 0xFF);
    FLASH_SPI_SendByte((addr >> 8) & 0xFF);
    FLASH_SPI_SendByte(addr & 0xFF);

    enable_ext_flash(0);
    Delayms(1);
    wait_write_in_process(OldAddress);
}

/*****************************************************************************/
/**
 * @brief	:  Block Size = 64KB. 
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void FlashEraseBlock64K(uint16_t SectorCount)
{
    uint32_t addr = 0;
    uint32_t OldAddress = 0;

    addr = SectorCount * 65536; //Sector 64KB
    OldAddress = addr;

    flash_write_control(1, addr);
    Delayms(1);

    enable_ext_flash(1);
    Delayms(1);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh */
        FLASH_SPI_SendByte(BE_CMD4);

        /* Gui 4 bytes dia chi */
        FLASH_SPI_SendByte((addr >> 24) & 0xFF);
    }
    else
    {
        /* Gui lenh */
        FLASH_SPI_SendByte(BE_CMD);
    }

    /* Gui 3 bytes dia chi */
    FLASH_SPI_SendByte((addr >> 16) & 0xFF);
    FLASH_SPI_SendByte((addr >> 8) & 0xFF);
    FLASH_SPI_SendByte(addr & 0xFF);

    enable_ext_flash(0);
    Delayms(1);
    wait_write_in_process(OldAddress);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t flash_self_test(void)
{
    uint8_t manufacture_id = 0xFF;
    uint8_t device_id = 0xFF;
    uint8_t reg_status = 0;
    uint8_t loop_count;

    //Retry init 3 times
    for (loop_count = 0; loop_count < 3; loop_count++)
    {
        enable_ext_flash(1);

        /* Lenh */
        FLASH_SPI_SendByte(READ_ID_CMD); /* 0x90 */

        /* 3 byte address */
        FLASH_SPI_SendByte(0);
        FLASH_SPI_SendByte(0);
        FLASH_SPI_SendByte(0);

        manufacture_id = FLASH_SPI_SendByte(0xFF);
        device_id = FLASH_SPI_SendByte(0xFF);

        enable_ext_flash(0);

        //    if (device_id == 0x16)			/* FL164K - 64Mb */
        //        m_flash_version = FL164K;
        //    else if (device_id == 0x17)	/* FL127S - 1 chan CS 128Mb */
        //        m_flash_version= FL127S;
        //    else if (device_id == 0x18)	/* FL256S - 1 chan CS 256Mb */
        //        m_flash_version = FL256S;

        DEBUG_PRINTF("device id: %X, manufacture id: %X", device_id, manufacture_id);
        if (manufacture_id == 0x01 && device_id == 0x16) /* FL164K - 64Mb */
            m_flash_version = FL164K;
        else if (manufacture_id == 0x01 && device_id == 0x17) /* FL127S - 1 chan CS 128Mb */
            m_flash_version = FL127S;
        else if (manufacture_id == 0x01 && device_id == 0x18) /* FL256S - 1 chan CS 256Mb */
            m_flash_version = FL256S;
        else if (manufacture_id == 0xC8 && device_id == 0x18) /* GD256C - GigaDevice 256Mb */
        {
            m_flash_version = GD256C;

            //Vao che do 4-Byte addr
            enable_ext_flash(1);
            FLASH_SPI_SendByte(EN4B_MODE_CMD); //0xB7
            enable_ext_flash(0);

            Delayms(10);
            //Doc thanh ghi status 2, bit ADS - 5
            enable_ext_flash(1);
            FLASH_SPI_SendByte(RDSR2_CMD);

            reg_status = FLASH_SPI_SendByte(SPI_DUMMY);
            enable_ext_flash(0);

            DEBUG_PRINTF("status register: %02X", reg_status);
            if (reg_status & 0x20)
            {
                DEBUG_PRINTF("Da vao che do 32 bits dia chi");
            }
            else
            {
                Delayms(500);
                continue;
            }
        }
        else if (manufacture_id == 0xEF && device_id == 0x18) /* W25Q256JV - Winbond 256Mb */
        {
            if (device_id == 0x18)
            {
                DEBUG_PRINTF("Winbond W25Q256JV\r\n");
                m_flash_version = W25Q256JV;
                //Vao che do 4-Byte addr
                enable_ext_flash(1);
                FLASH_SPI_SendByte(EN4B_MODE_CMD); //0xB7
                enable_ext_flash(0);

                Delayms(10);
                //Doc thanh ghi status 3, bit ADS  (S16) - bit 0
                enable_ext_flash(1);
                FLASH_SPI_SendByte(RDSR3_CMD);

                reg_status = FLASH_SPI_SendByte(SPI_DUMMY);
                DEBUG_PRINTF("status register: %02X", reg_status);
                if (reg_status & 0x01)
                {
                    DEBUG_PRINTF("Da vao che do 32 bits dia chi");
                }
                enable_ext_flash(0);
            }
            else if (device_id == 0x13)
            {
                DEBUG_PRINTF("Winbond W25Q80DLZPIG\r\n");
                m_flash_version = W25Q80DLZPIG;
                //Vao che do 4-Byte addr
                enable_ext_flash(1);
                // FLASH_SPI_SendByte(EN4B_MODE_CMD); //0xB7
                // enable_ext_flash(0);

                // Delayms(10);
                // //Doc thanh ghi status 3, bit ADS  (S16) - bit 0
                // enable_ext_flash(1);
                // FLASH_SPI_SendByte(RDSR3_CMD);

                // reg_status = FLASH_SPI_SendByte(SPI_DUMMY);
                // DEBUG_PRINTF("status register: %02X", reg_status);
                // if (reg_status & 0x01)
                // {
                //     DEBUG_PRINTF("Da vao che do 32 bits dia chi");
                // }
                reset_flash();
                enable_ext_flash(0);
            }
        }

        if (m_flash_version == 0xFF || manufacture_id == 0xFF)
        {
            BuzzerBeep(2000);
            DEBUG_PRINTF("DevID: %X, FacID: %X", device_id, manufacture_id);
            Delayms(500);
        }
        else
        {
            return 1;
        }
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
    Delayms(100);
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

/*****************************************************************************/
/**
 * @brief	:  	Clear FLASH
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	09/09/2015
 * @version	:
 * @reviewer:	
 */
void FLASH_Clear(void)
{
    uint8_t status = 0xFF;
    DEBUG_PRINTF("Erase all flash");

    flash_write_control(1, 0);
    enable_ext_flash(1);
    FLASH_SPI_SendByte(CE_CMD);
    enable_ext_flash(0);

    /* Doc thanh ghi status */
    Delayms(100);
    enable_ext_flash(1);
    FLASH_SPI_SendByte(RDSR_CMD);

    while (1)
    {
        ResetWatchdog();
        status = FLASH_SPI_SendByte(SPI_DUMMY);
        if ((status & 1) == 0)
            break;
        Delayms(10);
    }
    enable_ext_flash(0);

    DEBUG_PRINTF("[DONE]\r\n");
}

/*****************************************************************************/
/**
 * @brief	:  	Check FLASH first run
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	09/09/2015
 * @version	:
 * @reviewer:	
 */
static uint8_t flash_check_first_run(void)
{
    uint8_t tmp = 0;
    uint8_t tmp_buff[2];

    flash_read_bytes(FLASH_CHECK_FIRST_RUN, &tmp, 1);

    if (tmp != 0xA5)
    {
        DEBUG_PRINTF("Erase block 64kb\r\n");
        FlashEraseBlock64K(0); //Xoa sector dau tien cho nhanh

        tmp_buff[0] = 0xA5;
        flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
        return 0;
    }
    else
    {
        DEBUG_PRINTF("Check Byte : 0x%X\r\n", tmp);
    }

    return 1;
}
