
#ifndef _APP_SYNC_H_
#define _APP_SYNC_H_

#include <stdint.h>
#include <stdbool.h>
#include "sync_drv.h"

typedef struct 
{
    sync_drv_callback_t cb;
} app_sync_system_t;

typedef uint32_t (*app_sync_get_time_ms_cb_t)(void);

typedef struct 
{
    app_sync_get_time_ms_cb_t get_ms;
    uint32_t polling_interval_ms;
} app_sync_config_t;


/**
 * @brief Register sync driver callback
 * @param[in] interval Interval in milisecond
 * @param[in] mode Event excecute only nce time or excecute forever
 * @param[in] scope_sync Event execute in callback (interrupt) or in supper loop
 * @retval TRUE Register event success
 *          Othewise Register event failed
 */
bool app_sync_register_callback(sync_drv_callback_t callback, 
                                uint32_t interval, 
                                sync_drv_syn_mode_t mode, 
                                sync_drv_scope_t scope_sync);

/**
 * @brief Remove sync driver callback
 * @param[in] scope_sync Event execute in callback
 */
void app_sync_remove_callback(sync_drv_callback_t callback);

/**
 * @brief Get number of event
 * @retval Number of event in driver
 */
uint32_t app_sync_get_number_of_event(void);

/**
 * @brief Get sync event in milisecond
 * @retval Current sync period in milisecond
 */
uint32_t app_sync_get_timeslice_interval(void);

/**
 * @brief Initialize system event service
 * @param[in] config : Driver configuration parameter
 * @retval TRUE : App sync init success
 *         FALSE : Operation failed
 */
bool app_sync_sytem_init(app_sync_config_t * config);

/**
 * @brief app_sync_polling_task
 */
void app_sync_polling_task(void);

#endif /*_APP_SYNC_H_ */
