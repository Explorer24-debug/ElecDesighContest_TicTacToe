/**
 * @file    arm_usart.h
 * @brief   USART1 驱动接口
 *
 * 功能：
 *   - 初始化 USART1（PA9 TX, PA10 RX, 115200）
 *   - 发送字符串（调试输出）
 *   - 发送单字节
 *   - 轮询单字节接收（主循环调用，无中断依赖）
 *
 * 引脚：
 *   USART1_TX  PA9
 *   USART1_RX  PA10
 *
 * TODO(队友): 如果需要中断接收 OpenMV 数据，
 *            可在此模块添加 RXNE 中断处理。
 */

#ifndef ARM_USART_H
#define ARM_USART_H

#include "stm32f10x.h"

/**
 * @brief  初始化 USART1（在 arm_ctrl_init() 中已调用）
 */
void arm_usart1_init(void);

/**
 * @brief  发送字符串（以 \0 结尾）
 */
void arm_usart1_print(const char *str);

/**
 * @brief  发送单字节
 */
void arm_usart1_send(uint8_t byte);

/**
 * @brief  轮询接收一个字节（非阻塞）
 * @return 收到的字符 ASCII 值，无数据返回 0
 *
 * 主循环中调用，读取后自动清除 RXNE 标志。
 */
uint16_t arm_usart1_getchar(void);

#endif /* ARM_USART_H */
