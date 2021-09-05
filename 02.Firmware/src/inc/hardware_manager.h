#ifndef HW_MANAGER_H
#define HW_MANAGER_H

#include <stdint.h>
typedef union
{
    struct
    {
        uint8_t reserve : 2;
		uint8_t fault : 1;
        uint8_t pin_reset : 1;
        uint8_t power_on : 1;
        uint8_t software : 1;
        uint8_t watchdog : 1;
        uint8_t low_power : 1;
    } name;
    uint8_t value;
    
} hardware_manager_reset_reason_t;


/*!
 * @brief       Get reset reason
 * @retval      Reset reason 
 */
hardware_manager_reset_reason_t *hardware_manager_get_reset_reason(void) ;

/*!
 * @brief       Reset system
 * @param[in]   reset_reason Reset reason 
 */
void hardware_manager_sys_reset(uint8_t reset_reason);

/*!
 * @brief       Clear UART error flag 
 */
void hardware_manager_clear_uart_error_flag(void);


#endif // HW_MANAGER_H

