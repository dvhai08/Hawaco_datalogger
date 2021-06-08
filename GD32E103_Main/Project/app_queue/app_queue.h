#ifndef APP_QUEUE_H
#define APP_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#define APP_QUEUE_MAX_SIZE 6

typedef struct
{
    uint8_t *pointer;
    uint32_t length;
} app_queue_data_t;

/* queue buffer structure */
typedef struct
{
    app_queue_data_t data[APP_QUEUE_MAX_SIZE];
    uint16_t wr_idx;
    uint16_t rd_idx;
} app_queue_t;



static inline void app_queue_reset(app_queue_t *queue)
{
    queue->wr_idx = 0;
    queue->rd_idx = 0;
}

static bool app_queue_is_empty(app_queue_t *queue)
{
    if (queue->rd_idx == queue->wr_idx)
    {
        return true;
    }
    return  false;
}

static bool app_queue_is_full(app_queue_t *queue)
{
    uint32_t size = 1;
    if (queue->wr_idx < queue->rd_idx)
    {
        size = queue->rd_idx - queue->wr_idx - 1;
    }

    if (size == 0)
    {
        return true;
    }
    return  false;
}

static bool app_queue_put(app_queue_t *queue, app_queue_data_t *data)
{
    if (app_queue_is_full(queue))
    {
        return false;
    }

    memcpy(&queue->data[queue->wr_idx], data, sizeof(app_queue_data_t));
    queue->wr_idx++;
    if (queue->wr_idx == APP_QUEUE_MAX_SIZE)
    {
        queue->wr_idx = 0;
    }
    return  true;
}

static bool app_queue_get(app_queue_t *queue, app_queue_data_t *data)
{
    if (app_queue_is_empty(queue))
    {
        return false;
    }

    memcpy(data, &queue->data[queue->rd_idx], sizeof(app_queue_data_t));
    queue->rd_idx++;
    if (queue->rd_idx == APP_QUEUE_MAX_SIZE)
    {
        queue->rd_idx = 0;
    }
    return  true;
}

#endif /* APP_QUEUE_H */
