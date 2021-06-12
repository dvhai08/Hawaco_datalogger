#ifndef __GSM_H__
#define __GSM_H__

#include "DataDefine.h"
#include <stdbool.h>

#define GSM_ON 1
#define GSM_OFF 2

#define GSMIMEI 0
#define SIMIMEI 1

#define MAX_GSMRESETSYSTEMCOUNT 600 //10 phut

#define __HOPQUY_GSM__ 0
#define __GSM_SLEEP_MODE__ 1
#define __USED_HTTP__ 0
#define __GSM_SMS_ENABLE__ 0

typedef enum
{
    GSM_EVENT_OK = 0,  // GSM response dung
    GSM_EVENT_TIMEOUT, // Het timeout ma chua co response
    GSM_EVENT_ERROR,   // GSM response ko dung
} gsm_response_event_t;

typedef enum
{
    GSM_STATE_OK = 0,
    GSM_STATE_RESET = 1,
    GSM_STATE_SEND_SMS = 2,
    GSM_STATE_READ_SMS = 3,
    GSM_STATE_POWER_ON = 4,
    GSM_STATE_REOPEN_PPP = 5,
    GSM_STATE_GET_BTS_INFO = 6,
    GSM_STATE_SEND_ATC = 7,
    GSM_STATE_GOTO_SLEEP = 8,
    GSM_STATE_WAKEUP = 9,
    GSM_STATE_AT_MODE_IDLE,
    GSM_STATE_SLEEP,
    GSM_STATE_HTTP_GET,
    GSM_STATE_HTTP_POST,
} gsm_state_t;

typedef enum
{
    GSM_AT_MODE = 1,
    GSM_PPP_MODE
} gsm_at_mode_t;

typedef enum
{
    GSM_INTERNET_MODE_PPP_STACK,
    GSM_INTERNET_MODE_AT_STACK
} gsm_internet_mode_t;

typedef struct
{
    gsm_state_t state;
    uint8_t step;
    uint8_t RISignal;
    uint8_t Dial;
    uint8_t GetBTSInfor;
    uint8_t GSMReady;
    uint8_t FirstTimePower;
    uint8_t SendSMSAfterRead;
    uint8_t ppp_cmd_state;
    uint16_t TimeOutConnection;
    uint16_t TimeOutCSQ;
    uint8_t TimeOutOffAfterReset;
    uint8_t isGSMOff;
    uint8_t access_tech;
} GSM_Manager_t;

typedef struct
{
    char phone_number[15];
    char message[160];
    uint8_t need_to_send;
    uint8_t retry_count;
} gsm_sms_msg_t;


typedef void (*gsm_send_at_cb_t)(gsm_response_event_t event, void *ResponseBuffer);

void gsm_hw_send_at_cmd(char *cmd, char *expect_resp, char *expect_resp_at_the_end,
                        uint32_t timeout,uint8_t retry_count, gsm_send_at_cb_t callback);

void gsm_init_hw(void);
void gsm_data_layer_initialize(void);
void gsm_uart_rx_cb(void);
void gsm_manager_tick(void);
void gsm_hardware_tick(void);

/**
 * @brief               Get gsm imei from buffer
 * @param[in]           imei_buffer raw buffer from gsm module
 * @param[out]          result result output
 * @note                Maximum imei length is 15
 */ 
void gsm_utilities_get_imei(uint8_t *imei_buffer, uint8_t * result);

void gsm_get_short_apn(char *ShortAPN);
void gsm_get_cell_id_and_signal_strength(char *Buffer);


void gsm_query_sms(void);
void gsm_process_cmd_from_sms(char *Buffer);

/**
 * @brief Send sms to phone number
 * @param[in] phone_number : Des phone number
 * @param[in] message : A message send to phone number
 */
bool gsm_send_sms(char *phone_number, char *message);


/**
 * @brief Get SMS buffer size
 * @retval Pointer to sms buffer size
 */
uint32_t gsm_get_max_sms_memory_buffer(void);

/**
 * @brief Get SMS context
 * @retval Pointer to sms memory buffer
 */
gsm_sms_msg_t *gsm_get_sms_memory_buffer(void);


/**
 * @brief Process new sms message
 * @param[in] buffer : Received buffer from serial port
 */
void gsm_sms_layer_process_cmd(char * buffer);

void gsm_change_state(gsm_state_t NewState);
void gsm_pwr_control(uint8_t State);
void gsm_send_status_to_mobilephone(char *SDT);
//void gsm_send_sms(char *Buffer, uint8_t CallFrom);
void gsm_process_at_cmd(char *Lenh);
void gsm_reconnect_tcp(void);
void gsm_change_state_sleep(void);
void gsm_test_read_sms(void);
bool gsm_data_layer_is_module_sleeping(void);
void gsm_query_sms_tick(void);
void gsm_check_sms_tick(void);
void gsm_set_timeout_to_sleep(uint32_t second);


//======================== FOR MODEM ========================//

#define MODEM_BUFFER_SIZE 2048

#define MODEM_IDLE 0
#define MODEM_ERROR 1
#define MODEM_READY 2
#define MODEM_LISTEN 3
#define MODEM_ONLINE 4
#define MODEM_DIAL 5
#define MODEM_HANGUP 6

typedef struct
{
    char *cmd;
    char *expect_resp_from_atc;
    char *expected_response_at_the_end;
    uint32_t timeout_atc_ms;
    uint32_t current_timeout_atc_ms;
    uint8_t retry_count_atc;
    SmallBuffer_t recv_buff;
    gsm_send_at_cb_t send_at_callback;
} gsm_at_cmd_t;

typedef struct
{
    gsm_at_cmd_t atc;
} gsm_hardware_t;


/**
 * @brief       Build sensor msg 
 */
uint16_t gsm_build_http_post_msg(void);

/**
 * @brief       Send data directly to serial port
 * @param[in]   raw Raw data send to serial port
 * @param[in]   len Data length
 */
void gsm_hw_uart_send_raw(uint8_t *raw, uint32_t length);


/**
 * @brief       Get internet mode
 * @retval      Internet mode
 */
gsm_internet_mode_t *gsm_get_internet_mode(void);

/**
 * @brief       GSM hardware uart polling
 */
void gsm_hw_layer_run(void);

/**
 * @brief       Change GSM hardware uart polling interval is ms
 * @param[in]   Polling interval in ms
 */
void gsm_change_hw_polling_interval(uint32_t ms);

/**
 * @brief       Set flag to prepare enter read sms state
 */
void gsm_set_flag_prepare_enter_read_sms_mode(void);


/**
 * @brief       Enter read sms state
 */
void gsm_enter_read_sms(void);

/**
 * @brief       Get current tick in ms
 * @retval      Current tick in ms
 */
uint32_t gsm_get_current_tick(void);

/**
 * @brief       Update current index of gsm dma rx buffer
 * @param[in]   rx_status RX status code
 *				TRUE : data is valid, FALSE data is invalid
 */
void gsm_uart_rx_dma_update_rx_index(bool rx_status);

/**
 * @brief       UART transmit callback
 * @param[in]   status TX status code
 *				TRUE : transmit data is valid
 *				FALSE transmit data is invalid
 */
void gsm_uart_tx_complete_callback(bool status);

#endif // __GSM_H__
