#include "modbus_master.h"
#include "modbus_memory.h"

static uint8_t m_slave_addr;                            ///< Modbus slave (1..255) initialized in begin()
static uint16_t m_read_addr;                      ///< slave register from which to read
static uint16_t m_read_qty;                          ///< quantity of words to read
static uint16_t m_response_buffer[MODBUS_MASTER_MAX_BUFFER_SIZE]; ///< buffer to store Modbus slave response; read via GetResponseBuffer()
static uint16_t m_wr_addr;                     ///< slave register to which to write
static uint16_t m_wr_qty;                         ///< quantity of words to write
static uint16_t m_transmit_buffer[MODBUS_MASTER_MAX_BUFFER_SIZE]; ///< buffer containing data to transmit to Modbus slave; set via SetTransmitBuffer()
static uint8_t m_transmit_buffer_index;
static uint16_t m_transmit_buffer_length;
static uint8_t m_response_buffer_index;
static uint8_t m_response_buffer_length;


// Modbus timeout [milliseconds]
static uint16_t m_response_timeout = MODBUS_MASTER_TIMEOUT_MS; ///< Modbus timeout [milliseconds]

// idle callback function; gets called during idle time between TX and RX
void (*_idle)();
// preTransmission callback function; gets called before writing a Modbus message
void (*_preTransmission)();
// postTransmission callback function; gets called after a Modbus message has been sent
void (*_postTransmission)();

void modbus_master_reset(uint16_t modbus_wait_timeout_ms)
{
    m_transmit_buffer_index = 0;
    m_transmit_buffer_length = 0;
    modbus_memory_ringbuffer_initialize();
	m_response_timeout = modbus_wait_timeout_ms;
}

void modbus_master_transmission(uint16_t address)
{
    m_wr_addr = address;
    m_transmit_buffer_index = 0;
    m_transmit_buffer_length = 0;
}

// eliminate this function in favor of using existing MB request functions
uint8_t modbus_master_request_from(uint16_t address, uint16_t quantity)
{
    uint8_t read;
    // clamp to buffer length
    if (quantity > MODBUS_MASTER_MAX_BUFFER_SIZE)
    {
        quantity = MODBUS_MASTER_MAX_BUFFER_SIZE;
    }
    // set rx buffer iterator vars
    m_response_buffer_index = 0;
    m_response_buffer_length = read;

    return read;
}

void modbus_master_send_bit(uint8_t data)
{
    uint8_t tx_bit_index = m_transmit_buffer_length % 16;
    if ((m_transmit_buffer_length >> 4) < MODBUS_MASTER_MAX_BUFFER_SIZE)
    {
        if (0 == tx_bit_index)
        {
            m_transmit_buffer[m_transmit_buffer_index] = 0;
        }
        WORD_BIT_WRITE(m_transmit_buffer[m_transmit_buffer_index], tx_bit_index, data);
        m_transmit_buffer_length++;
        m_transmit_buffer_index = m_transmit_buffer_length >> 4;
    }
}

void modbus_master_send_uint16(uint16_t data)
{
    if (m_transmit_buffer_index < MODBUS_MASTER_MAX_BUFFER_SIZE)
    {
        m_transmit_buffer[m_transmit_buffer_index++] = data;
        m_transmit_buffer_length = m_transmit_buffer_index << 4;
    }
}

void modbus_master_send_uint32(uint32_t data)
{
    modbus_master_send_uint16(word_get_low(data));
    modbus_master_send_uint16(word_get_high(data));
}

void modbus_master_send_byte(uint8_t data)
{
    modbus_master_send_uint16((uint16_t)(data));
}

uint8_t modbus_master_available(void)
{
    return m_response_buffer_length - m_response_buffer_index;
}

uint16_t modbus_master_receive(void)
{
    if (m_response_buffer_index < m_response_buffer_length)
    {
        return m_response_buffer[m_response_buffer_index++];
    }
    else
    {
        return 0xFFFF;
    }
}

