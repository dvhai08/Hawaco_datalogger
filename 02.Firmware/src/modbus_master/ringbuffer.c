#include "ringbuffer.h"
/**
  * @brief  rbInitialize��ʼ�����ã�����������Ϣ����ṹ��
  * @param  p_ringbuffer:ringbuffer�ṹ��
	*         buff�����ݻ�����
	*         length����������С
  * @note   
  * @retval void
  * @author xiaodaqi
  */
void ringbuffer_initialize(ringbuffer *p_ringbuffer, uint8_t *buff, uint16_t length)
{
    p_ringbuffer->pBuff = buff;
    p_ringbuffer->pEnd = buff + length;
    p_ringbuffer->wp = buff;
    p_ringbuffer->rp = buff;
    p_ringbuffer->length = length;
    p_ringbuffer->over_flow = 0;
}

/**
  * @brief  ���ringbuffer�ṹ��������Ϣ
  * @param  pRingBuff����������ringbuffer
  * @note   
  * @retval void
  * @author xiaodaqi
  */

void ringbuffer_clear(ringbuffer *p_ringbuffer)
{
    p_ringbuffer->wp = p_ringbuffer->pBuff;
    p_ringbuffer->rp = p_ringbuffer->pBuff;
    p_ringbuffer->over_flow = 0;
}

/**
  * @brief  ѹ�뵥�ֽڵ�������
  * @param  pRingBuff����������ringbuffer
  *         value��ѹ�������
  * @note   
  * @retval void
  * @author xiaodaqi
  */
void ringbuffer_push(ringbuffer *p_ringbuffer, uint8_t value)
{
    uint8_t *wp_next = p_ringbuffer->wp + 1;
    if (wp_next == p_ringbuffer->pEnd)
    {
        wp_next -= p_ringbuffer->length; // Rewind pointer when exceeds bound
    }
    if (wp_next != p_ringbuffer->rp)
    {
        *p_ringbuffer->wp = value;
        p_ringbuffer->wp = wp_next;
    }
    else
    {
        p_ringbuffer->over_flow = 1;
    }
}

/**
  * @brief  ѹ�����ֽڵ�������
  * @param  pRingBuff����������ringbuffer   
  * @note   
  * @retval ѹ��������
  * @author xiaodaqi
  */
uint8_t ringbuffer_pop(ringbuffer *p_ringbuffer)
{
    if (p_ringbuffer->rp == p_ringbuffer->wp)
        return 0; // empty

    uint8_t ret = *(p_ringbuffer->rp++);
    if (p_ringbuffer->rp == p_ringbuffer->pEnd)
    {
        p_ringbuffer->rp -= p_ringbuffer->length; // Rewind pointer when exceeds bound
    }
    return ret;
}

/**
  * @brief  ��ȡ��������δ�������ֽ���
  * @param  pRingBuff����������ringbuffer   
  * @note   
  * @retval ���������ֽ���
  * @author xiaodaqi
  */
uint16_t ringbuffer_get_count(const ringbuffer *p_ringbuffer)
{
    return (p_ringbuffer->wp - p_ringbuffer->rp + p_ringbuffer->length) % p_ringbuffer->length;
}

/**
  * @brief  �жϻ������Ƿ�Ϊ��
  * @param  pRingBuff����������ringbuffer   
  * @note   
  * @retval ��Ϊ1������Ϊ0
  * @author xiaodaqi
  */
int8_t ringbuffer_is_empty(const ringbuffer *p_ringbuffer)
{
    return p_ringbuffer->wp == p_ringbuffer->rp;
}

/**
  * @brief  �жϻ������Ƿ��
  * @param  pRingBuff����������ringbuffer   
  * @note   
  * @retval ��Ϊ1������Ϊ0
  * @author xiaodaqi
  */
int8_t ringbuffer_is_full(const ringbuffer *p_ringbuffer)
{
    return (p_ringbuffer->rp - p_ringbuffer->wp + p_ringbuffer->length - 1) % p_ringbuffer->length == 0;
}
