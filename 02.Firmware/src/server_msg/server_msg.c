#include "server_msg.h"
#include "string.h"
#include "gsm_utilities.h"
#include "app_bkup.h"
#include "gsm.h"
#include "utilities.h"
#include "app_eeprom.h"
#include "hardware.h"
#include "version_control.h"
#include "app_debug.h"
#include "app_spi_flash.h"
#include "app_rtc.h"
#include "measure_input.h"

#define RS485_STRCAT_ID(str,id)				str##id##"\":"		

void server_msg_process_cmd(char *buffer, uint8_t *new_config)
{
    uint8_t has_new_cfg = 0;
    uint8_t rewrite_data_to_flash = 0;	
	app_eeprom_config_data_t *config = app_eeprom_read_config_data();
    sys_ctx_t *ctx = sys_ctx();
    ctx->status.disconnected_count = 0;
	
    char *cycle_wakeup = strstr(buffer, "Cyclewakeup\":");
    if (cycle_wakeup != NULL)
    {
        uint32_t wake_time_measure_data_ms = 1000*60*gsm_utilities_get_number_from_string(strlen("Cyclewakeup\":"), cycle_wakeup);
        if (config->measure_interval_ms != wake_time_measure_data_ms)
        {
            config->measure_interval_ms = wake_time_measure_data_ms;
            has_new_cfg++;
        }
    }

    char *cycle_send_web = strstr(buffer, "CycleSendWeb\":");
    if (cycle_send_web != NULL)
    {
        uint32_t send_time_ms = 1000*60*gsm_utilities_get_number_from_string(strlen("CycleSendWeb\":"), cycle_send_web);
        if (config->send_to_server_interval_ms != send_time_ms)
        {
            DEBUG_INFO("CycleSendWeb changed\r\n");
            config->send_to_server_interval_ms = send_time_ms;
            has_new_cfg++;
        }
    }
    char *output1 = strstr(buffer, "Output1\":");
    if (output1 != NULL)
    {
        uint8_t out1 = gsm_utilities_get_number_from_string(strlen("Output1\":"), output1) & 0x1;
        if (config->io_enable.name.output0 != out1)
        {
            config->io_enable.name.output0 = out1;
            has_new_cfg++;
            DEBUG_PRINTF("4-20MA output1 1 changed\r\n");
        }
    }
#ifdef DTG02
    char *output2 = strstr(buffer, "Output2\":");
    if (output2 != NULL)
    {
        uint8_t out2 = gsm_utilities_get_number_from_string(strlen("Output2\":"), output2) ? 1 : 0;
        if (config->io_enable.name.output1 != out2)
        {
            config->io_enable.name.output1 = out2;
            has_new_cfg++;
            DEBUG_PRINTF("Output 2 changed\r\n");
			TRANS_2_OUTPUT(out2);
        }
    }
	

    char *output3 = strstr(buffer, "Output3\":");
    if (output3 != NULL)
    {
        uint8_t out3 = gsm_utilities_get_number_from_string(strlen("Output3\":"), output3) ? 1 : 0;
        if (config->io_enable.name.output2 != out3)
        {
            config->io_enable.name.output2 = out3;
            has_new_cfg++;
            DEBUG_PRINTF("Output 3 changed\r\n");
			TRANS_3_OUTPUT(out3);
        }
    }

	char *output4 = strstr(buffer, "Output4\":");
    if (output4 != NULL)
    {
        uint8_t out4 = gsm_utilities_get_number_from_string(strlen("Output4\":"), output4) ? 1 : 0;
        if (config->io_enable.name.output3 != out4)
        {
            config->io_enable.name.output3 = out4;
            has_new_cfg++;
            DEBUG_PRINTF("Output 4 changed\r\n");
			TRANS_4_OUTPUT(out4);
        }
    }
    
    char *output4_20mA = strstr(buffer, "Output4_20\":");
    if (output4_20mA != NULL)
    {
        char output_value[16];
        memset(output_value, 0, sizeof(output_value));
        
        gsm_utilities_copy_parameters(output4_20mA+strlen("Output4_20\""), output_value, ':', ',');
        DEBUG_INFO("Output 4-20ma %s\r\n", output_value);
        float new_value = atof(output_value);
//        uint32_t interger = (uint32_t)new_value;
//        uint32_t dec = (uint32_t)((new_value-interger)/10);
//        uint8_t out_4_20 = gsm_utilities_get_number_from_string(strlen("Output4_20\":"), output4_20mA);
        
        if (config->output_4_20ma != new_value)
        {
            config->io_enable.name.output_4_20ma_enable = 1;
            config->output_4_20ma = new_value;
            has_new_cfg++;
            DEBUG_INFO("Output 4-20ma changed to %.2f\r\n", config->output_4_20ma);
        }
        
        if (config->output_4_20ma < 4.0f
            || config->output_4_20ma > 20.0f)
        {
            config->io_enable.name.output_4_20ma_enable = 0;
        }
    }
#endif
	
#ifdef DTG01
    char *mode_j1 = strstr(buffer, "InputMode0\":");		// mode
    if (mode_j1 != NULL)
    {
        uint8_t mode = gsm_utilities_get_number_from_string(strlen("InputMode0\":"), mode_j1);
        if (config->meter_mode[0] != mode)
        {
            DEBUG_PRINTF("PWM1 mode changed\r\n");
            config->meter_mode[0] = mode;
            has_new_cfg++;
        }
    }
    
    char *output2 = strstr(buffer, "Output2\":");
    if (output2 != NULL)
    {
        char output_value[16];
        memset(output_value, 0, sizeof(output_value));
        
        gsm_utilities_copy_parameters(output2+strlen("Output2\""), output_value, ':', ',');
        DEBUG_INFO("Output 4-20ma %s\r\n", output_value);
        float new_value = atof(output_value);
        if (config->output_4_20ma != new_value)
        {
            config->io_enable.name.output_4_20ma_enable = 1;
            config->output_4_20ma = new_value;
            has_new_cfg++;
            DEBUG_INFO("Output 4-20ma changed to %.2f\r\n", config->output_4_20ma);
        }
        
        if (config->output_4_20ma < 4.0f
            || config->output_4_20ma > 20.0f)
        {
            config->io_enable.name.output_4_20ma_enable = 0;
        }
    }
#else
    char *mode_j1 = strstr(buffer, "Input_J1\":");		// mode
    if (mode_j1 != NULL)
    {
        uint8_t mode = gsm_utilities_get_number_from_string(strlen("Input_J1\":"), mode_j1);
        if (config->meter_mode[0] != mode)
        {
            DEBUG_PRINTF("PWM1 mode changed\r\n");
            config->meter_mode[0] = mode;
            has_new_cfg++;
        }
    }
    
    char *mode_j2 = strstr(buffer, "Input_J2\":");
    if (mode_j2 != NULL)
    {
        uint8_t mode = gsm_utilities_get_number_from_string(strlen("Input_J2\":"), mode_j2);
        if (config->meter_mode[1] != mode)
        {
            DEBUG_PRINTF("PWM2 mode changed\r\n");
            config->meter_mode[1] = mode;
            has_new_cfg++;
        }
    }
#endif
    
    char *rs485_ptr = strstr(buffer, "RS485\":");
    if (rs485_ptr != NULL)
    {
        uint8_t in485 = gsm_utilities_get_number_from_string(strlen("RS485\":"), rs485_ptr) ? 1 : 0;
        if (config->io_enable.name.rs485_en != in485)
        {
            DEBUG_PRINTF("in485 changed\r\n");
            config->io_enable.name.rs485_en = in485;
            has_new_cfg++;
        }
    }

    char *alarm = strstr(buffer, "Warning\":");
    if (alarm != NULL)
    {
        uint8_t alrm = gsm_utilities_get_number_from_string(strlen("Warning\":"), alarm) ? 1 : 0;
        if (config->io_enable.name.warning != alrm)
        {
            DEBUG_PRINTF("Warning changed\r\n");
            config->io_enable.name.warning = alrm;
            has_new_cfg++;
        }
    }


    char *phone_num = strstr(buffer, "SOS\":");
    if (phone_num != NULL)
    {
        phone_num += strlen("SOS\":");
        char tmp_phone[30] = {0};
        if (gsm_utilities_copy_parameters(phone_num, tmp_phone, '"', '"'))
        {
#if 1
            uint8_t changed = 0;
			int len = strlen(tmp_phone);
			if (len > 15)
			{
				len = 15;
			}
			
            for(uint8_t i = 0; i < len; i++)
            {
                if(tmp_phone[i] != config->phone[i]
                    && changed == 0) 
                {
                    changed = 1;
                    has_new_cfg++;
                }
                config->phone[i] = tmp_phone[i];
            }
			
			config->phone[15] = 0;
            if (changed)
            {
                DEBUG_PRINTF("Phone changed to %s\r\n", config->phone);
            }
#endif
        }
    }
#ifdef DTG02
    char *counter_offset = strstr(buffer, "MeterIndicator_J1\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("MeterIndicator_J1\":"), counter_offset);
        if (config->offset[0] != offset)
        {
            has_new_cfg++;   
            config->offset[0] = offset;
            DEBUG_WARN("PWM 1 offset changed to %u\r\n", offset);
            measure_input_reset_counter(0);
			measure_input_counter_t counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
			app_bkup_read_pulse_counter(&counter[0]);
			counter[0].forward = 0;
			counter[0].reserve = 0;
            app_bkup_write_pulse_counter(&counter[0]);
			rewrite_data_to_flash |= (1 << 0);
        }
    }

	counter_offset = strstr(buffer, "MeterIndicator_J2\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("MeterIndicator_J2\":"), counter_offset);
        if (config->offset[1] != offset)
        {
            has_new_cfg++;   
            config->offset[1] = offset;
            DEBUG_PRINTF("PWM2 offset changed to %u\r\n", offset);
            measure_input_reset_counter(1);
			measure_input_counter_t counter[MEASURE_NUMBER_OF_WATER_METER_INPUT];
			counter[1].forward = 0;
			counter[1].reserve = 0;
            app_bkup_write_pulse_counter(&counter[0]);
			rewrite_data_to_flash |= (1 << 0);
        }
    }
	
	char *k_factor = strstr(buffer, "K_J1\":");
    if (k_factor)
    {
        uint32_t k = gsm_utilities_get_number_from_string(strlen("K_J1\":"), k_factor);
        if (k == 0)
        {
            k = 1;
        }

        if (config->k[0] != k)
        {
            config->k[0] = k;
            DEBUG_PRINTF("K0 factor changed to %u\r\n", k);
//            measure_input_reset_all_counter();
            has_new_cfg++; 
        }
    }

    k_factor = strstr(buffer, "K_J2\":");
	if (k_factor)
    {
        uint32_t k = gsm_utilities_get_number_from_string(strlen("K_J2\":"), k_factor);
        if (k == 0)
        {
            k = 1;
        }

        if (config->k[1] != k)
        {
            config->k[1] = k;
            DEBUG_PRINTF("K1 factor changed to %u\r\n", k);
//            measure_input_reset_all_counter();
            has_new_cfg++; 
        }
    }
