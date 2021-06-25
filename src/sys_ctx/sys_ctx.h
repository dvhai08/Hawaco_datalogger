#ifndef SYS_CTX_H
#define SYS_CTX_H

#include <stdint.h>
#include <stdbool.h>

#define SYS_CTX_BUFFER_STATE_BUSY 1       // Trang thai dang them du lieu
#define SYS_CTX_BUFFER_STATE_IDLE 2       // Trang thai cho
#define SYS_CTX_BUFFER_STATE_PROCESSING 3 // Trang thai du lieu dang duoc xu ly
#define SYS_CTX_SMALL_BUFFER_SIZE 384

typedef struct
{
    uint8_t buffer[SYS_CTX_SMALL_BUFFER_SIZE];
    uint16_t index;
    uint8_t state;
} sys_ctx_small_buffer_t;
typedef struct
{
	uint32_t sleep_time_s;
	uint32_t disconnect_timeout_s;
	uint8_t ota_url[128+48];
    bool is_enter_test_mode;
} sys_ctx_status_t;

typedef struct
{
	sys_ctx_status_t status;
} sys_ctx_t;


/**
 * @brief		Get system context
 */
sys_ctx_t *sys_ctx(void);

#endif /* SYS_CTX_H */
