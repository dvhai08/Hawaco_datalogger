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

void server_msg_process_cmd(char *buffer, uint8_t *new_config)
{
    uint8_t has_new_cfg = 0;
//    utilities_to_upper_case(buffer);
	
	app_eeprom_config_data_t *config = app_eeprom_read_config_data();

	
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
            DEBUG_PRINTF("CycleSendWeb changed\r\n");
            config->send_to_server_interval_ms = send_time_ms;
            has_new_cfg++;
        }
    }
#ifdef DTG02
    char *output1 = strstr(buffer, "Output1\":");
    if (output1 != NULL)
    {
        uint8_t out1 = gsm_utilities_get_number_from_string(strlen("Output1\":"), output1) & 0x1;
        if (config->io_enable.name.output0 != out1)
        {
            config->io_enable.name.output0 = out1;
            has_new_cfg++;
            DEBUG_PRINTF("4-20MA output1 1 changed\r\n");
            //Dk ngoai vi luon
			#warning "Please control output 1 4-20mA with delay"
        }
    }

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
#endif
	
    char *mode_j1 = strstr(buffer, "Input_j1\":");		// mode
    if (mode_j1 != NULL)
    {
        uint8_t mode = gsm_utilities_get_number_from_string(strlen("Input_j1\":"), mode_j1);
        if (config->meter_mode[0] != mode)
        {
            DEBUG_PRINTF("PWM1 mode changed\r\n");
            config->meter_mode[0] = mode;
            has_new_cfg++;
        }
    }

    char *mode_j2 = strstr(buffer, "Input_j2\":");
    if (mode_j2 != NULL)
    {
        uint8_t mode = gsm_utilities_get_number_from_string(strlen("Input_j2\":"), mode_j2);
        if (config->meter_mode[1] != mode)
        {
            DEBUG_PRINTF("PWM2 mode changed\r\n");
            config->meter_mode[1] = mode;
            has_new_cfg++;
        }
    }

    char *rs485 = strstr(buffer, "RS485\":");
    if (rs485 != NULL)
    {
        uint8_t in485 = gsm_utilities_get_number_from_string(strlen("RS485\":"), rs485) ? 1 : 0;
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
                    DEBUG_PRINTF("Phone number changed to %s\r\n", config->phone);
            }
#endif
        }
    }
#ifdef DTG02
    char *counter_offset = strstr(buffer, "MeterIndicator_J1\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("MeterIndicator_J1\":"), counter_offset);
        if (config->offset0 != offset)
        {
            has_new_cfg++;   
            config->offset0 = offset;
            DEBUG_PRINTF("PWM dir 1 offset changed to %u\r\n", offset);
			uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
			app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
            app_bkup_write_pulse_counter(0, 0, counter0_r, counter1_r);
			#warning "Please write current measurement data to flash"
        }
    }

	counter_offset = strstr(buffer, "MeterIndicator_J2\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("MeterIndicator_J2\":"), counter_offset);
        if (config->offset1 != offset)
        {
            has_new_cfg++;   
            config->offset1 = offset;
            DEBUG_PRINTF("PWM dir 2 offset changed to %u\r\n", offset);
			uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
			app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
            app_bkup_write_pulse_counter(counter0_f, counter0_r, 0, 0);
			#warning "Please write current measurement data to flash"
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

        if (config->k0 != k)
        {
            config->k0 = k;
            DEBUG_PRINTF("K0 factor changed to %u\r\n", k);
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

        if (config->k1 != k)
        {
            config->k1 = k;
            DEBUG_PRINTF("K1 factor changed to %u\r\n", k);
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
            DEBUG_PRINTF("PWM dir 1 offset changed to %u\r\n", offset);
			uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
			app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
            app_bkup_write_pulse_counter(0, 0, counter0_r, counter1_r);
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
            DEBUG_PRINTF("K0 factor changed to %u\r\n", k);
            has_new_cfg++; 
        }
    }
#endif
	
	char *do_ota = strstr(buffer, "Update\":");
	char *version = strstr(buffer, "version\":\"");
	char *link = strstr(buffer, "Link\":");
    if (do_ota && version && link)
    {
		version += strlen("Version\":\"");
		link += strlen("Link\":");
		
        uint32_t update = gsm_utilities_get_number_from_string(strlen("Update\":"), do_ota);
        if (update)
        {
			DEBUG_PRINTF("Server request device to ota update, current fw version %s\r\n", VERSION_CONTROL_FW);
			uint8_t version_compare;
			version = strtok(version, "\"");
			version_compare = version_control_compare(version);
			if (version_compare == VERSION_CONTROL_FW_NEWER)
			{
				link = strtok(link, "\"");
				if (link && strlen(link) && strstr(link, "http"))
				{
                    sys_ctx()->status.delay_ota_update = 5;
                    sys_ctx()->status.enter_ota_update = true;
                    sprintf((char*)sys_ctx()->status.ota_url, "%s", strstr(link, "http"));
				}
			}
			else
			{
				DEBUG_PRINTF("Invalid fw version\r\n");
			}
        }
    }
	
    //Luu config moi
    if (has_new_cfg)
    {
 		app_eeprom_save_config();
    }
    else
    {
        DEBUG_PRINTF("CFG: has no new config\r\n");
    }

    *new_config = has_new_cfg;
}
