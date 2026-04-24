/**
 * @file    arm_delay.c
 * @brief   软件延时实现（72 MHz HSE + PLL）
 *
 * 延时精度依赖编译优化等级；Release (-O2) 下误差 < 5%。
 * 如需高精度，可改用 DWT 计数器。
 */

#include "arm_delay.h"

/* ------------------------------------------------------------------ */
/*  内部基础延时                                                         */
/*  72 MHz 下约 6 个 NOP ≈ 1 µs                                        */
/* ------------------------------------------------------------------ */
static void _delay_unit(uint16_t t)
{
    while (t--);
}

void arm_delay_us(uint32_t us)
{
    while (us--) {
        _delay_unit(6);
    }
}

void arm_delay_ms(uint32_t ms)
{
    while (ms--) {
        arm_delay_us(1000);
    }
}
