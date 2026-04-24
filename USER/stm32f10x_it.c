/**
 * @file    stm32f10x_it.c
 * @brief   中断服务函数实现
 *
 * v2: 只保留 TIM2 中断（舵机缓动）。
 * USART 中断已移除，调试模式使用轮询。
 *
 * TODO(队友): 接入 OpenMV 后在此添加 USART1_IRQHandler。
 */

#include "stm32f10x_it.h"
#include "arm_servo.h"

/* ================================================================== */
/*  TIM2 中断：每 20ms 更新舵机 CCR（缓动插值）                         */
/* ================================================================== */
void TIM2_IRQHandler(void)
{
    arm_servo_tim2_isr();   /* 内部已检查 + 清除 TIM2 Update 标志 */
}

/* ================================================================== */
/*  TODO(队友): USART1 中断处理                                        */
/* ================================================================== */
/*
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t data = USART_ReceiveData(USART1);
        // TODO: 处理 OpenMV 数据
        // 例如存入环形缓冲区，在主循环中解析
    }
}
*/
