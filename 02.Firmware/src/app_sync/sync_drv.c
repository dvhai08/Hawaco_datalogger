#include "sync_drv.h"
#include "clist.h"
#include <string.h>

static clist_node_t sync_head_node;

#if SYNC_DRV_STATIC_ALLOCATOR

#include <stdbool.h>
#include <stdint.h>

#define DEBUG_SYNC_DRV 1

#define SYNC_DRV_MEM_AVAILBLE 0
#define SYNC_DRV_MEM_NOT_AVAILBLE 1

#if DEBUG_SYNC_DRV

#define SYNC_LOG_LEVEL LOG_LEVEL_INFO
#else
#define SYNC_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

typedef struct
{
    sync_drv_reg_t evt;
    uint8_t in_use;
} sync_drv_pool_memory;

sync_drv_pool_memory m_pool[SYNC_DRV_MAX_EVENT];

static int32_t m_active_evt = 0;

#endif /* SYNC_DRV_STATIC_ALLOCATOR  */

void sync_drv_initialize(void)
{
#if SYNC_DRV_STATIC_ALLOCATOR

    static bool done = false;
    if (done == false)
    {
        for (uint32_t i = 0; i < SYNC_DRV_MAX_EVENT; i++)
        {
            m_pool[i].in_use = SYNC_DRV_MEM_AVAILBLE;
            memset(&m_pool[i].evt, 0, sizeof(sync_drv_reg_t));
        }
        done = true;
    }

#endif /* SYNC_DRV_STATIC_ALLOCATOR */
}

#if SYNC_DRV_STATIC_ALLOCATOR

static sync_drv_reg_t *sync_static_malloc(uint32_t size)
{
    (void)size; // we dont use param size

    uint32_t i = 0;
    for (i = 0; i < SYNC_DRV_MAX_EVENT; i++)
    {
        if (m_pool[i].in_use == SYNC_DRV_MEM_AVAILBLE)
        {
            m_pool[i].in_use = SYNC_DRV_MEM_NOT_AVAILBLE;
            m_active_evt++;
            // DEBUG_PRINTF("[%s] : Allocate at addr %p, memory idx %d\n", __FUNCTION__, &m_pool[i].evt, i);
            return &m_pool[i].evt;
        }
    }

    // DEBUG_PRINTF("[%s] : Malloc failed\n", __FUNCTION__);
    return NULL;
}

static void sync_static_free(sync_drv_reg_t *evt)
{
    if (!evt)
        return;

    for (uint32_t i = 0; i < SYNC_DRV_MAX_EVENT; i++)
    {
        if (m_pool[i].in_use == SYNC_DRV_MEM_NOT_AVAILBLE 
            && memcmp(evt, &m_pool[i].evt, sizeof(sync_drv_reg_t)) == 0)
        {
            m_pool[i].in_use = SYNC_DRV_MEM_AVAILBLE;
            memset(&m_pool[i].evt, 0, sizeof(sync_drv_reg_t));
            m_active_evt--;
            return;
        }
    }

    // DEBUG_PRINTF("[%s] : Invalid param\n", __FUNCTION__);
}

uint32_t sync_drv_get_maximum_event(void)
{
    return SYNC_DRV_MAX_EVENT;
}

uint32_t sync_drv_get_number_of_active_event(void)
{
    return m_active_evt;
}

#endif /* SYNC_DRV_STATIC_ALLOCATOR */

static sync_drv_reg_t *sync_drv_create_node(sync_drv_callback_t callback, 
                                            uint32_t interval, 
                                            sync_drv_syn_mode_t mode, 
                                            sync_drv_scope_t scope_sync)
{
    if (callback == NULL)
        return NULL;

    sync_drv_reg_t *node_temp = (sync_drv_reg_t *)sync_malloc(sizeof(sync_drv_reg_t));
    if (node_temp == NULL)
        return NULL;

    node_temp->interval = interval;
    node_temp->event_remain_cnt = interval;
    node_temp->mode = mode;
    node_temp->scope_sync = scope_sync;
    node_temp->fn = callback;

    return node_temp;
}

sync_drv_reg_t *sync_drv_find_node_by_callback(sync_drv_callback_t callback)
{
    if (sync_head_node.next == NULL)
        return NULL;

    sync_drv_reg_t *node_temp = (sync_drv_reg_t *)sync_head_node.next;

    do
    {
        if (node_temp == NULL)
            return NULL;

        if (node_temp->fn == callback)
        {
            return node_temp;
        }

        node_temp = node_temp->next;
    } while (node_temp != (sync_drv_reg_t *)sync_head_node.next);

    return NULL;
}

bool sync_drv_insert_event(sync_drv_callback_t callback, 
                            uint32_t interval, 
                            sync_drv_syn_mode_t mode, 
                            sync_drv_scope_t scope_sync)
{
    if (!sync_drv_find_node_by_callback(callback))
    {
        sync_drv_reg_t *node_temp = sync_drv_create_node(callback, interval, mode, scope_sync);

        if (node_temp == NULL)
            return false;

        clist_rpush(&sync_head_node, (clist_node_t *)node_temp);
        return true;
    }
    return false;
}

sync_drv_reg_t *sync_drv_next(void)
{
    return (sync_drv_reg_t *)sync_head_node.next;
}

void sync_drv_remove(sync_drv_reg_t *node_sync)
{
    if (node_sync == NULL)
        return;

    sync_drv_reg_t *node_temp = (sync_drv_reg_t *)clist_remove(&sync_head_node,
                                                               (clist_node_t *)node_sync);

    if (node_temp == NULL)
        return;

    sync_free(node_temp);
}

void sync_drv_remove_by_callback(sync_drv_callback_t callback)
{
    if (callback == NULL)
        return;

    sync_drv_reg_t *node_temp = sync_drv_find_node_by_callback(callback);

    if (node_temp == NULL)
        return;

    sync_drv_remove(node_temp);
}

void sync_drv_change_interval(sync_drv_callback_t callback, uint32_t interval)
{
    if (callback == NULL)
        return;

    sync_drv_reg_t *node_temp = sync_drv_find_node_by_callback(callback);
    if (node_temp == NULL)
        return;

    node_temp->interval = interval;
    node_temp->event_remain_cnt = interval;
}