/**
Retrieve data from response buffer.

@see ModbusMaster::clearResponseBuffer()
@param index index of response buffer array (0x00..0x3F)
@return value in position index of response buffer (0x0000..0xFFFF)
@ingroup buffer
*/
uint16_t modbus_master_get_response_buffer(uint8_t index)
{
    if (index < MODBUS_MASTER_MAX_BUFFER_SIZE)
    {
        return m_response_buffer[index];
    }
    else
    {
        return 0xFFFF;
    }
}

/**
Clear Modbus response buffer.

@see ModbusMaster::getResponseBuffer(uint8_t index)
@ingroup buffer
*/
void modbus_master_clear_response_buffer()
{
    uint8_t i;

    for (i = 0; i < MODBUS_MASTER_MAX_BUFFER_SIZE; i++)
    {
        m_response_buffer[i] = 0;
    }
}


/**
Place data in transmit buffer.

@see ModbusMaster::clearTransmitBuffer()
@param index index of transmit buffer array (0x00..0x3F)
@param value value to place in position index of transmit buffer (0x0000..0xFFFF)
@return 0 on success; exception number on failure
@ingroup buffer
*/
uint8_t modbus_master_set_transmit_buffer(uint8_t index, uint16_t value)
{
    if (index < MODBUS_MASTER_MAX_BUFFER_SIZE)
    {
        m_transmit_buffer[index] = value;
        return MODBUS_MASTER_ERROR_CODE_SUCCESS;
    }
    else
    {
        return MODBUS_MASTER_ERROR_CODE_ILLEGAL_ADDR;
    }
}

/**
Clear Modbus transmit buffer.

@see ModbusMaster::setTransmitBuffer(uint8_t index, uint16_t value)
@ingroup buffer
*/
void modbus_master_clear_transmit_buffer()
{
    uint8_t i;

    for (i = 0; i < MODBUS_MASTER_MAX_BUFFER_SIZE; i++)
    {
        m_transmit_buffer[i] = 0;
    }
}

