#ifndef SYS_CTX_H
#define SYS_CTX_H

#include <stdint.h>
#include <stdbool.h>

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
