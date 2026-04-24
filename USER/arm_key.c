/**
 * @file    arm_key.c
 * @brief   简单按键驱动实现
 */

#include "arm_key.h"
#include "arm_config.h"
#include "arm_delay.h"

void arm_key_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* KEY1 = PA8 */
    RCC_APB2PeriphClockCmd(KEY1_GPIO_RCC, ENABLE);
    GPIO_InitStruct.GPIO_Pin  = KEY1_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU; /* 内部上拉 */
    GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStruct);

    /* KEY2 = PA11 */
    RCC_APB2PeriphClockCmd(KEY2_GPIO_RCC, ENABLE);
    GPIO_InitStruct.GPIO_Pin  = KEY2_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStruct);
}

uint8_t arm_key1_pressed(void)
{
    if (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == Bit_RESET) {
        arm_delay_ms(20); /* 消抖 */
        if (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == Bit_RESET) {
            /* 等待松开 */
            while (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == Bit_RESET) {
                arm_delay_ms(10);
            }
            return 1;
        }
    }
    return 0;
}

uint8_t arm_key2_pressed(void)
{
    if (GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == Bit_RESET) {
        arm_delay_ms(20);
        if (GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == Bit_RESET) {
            while (GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == Bit_RESET) {
                arm_delay_ms(10);
            }
            return 1;
        }
    }
    return 0;
}
