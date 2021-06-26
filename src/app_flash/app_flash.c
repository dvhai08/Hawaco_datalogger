#include "app_flash.h"
#include "app_debug.h"
#include <string.h>
#include "spi.h"
#include "main.h"

#define FL164K 1        // 8 MB
#define FL127S 2        // 16 MB
#define FL256S 3        // 32 MB
#define GD256C 4        // 32MB
#define W25Q256JV 5     // 32MB
#define W25Q80DLZPIG 6  // 8Mbit
#define W25Q128 	7  // 8Mbit

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
#define WB_POWER_DOWN_CMD		0xB9
#define WB_WAKEUP_CMD			0xAB


#define FLASH_CHECK_FIRST_RUN (uint32_t)0x0000

#define PAGE_SIZE				256
#define SECTOR_SIZE				4096

#define HAL_SPI_CS(x)       {   \
                                if (x)  LL_GPIO_SetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin); \
                                else    LL_GPIO_ResetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin); \
                            }

#define HAL_SPI_Initialize()        while(0)
#define EXT_FLASH_HSPI              hspi2

static uint8_t flash_self_test(void);
static uint8_t flash_check_first_run(void);
uint8_t flash_test(void);
void flash_read_bytes(uint32_t addr, uint8_t *buffer, uint16_t length);
static uint8_t m_flash_version;
static bool m_flash_inited = false;
static uint32_t m_read_addr = PAGE_SIZE;		// sector 1
static uint32_t m_wr_addr = PAGE_SIZE;					// sector 1
void flash_erase_sector_4k(uint16_t sector_count);

static void reset_flash(void)
{
	uint8_t cmd = WB_RESET_STEP0_CMD;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = WB_RESET_STEP1_CMD;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
}

static inline void enable_ext_flash(bool enable)
{
    if (enable)
    {
        HAL_SPI_CS(false);
    }
    else
    {
        HAL_SPI_CS(true);
    }
}

void app_flash_initialize(void)
{
    uint16_t flash_test_status = 0;
	HAL_SPI_Initialize();
    if (flash_self_test())
    {
        m_flash_inited = true;
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
		case W25Q80DLZPIG:
			DEBUG_RAW("W25Q80DLZPIG. ");
			break;
		case W25Q128:
			DEBUG_RAW("W25Q128. ");
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
        DEBUG_PRINTF("Khoi tao Flash: [FAIL]\r\n");
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
		DEBUG_PRINTF("Check first run ok\r\n");
	}
}

bool app_flash_is_ok(void)
{
	return m_flash_inited;
}

static void flash_write_control(uint8_t enable, uint32_t addr)
{
	uint8_t cmd;
    if (enable)
    {
        enable_ext_flash(1);
		cmd = WREN_CMD;
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
        enable_ext_flash(0);
    }
    else
    {
        enable_ext_flash(1);
		cmd = WRDI_CMD;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
        enable_ext_flash(0);
    }
}

static void wait_write_in_process(uint32_t addr)
{
    uint32_t timeout = 500000; //Adjust depend on system clock
    uint8_t status = 0;

    /* Doc thanh ghi */
    enable_ext_flash(1);
	uint8_t cmd = RDSR_CMD;
	
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

    while (timeout)
    {
        timeout--;
        cmd = SPI_DUMMY;
		HAL_SPI_Receive(&EXT_FLASH_HSPI, &cmd, 1, 100);
		
        if ((status & 1) == 0)
            break;
    }
    enable_ext_flash(0);
}

static void flash_write_page(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    uint16_t i = 0;
    uint32_t old_addr = addr;

    flash_write_control(1, addr);
    enable_ext_flash(1);

//    ResetWatchdog();
	uint8_t cmd;
    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
		/* Gui lenh ghi */
		cmd = PP_CMD4;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

//        /* Gui 4 byte dia chi */
		cmd = (addr >> 24) & 0xFF;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }
    else
    {
        /* Gui lenh ghi */
		cmd = PP_CMD;
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }

    /* Gui 3 byte dia chi */
	cmd = (addr >> 16) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = (addr >> 8) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = addr & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

    /* Gui du lieu */
    for (i = 0; i < length; i++)
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, buffer + i, 1, 100);

    enable_ext_flash(0);
    wait_write_in_process(old_addr);
	
	// Erase next page
	addr += PAGE_SIZE;
	if (addr >= APP_FLASH_SIZE)
	{
		addr = PAGE_SIZE;		// Page 1
	}
	uint8_t tmp[APP_FLASH_SIZE];
	flash_read_bytes(addr, tmp, PAGE_SIZE);
	bool need_erase = false;
	for (uint32_t i = 0; i < PAGE_SIZE; i++)
	{
		if (tmp[i] != 0xFF)
		{
			need_erase = true;
			break;
		}
	}
	if (need_erase)
	{
		flash_erase_sector_4k(need_erase/SECTOR_SIZE);
	}
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
        offset_addr = addr % PAGE_SIZE;

        if (offset_addr > 0)
        {
            if (offset_addr + length > PAGE_SIZE)
            {
                length_need_to_write = PAGE_SIZE - offset_addr;
            }
            else
            {
                length_need_to_write = length;
            }
        }
        else
        {
            if (length > PAGE_SIZE)
            {
                length_need_to_write = PAGE_SIZE;
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

//        ResetWatchdog();
    }
}



