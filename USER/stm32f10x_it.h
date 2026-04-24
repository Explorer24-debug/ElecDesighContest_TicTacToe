/**
 * @file    stm32f10x_it.h
 * @brief   中断服务函数声明
 *
 * v2 只需要 TIM2 中断（舵机缓动）。
 * USART 中断已移除（调试模式用轮询）。
 *
 * TODO(队友): 接入 OpenMV 后如需中断接收，在此声明 USART1_IRQHandler。
 */

#ifndef __STM32F10X_IT_H
#define __STM32F10X_IT_H

#include "stm32f10x.h"

void TIM2_IRQHandler(void);

/* TODO(队友): 如需 USART1 中断接收 OpenMV 数据，取消下行注释 */
/* void USART1_IRQHandler(void); */

#endif /* __STM32F10X_IT_H */
