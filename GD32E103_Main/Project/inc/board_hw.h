#ifndef BOARD_HW_H
#define BOARD_HW_H

#include <stdint.h>
#include <stdbool.h>

#define BOARD_HW_BUTTON_0           0x00        // pair
#define BOARD_HW_BUTTON_1           0x01        // On/off
#define BOARD_HW_BUTTON_2           0x02


typedef enum
{
    BOARD_HW_RELAY_0,
    BOARD_HW_RELAY_1
} board_hw_relay_t;

/**
 * @brief               Initialize all board gpio
 */
void board_hw_init_gpio(void);

/**
 * @brief               Read DIO gpio pin 
 * @retval              gpio level (0-1)
 */
uint32_t board_hw_read_dio(void);

/**
 * @brief               Write LoRa reset pin gpio level
 * @param[in]           on = 1 Write reset pin high
 *                      on = 0 Write reset pin low
 */
void board_hw_lora_write_lora_reset_pin(bool on);

/**
 * @brief               Read Latch GPIO
 * @retval              Latch GPIO output level (0-1)
 */
uint32_t board_hw_read_latch_level(void);

/**
 * @brief               Write reset pin gpio level
 * @param[in]           relay : Relay number
 * @param[in]           on = 1 Turn on relay
 *                      on = 0 Turn off relay
 */
void board_hw_set_relay(board_hw_relay_t relay, bool on);

/**
 * @brief               Read button input value
 * @retval              Button level
 */
uint32_t board_hw_button_read(uint32_t pin);

/**
 * @brief               Read contactor gpio input value
 * @retval              Contactor gpio input level
 */
uint32_t board_hw_read_contactor_input_gpio_level(void);

/**
 * @brief               Read phase gpio input value
 * @retval              Phase gpio input
 */
uint32_t board_hw_read_phase_input_gpio_level(void);

/**
 * @brief               Read contactor input value
 * @retval              TRUE Contactor detected
 *                      FALSE Contactor not detected
 */
bool board_hw_is_contactor_detect(void);

/**
 * @brief               Internal hardware timer poll
 */
void board_hw_internal_poll(void);

/**
 * @brief               Read phase lost status
 * @retval              TRUE Phase lost
 *                      FALSE Phase not lost
 */
bool board_hw_is_phase_lost(void);

/**
 * @brief               Reset phase lost timeout
 */
void board_hw_reset_phase_lost_timeout(void);

/**
 * @brief               Reset contactor timeout
 */
void board_hw_reset_contactor_monitor_timeout(void);

/**
 * @brief               Reset system
 */
void board_hw_reset(void);

/**
 * @brief               Read current relay latch feedback pin level
 * @param[in]           relay Relay number
 * @retval              Latch feedback level
 */
bool board_hw_read_latch_feedback(board_hw_relay_t relay);

/**
 * @brief               Start hw timer for contactor monitor and phase monitor
 */
void board_hw_internal_timer_start(void);

/**
 * @brief               Initialize hardware uart debug
 */
void board_hw_uart_debug_initialize(void);

#endif /*BOARD_HW_H */

