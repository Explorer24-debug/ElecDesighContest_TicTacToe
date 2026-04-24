/**
 * @file    arm_key.h
 * @brief   简单按键驱动
 *
 * KEY1（PA8）：先后手选择 —— 按下 = 机器先手
 * KEY2（PA11）：防篡改测试 —— 按下 = 模拟棋盘被篡改
 *
 * 使用上拉输入 + 软件消抖（简单延时）。
 */

#ifndef ARM_KEY_H
#define ARM_KEY_H

#include "stm32f10x.h"

/**
 * @brief  初始化按键 GPIO（上拉输入）
 */
void arm_key_init(void);

/**
 * @brief  检测 KEY1 是否按下（阻塞消抖）
 * @return 1 = 按下，0 = 未按下
 */
uint8_t arm_key1_pressed(void);

/**
 * @brief  检测 KEY2 是否按下（阻塞消抖）
 * @return 1 = 按下，0 = 未按下
 */
uint8_t arm_key2_pressed(void);

#endif /* ARM_KEY_H */
