#ifndef _SYNC_DRV_H_
#define _SYNC_DRV_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//#include "clist.h"

typedef void (*sync_drv_callback_t)(void * param);

#ifndef SYNC_DRV_STATIC_ALLOCATOR
#define SYNC_DRV_STATIC_ALLOCATOR   1
#endif

#if SYNC_DRV_STATIC_ALLOCATOR   
#define SYNC_DRV_MAX_EVENT          (24)
#endif 

#if SYNC_DRV_STATIC_ALLOCATOR
#define sync_malloc    sync_static_malloc
#define sync_free      sync_static_free
#else
#define sync_malloc    malloc
#define sync_malloc      free
#endif

typedef enum 
{
    SYNC_DRV_SINGLE_SHOT = 0,
    SYNC_DRV_REPEATED = 1
} sync_drv_syn_mode_t;

typedef enum 
{
    SYNC_DRV_SCOPE_IN_LOOP = 0,
    SYNC_DRV_SCOPE_IN_CALLBACK = 1
} sync_drv_scope_t;


typedef struct 
{
    void        *next;
    uint32_t    interval;
    int32_t    event_remain_cnt;
    sync_drv_syn_mode_t    mode; //sync_drv_syn_mode_t
    sync_drv_scope_t scope_sync;
    sync_drv_callback_t fn;
} sync_drv_reg_t;

/**
 * @brief Insert event and callback into linked list
 * @retval TRUE : success
 *         FALSE : No memory
 */
bool sync_drv_insert_event(sync_drv_callback_t callback, uint32_t interval, sync_drv_syn_mode_t mode, sync_drv_scope_t scope_sync);


sync_drv_reg_t *sync_drv_next(void);

/**
 * @brief Search and remove sync driver by callback
 */
void sync_drv_remove_by_callback(sync_drv_callback_t cb);

/**
 * @brief Change sync interval 
 */
void sync_drv_change_interval(sync_drv_callback_t cb, uint32_t interval);

/**
 * @brief Remove sync event
 */
void sync_drv_remove(sync_drv_reg_t *node_sync);

/**
 * @brief Initialize sync driver
 */
void sync_drv_initialize(void);

/**
 * @brief Find node by callback
 */
sync_drv_reg_t *sync_drv_find_node_by_callback(sync_drv_callback_t callback);

#if SYNC_DRV_STATIC_ALLOCATOR

/**
 * @brief Get maximim sync event supported by driver
 * @retval Max event
 */
uint32_t sync_drv_get_maximum_event(void);

/**
 * @brief Get number of sync event currently running
 * @retval Number of active event
 */
uint32_t sync_drv_get_number_of_active_event(void);

#endif

#endif //_SYNC_DRV_H_
