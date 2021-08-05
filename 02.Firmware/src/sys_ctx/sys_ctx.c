#include "sys_ctx.h"
#include "app_eeprom.h"

static sys_ctx_t m_ctx = 
{
    .status.is_enter_test_mode = false,
    .error.value = 0,
    .peripheral_running.value = 0,
};

sys_ctx_t *sys_ctx(void)
{
	return &m_ctx;
}
