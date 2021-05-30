#ifndef GSM_HTTP_H
#define GSM_HTTP_H

#include <stdint.h>
#include <stdbool.h>

#define GSM_HTTP_MAX_URL_SIZE 255


typedef enum
{
    GSM_HTTP_EVENT_START,
    GSM_HTTP_EVENT_CONNTECTED,
    GSM_HTTP_EVENT_DATA,
    GSM_HTTP_EVENT_FINISH_SUCCESS,
    GSM_HTTP_EVENT_FINISH_FAILED
} gsm_http_event_t;

typedef enum
{
    GSM_HTTP_ACTION_POST = 0x00,
    GSM_HTTP_ACTION_GET,
} gsm_http_action_t;

typedef struct
{
    gsm_http_action_t action;
    uint32_t length;
    uint8_t *data;
} gsm_http_data_t;


typedef void (*gsm_http_event_cb_t)(gsm_http_event_t event, void * data);


typedef struct
{
    gsm_http_event_cb_t on_event_cb;
    char url[GSM_HTTP_MAX_URL_SIZE+1];
    gsm_http_action_t action;
    uint16_t port;
} gsm_http_config_t;


/**
 * @brief Get max url size allowed
 * @retval Max url length
 */
static inline uint32_t gsm_http_get_max_url_size(void)
{
    return GSM_HTTP_MAX_URL_SIZE - 1;       // reserve 1 byte for null terminal
}

/**
 * @brief Get current gsm http configuration
 * @retval Pointer to current gsm configuration
 */
gsm_http_config_t *gsm_http_get_config(void);

/**
 * @brief Clean up current configuration
 */
void gsm_http_cleanup(void);

/**
 * @brief Start gsm http download
 * @param[in] config : HTTP configuration
 * @retval TRUE : Operation success
 *         FALSE : Operation failed
 */
bool gsm_http_start(gsm_http_config_t * config);

void gsm_http_query(gsm_response_event_t event, void *response_buffer);


#endif /* GSM_HTTP_H */