/* _____PRIVATE FUNCTIONS____________________________________________________ */
/**
Modbus transaction engine.
Sequence:
  - assemble Modbus Request Application Data Unit (ADU),
    based on particular function called
  - transmit request over selected serial port
  - wait for/retrieve response
  - evaluate/disassemble response
  - return status (success/exception)

@param function_code Modbus function (0x01..0xFF)
@return 0 on success; exception number on failure
*/
uint8_t modbus_master_start_transaction(uint8_t function_code)
{
    uint8_t modbus_adu[256];
    uint8_t modbus_adu_size = 0;
    uint8_t i, qty;
    uint16_t crc;
    uint32_t u32StartTime;
    uint8_t bytes_left = 8;
    uint8_t mb_status = MODBUS_MASTER_ERROR_CODE_SUCCESS;

    // assemble Modbus Request Application Data Unit
    modbus_adu[modbus_adu_size++] = m_slave_addr;
    modbus_adu[modbus_adu_size++] = function_code;

    switch (function_code)
    {
    case MODBUS_MASTER_FUNCTION_READ_COILS:
    case MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT:
    case MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER:
    case MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER:
    case MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_read_addr);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_read_addr);
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_read_qty);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_read_qty);
        break;
    }

    switch (function_code)
    {
    case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_COILS:
    case MODBUS_MASTER_MASK_WRITE_REGISTER:
    case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS:
    case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_REGISTER:
    case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER:
    case MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_wr_addr);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_wr_addr);
        break;
    }

    switch (function_code)
    {
    case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_COILS:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_wr_qty);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_wr_qty);
        break;

    case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_REGISTER:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_transmit_buffer[0]);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_transmit_buffer[0]);
        break;

    case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_wr_qty);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_wr_qty);
        qty = (m_wr_qty % 8) ? ((m_wr_qty >> 3) + 1) : (m_wr_qty >> 3);
        modbus_adu[modbus_adu_size++] = qty;
        for (i = 0; i < qty; i++)
        {
            switch (i % 2)
            {
            case 0: // i is even
                modbus_adu[modbus_adu_size++] = word_get_low_byte(m_transmit_buffer[i >> 1]);
                break;

            case 1: // i is odd
                modbus_adu[modbus_adu_size++] = word_get_high_byte(m_transmit_buffer[i >> 1]);
                break;
            }
        }
        break;

    case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER:
    case MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_wr_qty);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_wr_qty);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_wr_qty << 1);

        for (i = 0; i < word_get_low_byte(m_wr_qty); i++)
        {
            modbus_adu[modbus_adu_size++] = word_get_high_byte(m_transmit_buffer[i]);
            modbus_adu[modbus_adu_size++] = word_get_low_byte(m_transmit_buffer[i]);
        }
        break;

    case MODBUS_MASTER_MASK_WRITE_REGISTER:
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_transmit_buffer[0]);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_transmit_buffer[0]);
        modbus_adu[modbus_adu_size++] = word_get_high_byte(m_transmit_buffer[1]);
        modbus_adu[modbus_adu_size++] = word_get_low_byte(m_transmit_buffer[1]);
        break;
    }

    // append CRC
    crc = 0xFFFF;
    for (i = 0; i < modbus_adu_size; i++)
    {
        crc = crc16_update(crc, modbus_adu[i]);
    }
    modbus_adu[modbus_adu_size++] = word_get_low_byte(crc);
    modbus_adu[modbus_adu_size++] = word_get_high_byte(crc);
    modbus_adu[modbus_adu_size] = 0;

    // flush receive buffer before transmitting request
    modbus_memory_flush_rx_buffer();

    // transmit request RS485�ӿ�����Ҫÿ�η���ǰ�ı�ӿڵ�ģʽ����Ȼ��ǯס���߶����ܷ��͵�ԭ��
    /*
  if (_preTransmission)
  {
    _preTransmission();
  }
	*/

    //���ڷ�������
    modbus_master_serial_write(modbus_adu, modbus_adu_size);
    modbus_adu_size = 0;
    /*
	  if (_postTransmission)
  {
    _postTransmission();
  }
	*/
    // loop until we run out of time or bytes, or an error occurs
    u32StartTime = modbus_master_get_milis();
    while (bytes_left && !mb_status)
    {
        if (modbus_memory_rx_availble())
        {
            modbus_adu[modbus_adu_size++] = modbus_memory_get_byte();
            bytes_left--;
        }
        else
        {
			modbus_master_sleep();
        }

        // evaluate slave ID, function code once enough bytes have been read
        if (modbus_adu_size == 5)
        {
            // verify response is for correct Modbus slave
            if (modbus_adu[0] != m_slave_addr)
            {
                mb_status = MODBUS_MASTER_ERROR_CODE_INVALID_SLAVE_ID;
                break;
            }

            // verify response is for correct Modbus function code (mask exception bit 7)
            if ((modbus_adu[1] & 0x7F) != function_code)
            {
                mb_status = MODBUS_MASTER_ERROR_CODE_INVALID_FUNCTION;
                break;
            }

            // check whether Modbus exception occurred; return Modbus Exception Code
            if (WORD_BIT_READ(modbus_adu[1], 7))
            {
                mb_status = modbus_adu[2];
                break;
            }

            // evaluate returned Modbus function code
            switch (modbus_adu[1])
            {
            case MODBUS_MASTER_FUNCTION_READ_COILS:
            case MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT:
            case MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER:
            case MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER:
            case MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER:
                bytes_left = modbus_adu[2];
                break;

            case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_COILS:
            case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS:
            case MODBUS_MASTER_FUNCTION_WRITE_SINGLE_REGISTER:
            case MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER:
                bytes_left = 3;
                break;

            case MODBUS_MASTER_MASK_WRITE_REGISTER:
                bytes_left = 5;
                break;
            }
        }
        if ((modbus_master_get_milis() - u32StartTime) > m_response_timeout)
        {
            mb_status = MODBUS_MASTER_ERROR_CODE_RESPONSE_TIMEOUT;
        }
    }

    // verify response is large enough to inspect further
    if (!mb_status && modbus_adu_size >= 5)
    {
        // calculate CRC
        crc = 0xFFFF;
        for (i = 0; i < (modbus_adu_size - 2); i++)
        {
            crc = crc16_update(crc, modbus_adu[i]);
        }

        // verify CRC
        if (!mb_status && (word_get_low_byte(crc) != modbus_adu[modbus_adu_size - 2] ||
                            word_get_high_byte(crc) != modbus_adu[modbus_adu_size - 1]))
        {
            mb_status = MODBUS_MASTER_ERROR_CODE_INVALID_CRC;
        }
    }

    // disassemble ADU into words
    if (!mb_status)
    {
        // evaluate returned Modbus function code
        switch (modbus_adu[1])
        {
        case MODBUS_MASTER_FUNCTION_READ_COILS:
        case MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT:
            // load bytes into word_make; response bytes are ordered L, H, L, H, ...
            for (i = 0; i < (modbus_adu[2] >> 1); i++)
            {
                if (i < MODBUS_MASTER_MAX_BUFFER_SIZE)
                {
                    m_response_buffer[i] = word_make(modbus_adu[2 * i + 4], modbus_adu[2 * i + 3]);
                }

                m_response_buffer_length = i;
            }

            // in the event of an odd number of bytes, load last byte into zero-padded word
            if (modbus_adu[2] % 2)
            {
                if (i < MODBUS_MASTER_MAX_BUFFER_SIZE)
                {
                    m_response_buffer[i] = word_make(0, modbus_adu[2 * i + 3]);
                }

                m_response_buffer_length = i + 1;
            }
            break;

        case MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER:
        case MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER:
        case MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER:
            // load bytes into word; response bytes are ordered H, L, H, L, ...
            for (i = 0; i < (modbus_adu[2] >> 1); i++)
            {
                if (i < MODBUS_MASTER_MAX_BUFFER_SIZE)
                {
                    m_response_buffer[i] = word_make(modbus_adu[2 * i + 3], modbus_adu[2 * i + 4]);
                }

                m_response_buffer_length = i;
            }
            break;
        }
    }

    m_transmit_buffer_index = 0;
    m_transmit_buffer_length = 0;
    m_response_buffer_index = 0;
    return mb_status;
}

