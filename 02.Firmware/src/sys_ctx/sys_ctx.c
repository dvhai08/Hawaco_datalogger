#include "sys_ctx.h"
#include "app_eeprom.h"

static sys_ctx_t m_ctx =
{
#if BOOTLOADER_MODE == 0
        .status.is_enter_test_mode = false,
        .error_not_critical.value = 0,
        .error_critical.value = 0,
#endif
        .peripheral_running.value = 0,
};

sys_ctx_t *sys_ctx(void)
{
    return &m_ctx;
}
