#include "board_hw.h"
#include "gd32e10x.h"
#include "gd32e10x_usart.h"

void board_hw_init_gpio(void)
{
//    rcu_periph_clock_enable(RCU_GPIOA);
//    rcu_periph_clock_enable(RCU_GPIOB);
//    rcu_periph_clock_enable(RCU_GPIOC);
//    rcu_periph_clock_enable(RCU_GPIOF);
//    /* Enable the CFGCMP clock */
//    rcu_periph_clock_enable(RCU_CFGCMP);
//    
//    /* Step 1 Init LoRa DIO pin */
//    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_11);
//    
//    /* Step 2 Init LoRa Reset pin */
//    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_12);
//    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
//    gpio_bit_reset(GPIOB, GPIO_PIN_12);
//            
//    // Init lora nss pin
//    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_4);
//    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4);
//    gpio_bit_set(GPIOA, GPIO_PIN_4);
//    
//    /* Step 3 LoRa dio1-5 pin */
//    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13);
//    
//    /* Step 4 Init relays pin */
//    // Feedback
//    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_4 | GPIO_PIN_5);
//    
//    // Latch, Lat1, lat2
//    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_3);
//    gpio_bit_write(GPIOB, GPIO_PIN_3, 0);
//    
//    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_4 | GPIO_PIN_5);    
//    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4 | GPIO_PIN_5);
//    gpio_bit_write(GPIOF, GPIO_PIN_6, gpio_output_bit_get(GPIOB, GPIO_PIN_5));
//    gpio_bit_write(GPIOF, GPIO_PIN_7, gpio_output_bit_get(GPIOB, GPIO_PIN_4));
//    
//    /* Step 5 Init led pin */
//    gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_13);    
//    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
//    gpio_bit_write(GPIOB, GPIO_PIN_5, 0);
//    
//    /* Step 6 Init uart debug  : Not used */
//    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
//    
//    /* Step  7 : Interrupt on phase and contactor detect */
//    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_12 | GPIO_PIN_15);
//    /* enable and set key EXTI interrupt to the specified priority */
//    nvic_irq_enable(EXTI4_15_IRQn, 0U);
//    /* connect key EXTI line to key GPIO pin */
//    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN12 | EXTI_SOURCE_PIN15);
//    /* configure key EXTI line */
//    exti_init(EXTI_12 | EXTI_15, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
//    exti_interrupt_flag_clear(EXTI_12 | EXTI_15);
//    
//    /* Step 8 Config unuse pin as input */
//    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
//    
//    /* Step 9 Config button on node to toggle output state */
//    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_9);
//    
//    /* Step 10 Button pair gateway */
//    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_10);
}

/**
 * @brief               Read DIO gpio pin 
 * @retval              gpio level (0-1)
 */
uint32_t board_hw_read_dio(void)
{
    return gpio_input_bit_get(GPIOB, GPIO_PIN_0);
}


void board_hw_reset(void)
{
    NVIC_SystemReset();
}

void board_hw_uart_debug_initialize(void)
{
//    uint32_t COM_ID;
//    
//    /* enable COM GPIO clock */
//    rcu_periph_clock_enable(RCU_GPIOA);

//    /* enable USART clock */
//    rcu_periph_clock_enable(RCU_USART1);

//    /* connect port to USARTx_Tx */
//    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_2);

//    /* connect port to USARTx_Rx */
//    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_3);

//    /* configure USART Tx as alternate function push-pull */
//    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
//    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);

//    /* configure USART Rx as alternate function push-pull */
//    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_3);
//    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_3);

//    /* USART configure */
//    usart_deinit(USART1);
//    usart_baudrate_set(USART1, 115200U);
//    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
//    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);

//    usart_enable(USART1);
//    usart_interrupt_enable(USART1, USART_INT_RBNE);
}