/**
Modbus function 0x01 Read Coils.

This function code is used to read from 1 to 2000 contiguous status of 
coils in a remote device. The request specifies the starting address, 
i.e. the address of the first coil specified, and the number of coils. 
Coils are addressed starting at zero.

The coils in the response buffer are packed as one coil per bit of the 
data field. Status is indicated as 1=ON and 0=OFF. The LSB of the first 
data word contains the output addressed in the query. The other coils 
follow toward the high order end of this word and from low order to high 
order in subsequent words.

If the returned quantity is not a multiple of sixteen, the remaining 
bits in the final data word will be padded with zeros (toward the high 
order end of the word).

@param read_addr address of first coil (0x0000..0xFFFF)
@param bit_qty quantity of coils to read (1..2000, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup discrete
*/
uint8_t modbus_master_read_coils(uint8_t slave_addr, uint16_t read_addr, uint16_t bit_qty)
{
    m_slave_addr = slave_addr;
    m_read_addr = read_addr;
    m_read_qty = bit_qty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_READ_COILS);
}

/**
Modbus function 0x02 Read Discrete Inputs.

This function code is used to read from 1 to 2000 contiguous status of 
discrete inputs in a remote device. The request specifies the starting 
address, i.e. the address of the first input specified, and the number 
of inputs. Discrete inputs are addressed starting at zero.

The discrete inputs in the response buffer are packed as one input per 
bit of the data field. Status is indicated as 1=ON; 0=OFF. The LSB of 
the first data word contains the input addressed in the query. The other 
inputs follow toward the high order end of this word, and from low order 
to high order in subsequent words.

If the returned quantity is not a multiple of sixteen, the remaining 
bits in the final data word will be padded with zeros (toward the high 
order end of the word).

@param read_addr address of first discrete input (0x0000..0xFFFF)
@param bit_qty quantity of discrete inputs to read (1..2000, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup discrete
*/
uint8_t modbus_master_read_discrete_input(uint8_t slave_addr, uint16_t read_addr,
                                        uint16_t bit_qty)
{
    m_slave_addr = slave_addr;
    m_read_addr = read_addr;
    m_read_qty = bit_qty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT);
}