#else
    char *counter_offset = strstr(buffer, "MeterIndicator\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("MeterIndicator\":"), counter_offset);
        if (config->offset0 != offset)
        {
            has_new_cfg++;   
            config->offset0 = offset;
            DEBUG_PRINTF("PWM1 offset changed to %u\r\n", offset);
			uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
			app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
            app_bkup_write_pulse_counter(0, 0, counter0_r, counter1_r);
            rewrite_data_to_flash |= (1 << 0);
        }
    }
	
	char *k_factor = strstr(buffer, "K\":");
    if (k_factor)
    {
        uint32_t k = gsm_utilities_get_number_from_string(strlen("K\":"), k_factor);
        if (k == 0)
        {
            k = 1;
        }

        if (config->k0 != k)
        {
            config->k0 = k;
            DEBUG_PRINTF("K0 changed to %u\r\n", k);
            has_new_cfg++; 
        }
    }
#endif
	
    char *p_delay = strstr(buffer, "Delay\":");
    if (p_delay)
    {
        uint32_t delay = gsm_utilities_get_number_from_string(strlen("Delay\":"), p_delay);
        config->send_to_server_delay_s = delay;
        DEBUG_INFO("Delay changed to %us\r\n", delay);
        has_new_cfg++;
    }
    
    char *server = strstr(buffer, "Server\":");
    if (server)
    {
        server += strlen("Server\":");
        uint8_t tmp[APP_EEPROM_MAX_SERVER_ADDR_LENGTH];
        if (gsm_utilities_copy_parameters(server, (char*)tmp, '"', '"'))
        {
            has_new_cfg++;
            snprintf((char*)config->server_addr, APP_EEPROM_MAX_SERVER_ADDR_LENGTH - 1, "%s", (char*)tmp);
            DEBUG_INFO("Server changed to %s\r\n", config->server_addr);
        }
    }
	bool same_hardware = false;
    char *hardware = strstr(buffer, "Hardware\":");  
	if (hardware)
	{
		char tmp_hw[16];
		memset(tmp_hw, 0, sizeof(tmp_hw));
		hardware += strlen("Hardware\":");  
		if (gsm_utilities_copy_parameters(hardware, tmp_hw, '"', '"'))
		{
			if (strcmp(tmp_hw, VERSION_CONTROL_HW) == 0)
			{
				same_hardware = true;
			}
		}
	}
	char *do_ota = NULL;
	char *version = NULL;
	char *link = NULL;
	
	if (same_hardware)
	{
		do_ota = strstr(buffer, "Update\":");
		version = strstr(buffer, "Version\":\"");
		link = strstr(buffer, "Link\":");
	}

    if (same_hardware && do_ota && version && link)
    {
		version += strlen("Version\":\"");
		link += strlen("Link\":");
		
        uint32_t update = gsm_utilities_get_number_from_string(strlen("Update\":"), do_ota);
        if (update)
        {
			DEBUG_VERBOSE("Server request device to ota update, current fw version %s\r\n", VERSION_CONTROL_FW);
			uint8_t version_compare;
			version = strtok(version, "\"");
			version_compare = version_control_compare(version);
			if (version_compare == VERSION_CONTROL_FW_NEWER)
			{
				link = strtok(link, "\"");
				if (link && strlen(link) && strstr(link, "http"))
				{
                    ctx->status.delay_ota_update = 5;
                    ctx->status.enter_ota_update = true;
                    sprintf((char*)ctx->status.ota_url, "%s", strstr(link, "http"));
				}
			}
			else
			{
				DEBUG_VERBOSE("Invalid fw version\r\n");
			}
        }
    }
    
    // Process RS485
    // Total 2 device
    // "ID485_0":1,
	// Register_1_1:30001
	// Unit_1_1:"kg/m3",
	// Type_1_1:"int16/int32/float",
	// Register_1_3:30003,
	// 
	//"ID485_1":1,
	// Register_2_1:40001
	//  Register_2_3:40003
	
	for (uint32_t slave_count = 0; slave_count < RS485_MAX_SLAVE_ON_BUS && config->io_enable.name.rs485_en; slave_count++)
	{
		char search_str[16];
		sprintf(search_str, "ID485_%u\":", slave_count+1);
		char *rs485_id_str = strstr(buffer, search_str);
		
		for (uint32_t sub_reg_idx = 0; sub_reg_idx < RS485_MAX_SUB_REGISTER; sub_reg_idx++)
		{			
			sprintf(search_str, "Register_%u_%u\":", slave_count+1, sub_reg_idx+1);
			char *rs485_reg_str = strstr(buffer, search_str);
			
			sprintf(search_str, "Type_%u_%u\":", slave_count+1, sub_reg_idx+1);
			char *rs485_type_str = strstr(buffer, search_str);
			
			sprintf(search_str, "Unit_%u_%u\":", slave_count+1, sub_reg_idx+1);
			char *rs485_unit = strstr(buffer, search_str);
			
			if (rs485_id_str && rs485_reg_str && rs485_type_str)
			{
				uint32_t temp;
				// Get rs485 slave id
				temp = gsm_utilities_get_number_from_string(9, rs485_id_str);  //7 = strlen(ID485_1":)
				if (temp != config->rs485[slave_count].slave_addr)
				{
					config->rs485[slave_count].slave_addr = temp;
					has_new_cfg++;
					DEBUG_INFO("Slave addr %u\r\n", temp);
				}
				
				// Get RS485 data type
				temp = RS485_DATA_TYPE_INT16;
				config->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.valid = 1;
				if (strstr(rs485_type_str, "\"int32\","))
				{
					temp = RS485_DATA_TYPE_INT32;
				}
				else if (strstr(rs485_type_str, "\"float\","))
				{
					temp = RS485_DATA_TYPE_FLOAT;
				}
				
				if (temp != config->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.type)
				{
					config->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.type = temp;
					has_new_cfg++;
					DEBUG_INFO("Type %u\r\n", temp);
				}
				
				// Get RS485 data addr 
				temp = gsm_utilities_get_number_from_string(14, rs485_reg_str);  //14 = strlen(Register_2_1":)
				if (temp != config->rs485[slave_count].sub_register[sub_reg_idx].register_addr)
				{
					config->rs485[slave_count].sub_register[sub_reg_idx].register_addr = temp;
					has_new_cfg++;
					DEBUG_INFO("Reg addr %u\r\n", temp);
				}
				
				// Unit
				if (rs485_unit == NULL)
				{
					config->rs485[slave_count].sub_register[sub_reg_idx].unit[0] = 0;
				}
				else
				{
					// strlen("\"Unit_%u_%u\":\"") = 11
					rs485_unit += 11;
					uint32_t copy_len = APP_EEPROM_MAX_UNIT_NAME_LENGTH - 1;
					char *p = strstr(rs485_unit, "\"");
					if (p - rs485_unit < copy_len)
					{
						copy_len = p - rs485_unit;
					}
					if (strstr(rs485_unit, (char*)config->rs485[slave_count].sub_register[sub_reg_idx].unit) == 0)
					{
						has_new_cfg++;
					}
					strncpy((char*)config->rs485[slave_count].sub_register[sub_reg_idx].unit, (char*)rs485_unit, copy_len); 
					DEBUG_INFO("Unit %s\r\n", (char*)config->rs485[slave_count].sub_register[sub_reg_idx].unit);
				}
			}
		}
	}
	
    //Luu config moi
    if (has_new_cfg)
    {
        DEBUG_INFO("Save eeprom config\r\n");
 		app_eeprom_save_config();
    }
    else
    {
        DEBUG_INFO("CFG: has no new config\r\n");
    }
    
    if (rewrite_data_to_flash)
    {
        app_spi_flash_data_t data;
        if (app_spi_flash_get_lastest_data(&data))
        {
            if (rewrite_data_to_flash & 0x01)		// 0x01 mean we need to store new data of pulse counter[0] to eeprom
            {
                data.meter_input[0].forward = 0;
                data.meter_input[0].reserve = 0;
            }
#ifdef DTG02
            if (rewrite_data_to_flash & 0x02)		// 0x01 mean we need to store new data of pulse counter[1] to eeprom
            {
                data.meter_input[1].forward = 0;
                data.meter_input[1].reserve = 0;
            }
#endif
            data.timestamp = app_rtc_get_counter();
            DEBUG_INFO("Save new config to flash\r\n");
            app_spi_flash_write_data(&data);
        }
    }

    *new_config = has_new_cfg;
}
