#include "server_msg.h"
#include "string.h"
#include "DataDefine.h"
#include "gsm_utilities.h"
#include "app_bkup.h"
#include "gsm.h"
#include "InternalFlash.h"
#include "utilities.h"

extern System_t xSystem;

void server_msg_process_cmd(char *buffer, uint8_t *new_config)
{
    uint8_t has_new_cfg = 0;
    utilities_to_upper_case(buffer);

    char *cycle_wakeup = strstr(buffer, "CYCLEWAKEUP\":");
    if (cycle_wakeup != NULL)
    {
        uint16_t wakeTime = gsm_utilities_get_number_from_string(strlen("CYCLEWAKEUP\":"), cycle_wakeup);
        if (xSystem.Parameters.period_measure_peripheral != wakeTime)
        {
            xSystem.Parameters.period_measure_peripheral = wakeTime;
            has_new_cfg++;
        }
    }

    char *cycle_send_web = strstr(buffer, "CYCLESENDWEB\":");
    if (cycle_send_web != NULL)
    {
        uint16_t sendTime = gsm_utilities_get_number_from_string(strlen("CYCLESENDWEB\":"), cycle_send_web);
        if (xSystem.Parameters.period_send_message_to_server_min != sendTime)
        {
            DEBUG_PRINTF("CYCLESENDWEB changed\r\n");
            xSystem.Parameters.period_send_message_to_server_min = sendTime;
            //#warning "Turn off cycle send webchanged"
            has_new_cfg++;
        }
    }

    char *output1 = strstr(buffer, "OUTPUT1\":");
    if (output1 != NULL)
    {
        uint8_t out1 = gsm_utilities_get_number_from_string(strlen("OUTPUT1\":"), output1) & 0x1;
        if (xSystem.Parameters.outputOnOff != out1)
        {
            xSystem.Parameters.outputOnOff = out1;
            has_new_cfg++;
            DEBUG_PRINTF("Output 1 changed\r\n");
            //Dk ngoai vi luon
            TRAN_OUTPUT(out1);
        }
    }

    char *output2 = strstr(buffer, "OUTPUT2\":");
    if (output2 != NULL)
    {
        uint8_t out2 = gsm_utilities_get_number_from_string(strlen("OUTPUT2\":"), output2);
        if (xSystem.Parameters.output420ma != out2)
        {
            DEBUG_PRINTF("Output 2 changed\r\n");
            xSystem.Parameters.output420ma = out2;
            has_new_cfg++;

            //Dk ngoai vi luon
            //TRAN_OUTPUT(out2);
        }
    }

    char *input1 = strstr(buffer, "INPUT1\":");
    if (input1 != NULL)
    {
        uint8_t in1 = gsm_utilities_get_number_from_string(strlen("INPUT1\":"), input1) & 0x1;
        if (xSystem.Parameters.input.name.pulse != in1)
        {
            DEBUG_PRINTF("INPUT1 changed\r\n");
            xSystem.Parameters.input.name.pulse = in1;
            has_new_cfg++;
        }
    }

    char *input2 = strstr(buffer, "INPUT2\":");
    if (input2 != NULL)
    {
        uint8_t in2 = gsm_utilities_get_number_from_string(strlen("INPUT2\":"), input2) & 0x1;
        if (xSystem.Parameters.input.name.ma420 != in2)
        {
            DEBUG_PRINTF("INPUT2 changed\r\n");
            xSystem.Parameters.input.name.ma420 = in2;
            has_new_cfg++;
        }
    }

    char *rs485 = strstr(buffer, "RS485\":");
    if (rs485 != NULL)
    {
        uint8_t in485 = gsm_utilities_get_number_from_string(strlen("RS485\":"), rs485) & 0x1;
        if (xSystem.Parameters.input.name.rs485 != in485)
        {
            DEBUG_PRINTF("in485 changed\r\n");
            xSystem.Parameters.input.name.rs485 = in485;
            has_new_cfg++;
        }
    }

    char *alarm = strstr(buffer, "WARNING\":");
    if (alarm != NULL)
    {
        uint8_t alrm = gsm_utilities_get_number_from_string(strlen("WARNING\":"), alarm) & 0x1;
        if (xSystem.Parameters.alarm_enable != alrm)
        {
            DEBUG_PRINTF("WARNING changed\r\n");
            xSystem.Parameters.alarm_enable = alrm;
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
            for(uint8_t i = 0; i < 15; i++)
            {
                if(tmp_phone[i] != xSystem.Parameters.phone_number[i]
                    && changed == 0) 
                {
                        changed = 1;
                        has_new_cfg++;
                }
                xSystem.Parameters.phone_number[i] = tmp_phone[i];
            }
            if (changed)
            {
                    DEBUG_PRINTF("PHONENUM changed\r\n");
            }
#endif
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

        if (xSystem.Parameters.kFactor != k)
        {
            xSystem.Parameters.kFactor = k;
            DEBUG_PRINTF("K Factor %u\r\n", k);
            has_new_cfg++; 
        }
    }

    char *counter_offset = strstr(buffer, "METERINDICATOR\":");
    if (counter_offset)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("METERINDICATOR\":"), counter_offset);
        if (xSystem.Parameters.input1Offset != offset)
        {
            has_new_cfg++;   
            xSystem.Parameters.input1Offset = offset;
            DEBUG_PRINTF("Input 1 offset %u\r\n", offset);
            xSystem.MeasureStatus.PulseCounterInFlash = 0;
            xSystem.MeasureStatus.PulseCounterInBkup = 0;
            app_bkup_write_pulse_counter(xSystem.MeasureStatus.PulseCounterInBkup);
            InternalFlash_WriteMeasures();
        }
    }


    //Luu config moi
    if (has_new_cfg)
    {
        gsm_set_timeout_to_sleep(10);        // Wait more 5 second
        //MqttClientSendFirstMessageWhenWakeup();
        InternalFlash_WriteConfig();
    }
    else
    {
        gsm_set_timeout_to_sleep(5);        // Wait more 5 second
        DEBUG_PRINTF("CFG: has no new config\r\n");
    }

    *new_config = has_new_cfg;
}
