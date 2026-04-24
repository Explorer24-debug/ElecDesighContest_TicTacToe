/**
 * @file    arm_usart.c
 * @brief   USART1 驱动实现（轮询收发）
 *
 * v2 改动：
 *   - 简化为纯轮询模式，移除 IDLE 中断
 *   - 新增 arm_usart1_getchar() 供主循环逐字符读取
 *   - 调试模式下不需要复杂的帧协议，直接处理 ASCII 字符
 *
 * TODO(队友): 如果 OpenMV 需要中断接收，
 *            在此文件中添加 NVIC 配置和 USART1_IRQHandler。
 *            可以参考 WeArm_Chess/v1 版本的 arm_usart.c。
 */

#include "arm_usart.h"
#include "arm_config.h"
#include <stdio.h>

/* ================================================================== */
/*  内部：fputc 重定向（支持 printf）                                    */
/* ================================================================== */
int fputc(int ch, FILE *f)
{
    (void)f;
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint16_t)ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    return ch;
}

/* ================================================================== */
/*  初始化                                                              */
/* ================================================================== */
void arm_usart1_init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    USART_InitTypeDef       USART_InitStructure;

    /* ---- TX: PA9  复用推挽 ---- */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ---- RX: PA10  浮空输入 ---- */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ---- USART1 配置 ---- */
    USART_InitStructure.USART_BaudRate            = ARM_USART1_BAUD;
    USART_InitStructure.USART_WordLength           = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits             = USART_StopBits_1;
    USART_InitStructure.USART_Parity               = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                 = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

/* ================================================================== */
/*  发送接口                                                            */
/* ================================================================== */
void arm_usart1_print(const char *str)
{
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, (uint16_t)*str);
        str++;
    }
}

void arm_usart1_send(uint8_t byte)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint16_t)byte);
}

/* ================================================================== */
/*  轮询接收：非阻塞                                                    */
/* ================================================================== */
uint16_t arm_usart1_getchar(void)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
        return (uint16_t)USART_ReceiveData(USART1);
    }
    return 0;
}
