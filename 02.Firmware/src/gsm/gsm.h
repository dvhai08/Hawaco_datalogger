#ifndef GSM_H
#define GSM_H

/**
 * \defgroup        gsm GSM
 * \brief           All gsm function process here
 * \{
 */

#include <stdbool.h>
#include "sys_ctx.h"

#define GSM_ON 1
#define GSM_OFF 2

#define GSMIMEI 0
#define SIMIMEI 1

#define MAX_GSMRESETSYSTEMCOUNT 600 //10 phut

#define MODEM_BUFFER_SIZE 2048

#define MODEM_IDLE 0
#define MODEM_ERROR 1
#define MODEM_READY 2
#define MODEM_LISTEN 3
#define MODEM_ONLINE 4
#define MODEM_DIAL 5
#define MODEM_HANGUP 6

#define __GSM_SLEEP_MODE__ 1
#define __USED_HTTP__ 0

#define GSM_READ_SMS_ENABLE     1
#define GSM_MAX_SMS_CONTENT_LENGTH   160
#define GSM_MAX_SMS_PHONE_LENGTH     16
#define GSM_MIN_SMS_PHONE_LENGTH     9

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
//    GSM_STATE_SEND_ATC = 7,
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
    uint8_t ri_signal;
    uint8_t gsm_ready;
    uint8_t timeout_after_reset;
    uint8_t access_tech;
} gsm_manager_t;

typedef struct
{
    char phone_number[GSM_MAX_SMS_PHONE_LENGTH];
    char message[GSM_MAX_SMS_CONTENT_LENGTH];
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


void gsm_get_short_apn(char *ShortAPN);
void gsm_get_cell_id_and_signal_strength(char *Buffer);


void gsm_query_sms(void);
void gsm_process_cmd_from_sms(char *Buffer);

/*!
 * @brief           Send sms to phone number
 * @param[in]       phone_number : Des phone number
 * @param[in]       message : A message send to phone number
 */
bool gsm_send_sms(char *phone_number, char *message);


/*!
 * @brief           Get SMS buffer size
 * @retval          Pointer to sms buffer size
 */
uint32_t gsm_get_max_sms_memory_buffer(void);

/*!
 * @brief       Get SMS context
 * @retval      Pointer to sms memory buffer
 */
gsm_sms_msg_t *gsm_get_sms_memory_buffer(void);


/*!
 * @brief       Process new sms message
 * @param[in]   buffer : Received buffer from serial port
 */
void gsm_sms_layer_process_cmd(char * buffer);

/*!
 * @brief           Change GSM state 
 * @param[in]       new_state New gsm state
 */
void gsm_change_state(gsm_state_t new_state);

/*!
 * @brief       Check gsm is sleeping or not
 * @retval      TRUE : GSM is sleeping
 *              FALSE : GSM is not sleeping
 */
bool gsm_data_layer_is_module_sleeping(void);

// void gsm_query_sms_tick(void);
// void gsm_check_sms_tick(void);

void gsm_set_timeout_to_sleep(uint32_t second);




typedef struct
{
    char *cmd;
    char *expect_resp_from_atc;
    char *expected_response_at_the_end;
    uint32_t timeout_atc_ms;
    uint32_t current_timeout_atc_ms;
    uint8_t retry_count_atc;
    sys_ctx_small_buffer_t recv_buff;
    gsm_send_at_cb_t send_at_callback;
} gsm_at_cmd_t;

typedef struct
{
    gsm_at_cmd_t atc;
} gsm_hardware_t;


/*!
 * @brief       Send data directly to serial port
 * @param[in]   raw Raw data send to serial port
 * @param[in]   len Data length
 */
void gsm_hw_uart_send_raw(uint8_t *raw, uint32_t length);


/*!
 * @brief       Get internet mode
 * @retval      Internet mode
 */
gsm_internet_mode_t *gsm_get_internet_mode(void);

/*!
 * @brief       GSM hardware uart polling
 */
void gsm_hw_layer_run(void);

/*!
 * @brief       Change GSM hardware uart polling interval is ms
 * @param[in]   Polling interval in ms
 */
void gsm_change_hw_polling_interval(uint32_t ms);

/*!
 * @brief       Set flag to prepare enter read sms state
 */
void gsm_set_flag_prepare_enter_read_sms_mode(void);


/*!
 * @brief       Enter read sms state
 */
void gsm_enter_read_sms(void);

/*!
 * @brief       Get current tick in ms
 * @retval      Current tick in ms
 */
uint32_t gsm_get_current_tick(void);

/*!
 * @brief       Update current index of gsm dma rx buffer
 * @param[in]   rx_status RX status code
 *				TRUE : data is valid, FALSE data is invalid
 */
void gsm_uart_rx_dma_update_rx_index(bool rx_status);

/*!
 * @brief       UART transmit callback
 * @param[in]   status TX status code
 *				TRUE : transmit data is valid
 *				FALSE transmit data is invalid
 */
void gsm_uart_tx_complete_callback(bool status);

/*!
 * @brief		Get SIM IMEI
 * @retval		SIM IMEI
 */
char* gsm_get_sim_imei(void);

/*!
 * @brief		Get SIM CCID
 * @retval		SIM CCID pointer
 */
char *gsm_get_sim_ccid(void);

/*!
 * @brief		Get GSM IMEI
 * @retval		GSM IMEI
 */
char* gsm_get_module_imei(void);


/*!
 * @brief		Set SIM IMEI
 * @param[in]	SIM IMEI
 */
void gsm_set_sim_imei(char *imei);

/*!
 * @brief		Set GSM IMEI
 * @param[in]	GSM IMEI
 */
void gsm_set_module_imei(char *imei);

/*!
 * @brief		Set network operator
 * @param[in]	Network operator
 */
void gsm_set_network_operator(char *nw_operator);

/*!
 * @brief		Get network operator
 * @retval		Network operator
 */
char *gsm_get_network_operator(void);

/*!
 * @brief		Set GSM CSQ
 * @param[in]	CSQ GSM CSQ
 */
void gsm_set_csq(uint8_t csq);

/*!
 * @brief		Get GSM CSQ
 * @retval	 	GSM CSQ
 */
uint8_t gsm_get_csq(void);

/*!
 * @brief		Get GSM CSQ in percent
 * @retval	 	GSM CSQ in percent
 */
uint8_t gsm_get_csq_in_percent(void);

/*!
 * @brief		Wakeup gsm module
 */
void gsm_set_wakeup_now(void);

/*!
 * @brief		Reset gsm rx buffer
 */
void gsm_hw_layer_reset_rx_buffer(void);

/*!
 * @brief		GSM mnr task
 */
void gsm_mnr_task(void *arg);

/**
 * \}
 */

#endif // GSM_H
