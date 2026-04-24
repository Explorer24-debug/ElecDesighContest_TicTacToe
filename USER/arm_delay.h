/**
 * @file    arm_delay.h
 * @brief   软件延时接口
 *
 * 基于 72 MHz 主频的纯软件循环延时，适合短时等待场景。
 * 长时等待（> 10 ms）建议使用 SysTick。
 */

#ifndef ARM_DELAY_H
#define ARM_DELAY_H

#include "stm32f10x.h"

/**
 * @brief  微秒级延时（精度约 ±1 µs，72 MHz）
 * @param  us  延时微秒数
 */
void arm_delay_us(uint32_t us);

/**
 * @brief  毫秒级延时
 * @param  ms  延时毫秒数
 */
void arm_delay_ms(uint32_t ms);

#endif /* ARM_DELAY_H */
