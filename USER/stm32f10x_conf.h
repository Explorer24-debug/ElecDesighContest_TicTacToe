/**
 * @file    stm32f10x_conf.h
 * @brief   STM32F10x 标准外设库头文件选择
 *
 * WeArm_Chess 版本使用的外设：
 *   - GPIO：舵机 PWM + 电磁铁 + 按键 + USART1
 *   - RCC：时钟配置
 *   - TIM：TIM2（缓动）+ TIM3/TIM4（硬件 PWM）
 *   - USART1：调试输出 + OpenMV 通信
 */

#ifndef STM32F10X_CONF_H
#define STM32F10X_CONF_H

/* ---- 必须使用的外设 ---- */
#include "stm32f10x_gpio.h"     /* GPIO                  */
#include "stm32f10x_rcc.h"      /* RCC 时钟              */
#include "stm32f10x_tim.h"      /* TIM2/TIM3/TIM4        */
#include "stm32f10x_usart.h"    /* USART1 调试 + OpenMV   */
#include "misc.h"               /* NVIC / SysTick        */

/* ---- Cortex-M3 断言支持（可选，调试期间打开） ---- */
/* #define USE_FULL_ASSERT */

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) \
      ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif

#endif /* STM32F10X_CONF_H */
