#include "trans_recieve_buff_control.h"
#include "ringbuffer.h"

RingBuffer m_Modbus_Master_RX_RingBuff;
uint8_t m_Modbus_Master_RX_Buff[256];

/**
  * @brief  ��ʼ���жϽ��յĵ�ringbuffer���ζ�������,�жϽ��յ��ֽڶ���m_Modbus_Master_RX_RingBuff�ýṹ��ָ����й���
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t Modbus_Master_RB_Initialize(void)
{
    /*��ʼ��ringbuffer��ص�����*/
    rbInitialize(&m_Modbus_Master_RX_RingBuff, m_Modbus_Master_RX_Buff, sizeof(m_Modbus_Master_RX_Buff));
    return 1;
}

/**
  * @brief  ������ζ���
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
void Modbus_Master_Rece_Flush(void)
{
    rbClear(&m_Modbus_Master_RX_RingBuff);
}
/**
  * @brief  �ж�GPS��ringbuffer�����Ƿ�����δ�������ֽ�
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t Modbus_Master_Rece_Available(void)
{
    /*������ݰ�buffer��������ˣ������㣬���¼���*/
    if (m_Modbus_Master_RX_RingBuff.flagOverflow == 1)
    {
        rbClear(&m_Modbus_Master_RX_RingBuff);
    }
    return !rbIsEmpty(&m_Modbus_Master_RX_RingBuff);
}

/****************************************************************************************************/
/*��������Ӳ���ӿڲ�������Ĳ��֣����ݲ�ͬ�������Ĵ�����ʽ������ֲ*/
/**
  * @brief  ��ȡ���ռĴ����������ֵ
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
__weak uint8_t Modbus_Master_GetByte(uint8_t *getbyte)
{
    return 0;
}

/**
  * @brief  �жϴ����������ڴ��ڽ����ж��е��ã����Ĵ�������ֵѹ�˻�����
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t Modbus_Master_Rece_Handler(uint8_t byte)
{
	rbPush(&m_Modbus_Master_RX_RingBuff, (uint8_t)(byte & (uint8_t)0xFFU));
	return HAL_OK;
}

/**
  * @brief  ����������������
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
uint8_t Modbus_Master_Read(void)
{
    uint8_t cur = 0xff;
    if (!rbIsEmpty(&m_Modbus_Master_RX_RingBuff))
    {
        cur = rbPop(&m_Modbus_Master_RX_RingBuff);
    }
    return cur;
}

/**
  * @brief  �����ݰ����ͳ�ȥ
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
__weak void Modbus_Master_Write(uint8_t *buf, uint8_t length)
{
    #warning "Please implement modbus write"
}

/**
  * @brief  1ms���ڵĶ�ʱ��
  * @param
  * @note
  * @retval void
  * @author xiaodaqi
  */
__weak uint32_t Modbus_Master_Millis(void)
{
    return 0;
}


__weak void Modbus_Master_Sleep(void)
{
	
}
