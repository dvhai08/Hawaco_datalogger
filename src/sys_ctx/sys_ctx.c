#include "sys_ctx.h"
#include "app_eeprom.h"

static sys_ctx_t m_ctx = 
{
    .status.is_enter_test_mode = false,
};

sys_ctx_t *sys_ctx(void)
{
	return &m_ctx;
}