void flash_read_bytes(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    uint16_t i = 0;
	uint8_t cmd;
    enable_ext_flash(1);
	
    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh doc */
		cmd = READ_DATA_CMD4;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
//        FLASH_SPI_SendByte(READ_DATA_CMD4);

        /* Gui 4 byte dia chi */
//        FLASH_SPI_SendByte((addr >> 24) & 0xFF);
		cmd = (addr >> 24) & 0xFF;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }
    else
    {
        /* Gui lenh doc */
		cmd = READ_DATA_CMD;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
//        FLASH_SPI_SendByte(READ_DATA_CMD);
    }

    /* Gui 3 byte dia chi */
	cmd = (addr >> 16) & 0xFF;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = (addr >> 8) & 0xFF;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = addr & 0xFF;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = SPI_DUMMY;
    /* Lay du lieu */
	HAL_SPI_Receive(&EXT_FLASH_HSPI, buffer, length, 100);

    enable_ext_flash(0);
}


void flash_erase_sector_4k(uint16_t sector_count)
{
    uint32_t addr = 0;
    uint32_t old_addr = 0;
	uint8_t cmd;
    addr = sector_count * SECTOR_SIZE; //Sector 4KB
    old_addr = addr;

    flash_write_control(1, addr);
    enable_ext_flash(1);

    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh */
		cmd = SE_CMD4;
//        FLASH_SPI_SendByte(SE_CMD4);
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

        /* Gui 4 bytes dia chi */
		cmd = (addr >> 24) & 0xFF;
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }
    else
    {
        /* Gui lenh */
		cmd = SE_CMD;
//        FLASH_SPI_SendByte(SE_CMD);
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }

    /* Gui 3 bytes dia chi */
	cmd = (addr >> 16) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = (addr >> 8) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = addr & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

    enable_ext_flash(0);
    wait_write_in_process(old_addr);
}


