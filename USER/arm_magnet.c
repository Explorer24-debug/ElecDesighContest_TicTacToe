/**
 * @file    arm_magnet.c
 * @brief   电磁铁 GPIO 驱动实现
 */

#include "arm_magnet.h"
#include "arm_config.h"

static uint8_t s_magnet_on = 0;

void arm_magnet_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(MAGNET_GPIO_RCC, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = MAGNET_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz; /* 2MHz 足够驱动继电器 */
    GPIO_Init(MAGNET_GPIO_PORT, &GPIO_InitStruct);

    /* 上电默认断电 */
    GPIO_ResetBits(MAGNET_GPIO_PORT, MAGNET_GPIO_PIN);
    s_magnet_on = 0;
}

void arm_magnet_on(void)
{
    GPIO_SetBits(MAGNET_GPIO_PORT, MAGNET_GPIO_PIN);
    s_magnet_on = 1;
}

void arm_magnet_off(void)
{
    GPIO_ResetBits(MAGNET_GPIO_PORT, MAGNET_GPIO_PIN);
    s_magnet_on = 0;
}

uint8_t arm_magnet_state(void)
{
    return s_magnet_on;
}
