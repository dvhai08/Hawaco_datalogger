#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdio.h>

    typedef struct
    {
        uint8_t *pBuff;
        uint8_t *pEnd; // pBuff + legnth
        uint8_t *wp;   // Write Point
        uint8_t *rp;   // Read Point
        uint16_t length;
        uint8_t over_flow; // set when buffer overflowed
    } ringbuffer;

    void ringbuffer_initialize(ringbuffer *p_ringbuffer, uint8_t *buff, uint16_t length);
    void ringbuffer_clear(ringbuffer *p_ringbuffer);
    void ringbuffer_push(ringbuffer *p_ringbuffer, uint8_t value);
    uint8_t ringbuffer_pop(ringbuffer *p_ringbuffer);
    uint16_t ringbuffer_get_count(const ringbuffer *p_ringbuffer);
    int8_t ringbuffer_is_empty(const ringbuffer *p_ringbuffer);
    int8_t ringbuffer_is_full(const ringbuffer *p_ringbuffer);

#ifdef __cplusplus
}
#endif

#endif
