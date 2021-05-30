#include "server_msg.h"
#include "string.h"
#include "DataDefine.h"
#include "gsm_utilities.h"

extern System_t xSystem;

void server_msg_process_cmd(char *buffer)
{
    uint8_t hasNewConfig = 0;

    char *cycleWake = strstr(buffer, "CYCLEWAKEUP(");
    if (cycleWake != NULL)
    {
        uint16_t wakeTime = gsm_utilities_get_number_from_string(12, cycleWake);
        if (xSystem.Parameters.TGDoDinhKy != wakeTime)
        {
            xSystem.Parameters.TGDoDinhKy = wakeTime;
            hasNewConfig++;
        }
    }

    char *cycleSend = strstr(buffer, "CYCLESENDWEB(");
    if (cycleSend != NULL)
    {
        uint16_t sendTime = gsm_utilities_get_number_from_string(13, cycleSend);
        if (xSystem.Parameters.TGGTDinhKy != sendTime)
        {
            DEBUG_PRINTF("CYCLESENDWEB changed\r\n");
            xSystem.Parameters.TGGTDinhKy = sendTime;
            hasNewConfig++;
        }
    }

    char *output1 = strstr(buffer, "OUTPUT1(");
    if (output1 != NULL)
    {
        uint8_t out1 = gsm_utilities_get_number_from_string(8, output1) & 0x1;
        if (xSystem.Parameters.outputOnOff != out1)
        {
            xSystem.Parameters.outputOnOff = out1;
            hasNewConfig++;
            DEBUG_PRINTF("Output 1 changed\r\n");
            //Dk ngoai vi luon
            TRAN_OUTPUT(out1);
        }
    }

    char *output2 = strstr(buffer, "OUTPUT2(");
    if (output2 != NULL)
    {
        uint8_t out2 = gsm_utilities_get_number_from_string(8, output2);
        if (xSystem.Parameters.output420ma != out2)
        {
            DEBUG_PRINTF("Output 2 changed\r\n");
            xSystem.Parameters.output420ma = out2;
            hasNewConfig++;

            //Dk ngoai vi luon
            //TRAN_OUTPUT(out2);
        }
    }

    char *input1 = strstr(buffer, "INPUT1(");
    if (input1 != NULL)
    {
        uint8_t in1 = gsm_utilities_get_number_from_string(7, input1) & 0x1;
        if (xSystem.Parameters.input.name.pulse != in1)
        {
            DEBUG_PRINTF("INPUT1 changed\r\n");
            xSystem.Parameters.input.name.pulse = in1;
            hasNewConfig++;
        }
    }

    char *input2 = strstr(buffer, "INPUT2(");
    if (input2 != NULL)
    {
        uint8_t in2 = gsm_utilities_get_number_from_string(7, input2) & 0x1;
        if (xSystem.Parameters.input.name.ma420 != in2)
        {
            DEBUG_PRINTF("INPUT2 changed\r\n");
            xSystem.Parameters.input.name.ma420 = in2;
            hasNewConfig++;
        }
    }

    char *rs485 = strstr(buffer, "RS485(");
    if (rs485 != NULL)
    {
        uint8_t in485 = gsm_utilities_get_number_from_string(7, rs485) & 0x1;
        if (xSystem.Parameters.input.name.rs485 != in485)
        {
            DEBUG_PRINTF("in485 changed\r\n");
            xSystem.Parameters.input.name.rs485 = in485;
            hasNewConfig++;
        }
    }

    char *alarm = strstr(buffer, "WARNING(");
    if (alarm != NULL)
    {
        uint8_t alrm = gsm_utilities_get_number_from_string(8, alarm) & 0x1;
        if (xSystem.Parameters.alarm != alrm)
        {
            DEBUG_PRINTF("WARNING changed\r\n");
            xSystem.Parameters.alarm = alrm;
            hasNewConfig++;
        }
    }


    char *phoneNum = strstr(buffer, "PHONENUM(");
    if (phoneNum != NULL)
    {
        char tmpPhone[30] = {0};
        if (gsm_utilities_copy_parameters(phoneNum, tmpPhone, '(', ')'))
        {
#if 0
			uint8_t changed = 0;
			for(uint8_t i = 0; i < 15; i++)
			{
				if(tmpPhone[i] != xSystem.Parameters.PhoneNumber[i]) {
					changed = 1;
					hasNewConfig ++;
				}
				xSystem.Parameters.PhoneNumber[i] = tmpPhone[i];
			}
			if (changed)
			{
				DEBUG_PRINTF("PHONENUM changed\r\n");
			}
#endif
        }
    }

    char *kFactor = strstr(buffer, "K(");
    if (kFactor)
    {
        uint32_t k_factor = gsm_utilities_get_number_from_string(strlen("K("), kFactor);
        if (k_factor == 0)
        {
            k_factor = 1;
        }

        if (xSystem.Parameters.kFactor != k_factor)
        {
            xSystem.Parameters.kFactor = k_factor;
            DEBUG_PRINTF("K Factor %u\r\n", k_factor);
            hasNewConfig++; 
        }
    }

    char *counterOffser = strstr(buffer, "METERINDICATOR(");
    if (counterOffser)
    {
        uint32_t offset = gsm_utilities_get_number_from_string(strlen("METERINDICATOR("), counterOffser);
        if (xSystem.Parameters.input1Offset != offset)
        {
            hasNewConfig++;   
            xSystem.Parameters.input1Offset = offset;
            DEBUG_PRINTF("Input 1 offset %u\r\n", offset);
            xSystem.MeasureStatus.PulseCounterInFlash = 0;
            xSystem.MeasureStatus.PulseCounterInBkup = 0;
            app_bkup_write_pulse_counter(xSystem.MeasureStatus.PulseCounterInBkup);
            InternalFlash_WriteMeasures();
        }
    }


    //Luu config moi
    if (hasNewConfig)
    {
        gsm_set_timeout_to_sleep(10);        // Wait more 5 second
        MqttClientSendFirstMessageWhenWakeup();
        InternalFlash_WriteConfig();
    }
    else
    {
        gsm_set_timeout_to_sleep(5);        // Wait more 5 second
        DEBUG_PRINTF("CFG: has no new config\r\n");
    }
}