void flash_erase_block_64K(uint16_t sector_count)
{
    uint32_t addr = 0;
    uint32_t old_addr = 0;
	uint8_t cmd;
	
    addr = sector_count * 65536; //Sector 64KB
    old_addr = addr;

    flash_write_control(1, addr);

    enable_ext_flash(1);


    if (m_flash_version == FL256S || m_flash_version == GD256C || m_flash_version == W25Q256JV)
    {
        /* Gui lenh */
		cmd = BE_CMD4;
		HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
//        FLASH_SPI_SendByte(BE_CMD4);

        /* Gui 4 bytes dia chi */
		cmd = (addr >> 24) & 0xFF;
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }
    else
    {
        /* Gui lenh */
		cmd = BE_CMD;
        HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    }

    /* Gui 3 bytes dia chi */
	cmd = (addr >> 16) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = (addr >> 8) & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	cmd = addr & 0xFF;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

    enable_ext_flash(0);
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
        enable_ext_flash(1);
        while (1)
        {
            /* Lenh */
            cmd = READ_ID_CMD;
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);

            /* 3 byte address */
            cmd = 0;
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
            
            cmd = 0xFF;
            HAL_SPI_TransmitReceive(&EXT_FLASH_HSPI, &cmd, &manufacture_id, 1, 100);
            HAL_SPI_TransmitReceive(&EXT_FLASH_HSPI, &cmd, &device_id, 1, 100);

            enable_ext_flash(1);
            if (manufacture_id == 0 && device_id == 0)
            {
                DEBUG_PRINTF("device id: %X, manufacture id: %X\r\n", device_id, manufacture_id);
                sys_delay_ms(100);
            }
        }

        //    if (device_id == 0x16)			/* FL164K - 64Mb */
        //        m_flash_version = FL164K;
        //    else if (device_id == 0x17)	/* FL127S - 1 chan CS 128Mb */
        //        m_flash_version= FL127S;
        //    else if (device_id == 0x18)	/* FL256S - 1 chan CS 256Mb */
        //        m_flash_version = FL256S;

        DEBUG_PRINTF("device id: %X, manufacture id: %X\r\n", device_id, manufacture_id);
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
        else if (manufacture_id == 0xC8 && device_id == 0x18) /* GD256C - GigaDevice 256Mb */
        {
            m_flash_version = GD256C;

            //Vao che do 4-Byte addr
            enable_ext_flash(1);
			cmd = EN4B_MODE_CMD;
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
            enable_ext_flash(0);

            sys_delay_ms(10);
            //Doc thanh ghi status 2, bit ADS - 5
            enable_ext_flash(1);
			cmd = RDSR2_CMD;
            HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
			
			cmd = SPI_DUMMY;
			HAL_SPI_TransmitReceive(&EXT_FLASH_HSPI, &cmd, &reg_status, 1, 100);
            enable_ext_flash(0);

            DEBUG_PRINTF("status register: %02X\r\n", reg_status);
            if (reg_status & 0x20)
            {
                DEBUG_PRINTF("Da vao che do 32 bits dia chi\r\n");
            }
            else
            {
                sys_delay_ms(500);
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
				cmd = EN4B_MODE_CMD;
                HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
                enable_ext_flash(0);

                sys_delay_ms(10);
                //Doc thanh ghi status 3, bit ADS  (S16) - bit 0
                enable_ext_flash(1);
				cmd = RDSR3_CMD;
                HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
				
				cmd = SPI_DUMMY;
                HAL_SPI_TransmitReceive(&EXT_FLASH_HSPI, &cmd, &reg_status, 1, 100);
				
                DEBUG_PRINTF("status register: %02X\r\n", reg_status);
                if (reg_status & 0x01)
                {
                    DEBUG_PRINTF("Da vao che do 32 bits dia chi\r\n");
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
            DEBUG_PRINTF("DevID: %X, FacID: %X\r\n", device_id, manufacture_id);
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

void app_flash_erase(void)
{
    uint8_t status = 0xFF;
    uint8_t cmd;
	DEBUG_PRINTF("Erase all flash\r\n");
	
    flash_write_control(1, 0);
    enable_ext_flash(1);
	
	cmd = CE_CMD;
    HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
    enable_ext_flash(0);

    /* Doc thanh ghi status */
    sys_delay_ms(100);
    enable_ext_flash(1);
	cmd = RDSR_CMD;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	
	uint32_t begin_tick = sys_get_ms();
    while (1)
    {
		cmd = SPI_DUMMY;
        HAL_SPI_TransmitReceive(&EXT_FLASH_HSPI, &cmd, &status, 1, 100);
        if ((status & 1) == 0)
            break;
        sys_delay_ms(100);
    }
    enable_ext_flash(0);

    DEBUG_PRINTF("Erase [DONE] in %ums\r\n", sys_get_ms() - begin_tick);
}


static uint8_t flash_check_first_run(void)
{
    uint8_t tmp = 0;
    uint8_t tmp_buff[2];

    flash_read_bytes(FLASH_CHECK_FIRST_RUN, &tmp, 1);

    if (tmp != 0xA5)
    {
        DEBUG_PRINTF("Erase flash\r\n");
        app_flash_erase();

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

uint32_t app_flash_estimate_next_write_addr(void)
{
	bool flash_full = false;
	
	while (1)
	{
		app_flash_data_t tmp;
		memset(&tmp, 0, sizeof(tmp));
		flash_read_bytes(m_wr_addr, (uint8_t*)&tmp, sizeof(tmp));
		if (tmp.valid_flag == APP_FLASH_VALID_DATA_KEY)
		{
			m_wr_addr += sizeof(tmp);
			if (m_wr_addr >= (APP_FLASH_SIZE - 2*sizeof(app_flash_data_t)))
			{
				flash_full = true;
				break;
			}
		}
		else
		{
			break;
		}
	}
	
	if (flash_full)
	{
		m_wr_addr = PAGE_SIZE;
		DEBUG_PRINTF("Flash full, erase first block\r\n");
		flash_erase_sector_4k(0);
		uint8_t tmp_buff[1] = {0xA5};
        flash_write_bytes(FLASH_CHECK_FIRST_RUN, tmp_buff, 1);
	}
	
	// Erase next page
	DEBUG_PRINTF("Write addr 0x%08X\r\n", m_wr_addr);
	return m_wr_addr;
}

uint32_t app_flash_estimate_current_read_addr(void)
{
	#warning "Estimate flash read addr, implement later\r\n");
	return m_read_addr;
}

uint32_t app_flash_dump_all_data(void)
{
	uint8_t read[5];
	flash_read_bytes(0, read, 4);
	read[4] = 0;
	DEBUG_PRINTF("Read data %s\r\n", (char*)read);
	return 0;
}

void app_flash_wakeup(void)
{
    enable_ext_flash(1);
	uint8_t cmd = WB_WAKEUP_CMD;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	enable_ext_flash(0);
	for (uint32_t i = 0; i < 16*3; i++)		// 3us
	{
		__nop();		
	}
}

void app_flash_shutdown(void)
{
	enable_ext_flash(1);
	uint8_t cmd = WB_POWER_DOWN_CMD;
	HAL_SPI_Transmit(&EXT_FLASH_HSPI, &cmd, 1, 100);
	enable_ext_flash(0);
}

bool app_flash_is_error(void)
{
    return m_flash_inited ? false : true;
}
