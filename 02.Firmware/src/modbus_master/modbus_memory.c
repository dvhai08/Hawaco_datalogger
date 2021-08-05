#include "modbus_memory.h"
#include "ringbuffer.h"

static ringbuffer m_rx_ringbuffer;
uint8_t m_rx_raw_buffer[MODBUS_MEMORY_MAX_RX_BUFFER_SIZE];

/**
  * @brief  ��ʼ���жϽ��յĵ�ringbuffer���ζ�������,�жϽ��յ��ֽڶ���m_Modbus_Master_RX_RingBuff�ýṹ��ָ����й���
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t modbus_memory_ringbuffer_initialize(void)
{
    /*��ʼ��ringbuffer��ص�����*/
    ringbuffer_initialize(&m_rx_ringbuffer, m_rx_raw_buffer, MODBUS_MEMORY_MAX_RX_BUFFER_SIZE);
    return 1;
}

/**
  * @brief  ������ζ���
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
void modbus_memory_flush_rx_buffer(void)
{
    ringbuffer_clear(&m_rx_ringbuffer);
}
/**
  * @brief  �ж�GPS��ringbuffer�����Ƿ�����δ�������ֽ�
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t modbus_memory_rx_availble(void)
{
    /*������ݰ�buffer��������ˣ������㣬���¼���*/
    if (m_rx_ringbuffer.over_flow == 1)
    {
        ringbuffer_clear(&m_rx_ringbuffer);
    }
    return !ringbuffer_is_empty(&m_rx_ringbuffer);
}

// /****************************************************************************************************/
// /*��������Ӳ���ӿڲ�������Ĳ��֣����ݲ�ͬ�������Ĵ�����ʽ������ֲ*/
// /**
//   * @brief  ��ȡ���ռĴ����������ֵ
//   * @param
//   * @note
//   * @retval void
//   * @author xiaodaqi
//   */
// __weak uint8_t Modbus_Master_GetByte(uint8_t *getbyte)
// {
//     return 0;
// }

/**
  * @brief  �жϴ����������ڴ��ڽ����ж��е��ã����Ĵ�������ֵѹ�˻�����
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
void modbus_memory_serial_rx(uint8_t byte)
{
	ringbuffer_push(&m_rx_ringbuffer, (uint8_t)(byte & (uint8_t)0xFFU));
}

/**
  * @brief  ����������������
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t modbus_memory_get_byte(void)
{
    uint8_t cur = 0xff;
    if (!ringbuffer_is_empty(&m_rx_ringbuffer))
    {
        cur = ringbuffer_pop(&m_rx_ringbuffer);
    }
    return cur;
}