/**
Modbus function 0x03 Read Holding Registers.

This function code is used to read the contents of a contiguous block of 
holding registers in a remote device. The request specifies the starting 
register address and the number of registers. Registers are addressed 
starting at zero.

The register data in the response buffer is packed as one word per 
register.

@param read_addr address of the first holding register (0x0000..0xFFFF)
@param read_qty quantity of holding registers to read (1..125, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_read_holding_register(uint8_t slave_addr, uint16_t read_addr,
                                          uint16_t read_qty)
{
    m_slave_addr = slave_addr;
    m_read_addr = read_addr;
    m_read_qty = read_qty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER);
}

/**
Modbus function 0x04 Read Input Registers.

This function code is used to read from 1 to 125 contiguous input 
registers in a remote device. The request specifies the starting 
register address and the number of registers. Registers are addressed 
starting at zero.

The register data in the response buffer is packed as one word per 
register.

@param read_addr address of the first input register (0x0000..0xFFFF)
@param read_qty quantity of input registers to read (1..125, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_read_input_register(uint8_t slave_addr, uint16_t read_addr,
                                        uint8_t read_qty)
{
    m_slave_addr = slave_addr;
    m_read_addr = read_addr;
    m_read_qty = read_qty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER);
}

/**
Modbus function 0x05 Write Single Coil.

This function code is used to write a single output to either ON or OFF 
in a remote device. The requested ON/OFF state is specified by a 
constant in the state field. A non-zero value requests the output to be 
ON and a value of 0 requests it to be OFF. The request specifies the 
address of the coil to be forced. Coils are addressed starting at zero.

@param write_addr address of the coil (0x0000..0xFFFF)
@param u8State 0=OFF, non-zero=ON (0x00..0xFF)
@return 0 on success; exception number on failure
@ingroup discrete
*/
uint8_t modbus_master_write_single_coil(uint8_t slave_addr, uint16_t write_addr, uint8_t u8State)
{
    m_slave_addr = slave_addr;
    m_wr_addr = write_addr;
    m_wr_qty = (u8State ? 0xFF00 : 0x0000);
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_SINGLE_COILS);
}

/**
Modbus function 0x06 Write Single Register.

This function code is used to write a single holding register in a 
remote device. The request specifies the address of the register to be 
written. Registers are addressed starting at zero.

@param write_addr address of the holding register (0x0000..0xFFFF)
@param write_value value to be written to holding register (0x0000..0xFFFF)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_write_single_register(uint8_t slave_addr, uint16_t write_addr,
                                         uint16_t write_value)
{
    m_slave_addr = slave_addr;
    m_wr_addr = write_addr;
    m_wr_qty = 0;
    m_transmit_buffer[0] = write_value;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_SINGLE_REGISTER);
}

/**
Modbus function 0x0F Write Multiple Coils.

This function code is used to force each coil in a sequence of coils to 
either ON or OFF in a remote device. The request specifies the coil 
references to be forced. Coils are addressed starting at zero.

The requested ON/OFF states are specified by contents of the transmit 
buffer. A logical '1' in a bit position of the buffer requests the 
corresponding output to be ON. A logical '0' requests it to be OFF.

@param write_addr address of the first coil (0x0000..0xFFFF)
@param bit_qty quantity of coils to write (1..2000, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup discrete
*/
uint8_t modbus_master_write_multiple_coils(uint8_t slave_addr, uint16_t write_addr,
                                        uint16_t bit_qty)
{
    m_slave_addr = slave_addr;
    m_wr_addr = write_addr;
    m_wr_qty = bit_qty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS);
}
/*
uint8_t modbus_master_write_multiple_coils()
{
  m_wr_qty = m_transmit_buffer_length;
  return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS);
}
*/

