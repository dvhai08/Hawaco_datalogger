#ifndef SYS_CTX_H
#define SYS_CTX_H

#include <stdint.h>
#include <stdbool.h>

#define SYS_CTX_BUFFER_STATE_BUSY 1       // Trang thai dang them du lieu
#define SYS_CTX_BUFFER_STATE_IDLE 2       // Trang thai cho
#define SYS_CTX_BUFFER_STATE_PROCESSING 3 // Trang thai du lieu dang duoc xu ly
#ifdef DTG01
#define SYS_CTX_SMALL_BUFFER_SIZE (1024)
#else
#define SYS_CTX_SMALL_BUFFER_SIZE (1024+128)
#endif
typedef struct
{
    uint8_t buffer[SYS_CTX_SMALL_BUFFER_SIZE];
    uint16_t index;
    uint8_t state;
} sys_ctx_small_buffer_t;

typedef struct
{
//	uint32_t sleep_time_s;
    
	uint32_t disconnect_timeout_s;
    uint32_t disconnected_count;
	uint32_t last_state_is_disconnect;
    bool is_enter_test_mode;
    bool enter_ota_update;
    uint8_t delay_ota_update;
    uint8_t delay_store_data_to_ext_flash;
    uint8_t ota_url[128+48];
	uint8_t try_new_server;
	char *new_server;
	uint8_t poll_broadcast_msg_from_server;
	uint32_t next_time_get_data_from_server;
	uint32_t timeout_wait_message_sync_data;
	uint8_t dump_flash;
	uint32_t total_sms_in_24_hour;
	uint32_t reset_sms_limit_timeout;
    uint8_t need_charge;
} sys_ctx_status_t;

typedef union
{
    struct
    {
        uint8_t flash : 1;
        uint8_t low_bat : 1;
        uint8_t circuit_break : 1;
		uint8_t rs485_err : 1;
        uint8_t reserve : 4;
    } detail;
    uint8_t value;
} sys_ctx_error_not_critical_t;

typedef union
{
    struct
    {
        uint8_t sensor_out_of_range : 1;		// sensor out of range
        uint8_t reserve : 7;
    } detail;
    uint8_t value;
} sys_ctx_error_critical_t;

typedef union
{
    struct
    {
        uint8_t adc : 1;
        uint8_t measure_input_pwm_running : 1;
        uint8_t gsm_running : 1;
        uint8_t flash_running : 1;
        uint8_t rs485_running : 1;
        uint8_t high_bat_detect : 1;
        uint8_t wait_for_input_4_20ma_power_on : 1;
        uint8_t reserve : 2;
    } name;
    uint8_t value;
} sys_ctx_sleep_peripheral_t;

typedef struct
{
#ifndef BOOTLOADER_MODE
	sys_ctx_status_t status;
    sys_ctx_error_not_critical_t error_not_critical;
	sys_ctx_error_critical_t error_critical;
#endif
    sys_ctx_sleep_peripheral_t peripheral_running;
} sys_ctx_t;

/*!
 * @brief		Get system context
 */
sys_ctx_t *sys_ctx(void);

#endif /* SYS_CTX_H */
