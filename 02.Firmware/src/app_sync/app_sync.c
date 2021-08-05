#include "app_sync.h"
#include <string.h>

#define SYNC_TIMER_INTERVAL APP_TIMER_TICKS(m_sync_slice_ms)
#define INTERVAL_MS_TO_TICK(x) (x/m_config.polling_interval_ms)  // convert ms to polling tick count

static app_sync_config_t m_config;

uint32_t app_sync_get_timeslice_interval(void)
{
    return m_config.polling_interval_ms;
}

bool app_sync_register_callback(sync_drv_callback_t callback, 
                                uint32_t interval, 
                                sync_drv_syn_mode_t mode, 
                                sync_drv_scope_t scope_sync)
{
    if (callback == NULL)
        return false;

    if (INTERVAL_MS_TO_TICK(interval) == 0)
        return false;

    if (!sync_drv_insert_event(callback, INTERVAL_MS_TO_TICK(interval), mode, scope_sync))
    {
//        DEBUG_PRINTF("Insert event error\r\n");
        return false;
    }

    return true;
}

void app_sync_remove_callback(sync_drv_callback_t callback)
{
    sync_drv_remove(sync_drv_find_node_by_callback(callback));
}

static void app_sync_system_handle(void *param)
{
    sync_drv_reg_t *node_sync = sync_drv_next();
    sync_drv_reg_t *origin_node = node_sync;
    uint32_t diff_count = (uint32_t)param;

    do
    {
        if (node_sync == NULL)
            return;

        if (node_sync->fn == NULL)
            return;

        node_sync->event_remain_cnt -= diff_count;

        if (node_sync->event_remain_cnt <= 0)
        {
            // run callback of function register
            //if (node_sync->scope_sync == SYNC_DRV_SCOPE_IN_CALLBACK)
            //{
            //    if (node_sync->fn)
            //        node_sync->fn(NULL);
            //}
            //else
            //{
            //    if (node_sync->fn)
            //        node_sync->fn(NULL);
            //}
            if (node_sync->fn)
                    node_sync->fn(NULL);
        }
        else
        {
            node_sync = node_sync->next; //sync_drv_next();
            continue;
        }

        if (node_sync->mode == SYNC_DRV_SINGLE_SHOT)
        {
            if (node_sync == origin_node)
            {
                origin_node = origin_node->next;
            }
            sync_drv_remove(node_sync);
        }
        else if (node_sync->mode == SYNC_DRV_REPEATED)
        {
            node_sync->event_remain_cnt = node_sync->interval;
        }

        node_sync = node_sync->next; //sync_drv_next();

    } while (node_sync != origin_node);
}

bool app_sync_sytem_init(app_sync_config_t *config)
{
    if (config->get_ms == NULL || config->polling_interval_ms == 0)
    {
        // DEBUG_PRINTF("Invalid param\r\n");
        return false;
    }
    
    memcpy(&m_config, config, sizeof(m_config));

    return true;
}


uint32_t app_sync_get_number_of_event(void)
{
    return sync_drv_get_number_of_active_event();
}

void app_sync_polling_task(void)
{
    static uint32_t m_last_tick = 0;
    uint32_t diff = m_config.get_ms() - m_last_tick;

    if (diff >= m_config.polling_interval_ms)
    {
        app_sync_system_handle((void*)(diff/m_config.polling_interval_ms));
        m_last_tick = m_config.get_ms();
    }
}