/**
Modbus function 0x10 Write Multiple Registers.

This function code is used to write a block of contiguous registers (1 
to 123 registers) in a remote device.

The requested written values are specified in the transmit buffer. Data 
is packed as one word per register.

@param write_addr address of the holding register (0x0000..0xFFFF)
@param u16WriteQty quantity of holding registers to write (1..123, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_write_multiple_registers(uint8_t slave_addr, uint16_t write_addr,
                                            uint16_t u16WriteQty)
{
    m_slave_addr = slave_addr;
    m_wr_addr = write_addr;
    m_wr_qty = u16WriteQty;
    return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER);
}

// new version based on Wire.h

/*uint8_t modbus_master_write_multiple_registers()
{
  m_wr_qty = m_transmit_buffer_index;
  return modbus_master_start_transaction(MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER);
}
*/

/**
Modbus function 0x16 Mask Write Register.

This function code is used to modify the contents of a specified holding 
register using a combination of an AND mask, an OR mask, and the 
register's current contents. The function can be used to set or clear 
individual bits in the register.

The request specifies the holding register to be written, the data to be 
used as the AND mask, and the data to be used as the OR mask. Registers 
are addressed starting at zero.

The function's algorithm is:

Result = (Current Contents && And_Mask) || (Or_Mask && (~And_Mask))

@param write_addr address of the holding register (0x0000..0xFFFF)
@param and_mask AND mask (0x0000..0xFFFF)
@param or_mask OR mask (0x0000..0xFFFF)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_mask_write_registers(uint8_t slave_addr, uint16_t write_addr,
                                       uint16_t and_mask, uint16_t or_mask)
{
    m_slave_addr = slave_addr;
    m_wr_addr = write_addr;
    m_transmit_buffer[0] = and_mask;
    m_transmit_buffer[1] = or_mask;
    return modbus_master_start_transaction(MODBUS_MASTER_MASK_WRITE_REGISTER);
}

/**
Modbus function 0x17 Read Write Multiple Registers.

This function code performs a combination of one read operation and one 
write operation in a single MODBUS transaction. The write operation is 
performed before the read. Holding registers are addressed starting at 
zero.

The request specifies the starting address and number of holding 
registers to be read as well as the starting address, and the number of 
holding registers. The data to be written is specified in the transmit 
buffer.

@param read_addr address of the first holding register (0x0000..0xFFFF)
@param read_qty quantity of holding registers to read (1..125, enforced by remote device)
@param write_addr address of the first holding register (0x0000..0xFFFF)
@param u16WriteQty quantity of holding registers to write (1..121, enforced by remote device)
@return 0 on success; exception number on failure
@ingroup register
*/
uint8_t modbus_master_read_write_multiple_registers(uint8_t slave_addr, uint16_t read_addr,
                                                uint16_t read_qty, uint16_t write_addr, uint16_t u16WriteQty)
{
    m_slave_addr = slave_addr;
    m_read_addr = read_addr;
    m_read_qty = read_qty;
    m_wr_addr = write_addr;
    m_wr_qty = u16WriteQty;
    return modbus_master_start_transaction(MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER);
}
/*
uint8_t modbus_master_read_write_multiple_registers(uint16_t read_addr,
  uint16_t read_qty)
{
  m_read_addr = read_addr;
  m_read_qty = read_qty;
  m_wr_qty = m_transmit_buffer_index;
  return modbus_master_start_transaction(MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER);
}
*/
