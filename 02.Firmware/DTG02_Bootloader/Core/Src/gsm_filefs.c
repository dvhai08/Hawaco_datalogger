#include "gsm.h"
#include "gsm_http.h"
#include "gsm_utilities.h"
#include "app_debug.h"
#include <stdio.h>
#include <string.h>
#include "gsm.h"
#include "main.h"
#include "ota_update.h"
#include "hardware.h"
#include "flash_if.h"
#define OTA_FILE_NAME        "UFS:ota.bin"

static uint32_t m_total_bytes_recv = 0;
static uint32_t m_file_size = 0;
static char m_http_cmd_buffer[256];
static int32_t m_file_step = 0;
static int32_t m_file_handle = -1;

void gsm_file_fs_read_ota_firmware(gsm_response_event_t event, void *response_buffer)
{
    DEBUG_INFO("Download big file step %u, result %s\r\n", 
						m_file_step,
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
    if (event != GSM_EVENT_OK)
    {
        DEBUG_ERROR("List file failed\r\n");
        NVIC_SystemReset();
    }
  
    if (m_file_step == 0)
    {
        // Get file size
        //+QFLST: "UFS:F_M12-1.bmp",562554
        char *filename = strstr((char*)response_buffer, OTA_FILE_NAME);
        if (filename == NULL)
        {
            DEBUG_ERROR("File name error\r\n");
            NVIC_SystemReset();
        }
        filename += strlen(OTA_FILE_NAME)  + 2;      // Skip ",
        m_file_size = gsm_utilities_get_number_from_string(0, filename);
        DEBUG_INFO("Image size %u bytes\r\n", m_file_size);
        ota_update_start(m_file_size);
        
        sprintf(m_http_cmd_buffer, "AT+QFOPEN=\"%s\",2\r\n", OTA_FILE_NAME);
        gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                            "+QFOPEN:", 
                            "OK\r\n", 
                            2000, 
                            1, 
                            gsm_file_fs_read_ota_firmware);
    }
    else if (m_file_step == 1)     // Seek file
    {
		gsm_utilities_parse_file_handle((char*)response_buffer, &m_file_handle);
        DEBUG_INFO("File handle %s\r\n", (char*)response_buffer);
		
		if (m_file_handle != -1)
		{
			sprintf(m_http_cmd_buffer, "AT+QFSEEK=%u,%u,0\r\n", m_file_handle, m_total_bytes_recv);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"OK\r\n",
								"",
								2000, 
								1, 
								gsm_file_fs_read_ota_firmware);
		}
		else
		{
			m_total_bytes_recv = 0;
            m_file_step = 0;
			m_file_handle = -1;
            DEBUG_ERROR("File handle failed %d\r\n", m_file_handle);
            NVIC_SystemReset();
            return;
		}
    }
    else if (m_file_step == 2)    // Read file
    {
		sprintf(m_http_cmd_buffer, "AT+QFREAD=%u,%u\r\n", m_file_handle, 128);
        gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                            "CONNECT ",
                            "OK\r\n",
                            2000, 
                            1, 
                            gsm_file_fs_read_ota_firmware);
    }
    else if(m_file_step == 3)
    {
		uint8_t *content;
		uint32_t size;
		gsm_utilities_get_qfile_content(response_buffer, &content, &size);
			
		DEBUG_INFO("Data size %u bytes\r\n", size);
        LED1_TOGGLE();
        ota_update_write_next(content, size);
        
        m_total_bytes_recv += size;
        if (m_total_bytes_recv >= m_file_size)
        {
			DEBUG_VERBOSE("Closing file\r\n");
			sprintf(m_http_cmd_buffer, "AT+QFCLOSE=%u\r\n", m_file_handle);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"OK\r\n",
								"",
								2000, 
								1, 
								gsm_file_fs_read_ota_firmware);
        }
		else
		{
			m_file_step = 2;
			sprintf(m_http_cmd_buffer, "AT+QFREAD=%u,%u\r\n", m_file_handle, 128);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"CONNECT",
								"OK\r\n",
								2000, 
								1, 
								gsm_file_fs_read_ota_firmware);
    
		}
    }
	else if (m_file_step == 4)
	{
		DEBUG_INFO("All data received\r\n");
        if (ota_update_commit_flash())
        {   
            ota_flash_cfg_t new_cfg;
            new_cfg.flag = OTA_FLAG_NO_NEW_FIRMWARE; // OTA_FLAG_UPDATE_NEW_FW;
            new_cfg.firmware_size = m_file_size;
            new_cfg.reserve[0] = 0;
            flash_if_write_ota_info_page((uint32_t*)&new_cfg, sizeof(ota_flash_cfg_t)/sizeof(uint32_t));
        }
        NVIC_SystemReset();
		return;
	}

    m_file_step++;
}

void gsm_file_fs_start(void)
{
    gsm_hw_send_at_cmd("AT+QFLST=\"*\"\r\n", 
                        "QFLST",
                        "OK\r\n",
                        8000, 
                        2, 
                        gsm_file_fs_read_ota_firmware);
}


