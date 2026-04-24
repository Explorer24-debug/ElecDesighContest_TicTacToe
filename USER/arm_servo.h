/**
 * @file    arm_servo.h
 * @brief   舵机硬件 PWM 驱动（TIM3 + TIM4，50 Hz）
 *
 * 工作原理
 * --------
 * 4 路舵机各占用一个定时器 PWM 通道，硬件自动输出 50 Hz 方波：
 *
 *   servo[0] (底座) PA6  → TIM3_CH1
 *   servo[1] (大臂) PA7  → TIM3_CH2
 *   servo[2] (小臂) PB6  → TIM4_CH1
 *   servo[3] (腕部) PB7  → TIM4_CH2
 *
 * 定时器配置：
 *   PSC = 72-1 → 1 MHz 计数频率
 *   ARR = 20000-1 → 50 Hz（20 ms 周期）
 *   CCR = 500~2500 → 脉宽 0.5 ms ~ 2.5 ms
 *
 * 缓动机制
 * --------
 * TIM2 每 20 ms 触发一次中断，线性插值更新各通道的 CCR 值。
 * 与软件 PWM 版本相比，TIM2 中断频率从 ~500 Hz 降到 50 Hz，
 * CPU 占用从 ~40% 降到 ~1%。
 *
 * 外部接口与软件 PWM 版本完全兼容，arm_ctrl.c 无需修改。
 */

#ifndef ARM_SERVO_H
#define ARM_SERVO_H

#include "stm32f10x.h"
#include "arm_config.h"

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief  初始化舵机 GPIO（PA6/PA7/PB6/PB7 复用推挽输出）
 *         不需要 AFIO 重映射，不影响 SWD 调试。
 */
void arm_servo_gpio_init(void);

/**
 * @brief  初始化硬件 PWM 定时器（TIM3 + TIM4）和缓动定时器（TIM2）
 *         调用后硬件 PWM 立即开始输出。
 *         务必在 arm_servo_gpio_init() 之后调用。
 */
void arm_servo_timer_init(void);

/* ------------------------------------------------------------------ */
/*  舵机控制接口（与软件 PWM 版本完全兼容）                            */
/* ------------------------------------------------------------------ */

/**
 * @brief  立即设置舵机 PWM（无缓动，瞬间到位）
 * @param  index  舵机编号 0~3
 * @param  pwm    目标脉宽 µs，范围 [500, 2500]
 */
void arm_servo_set_pwm(uint8_t index, uint16_t pwm);

/**
 * @brief  带时间缓动的舵机 PWM 设置（线性插值）
 * @param  index   舵机编号 0~3
 * @param  pwm     目标脉宽 µs，范围 [500, 2500]
 * @param  time_ms 运动时间 ms（>= 20 ms 才有缓动效果）
 */
void arm_servo_set_pwm_time(uint8_t index, uint16_t pwm, uint16_t time_ms);

/**
 * @brief  立即停止所有舵机运动（保持当前位置）
 */
void arm_servo_stop_all(void);

/**
 * @brief  将所有舵机复位到中位 1500 µs
 * @param  time_ms  复位时间 ms
 */
void arm_servo_reset_all(uint16_t time_ms);

/**
 * @brief  查询指定舵机是否已到达目标位置
 * @param  index  舵机编号 0~3
 * @return 1 = 已到位，0 = 运动中
 */
uint8_t arm_servo_is_done(uint8_t index);

/**
 * @brief  查询所有舵机是否全部到达目标位置
 * @return 1 = 全部到位，0 = 仍有舵机在运动
 */
uint8_t arm_servo_all_done(void);

/* ------------------------------------------------------------------ */
/*  内部 —— 供 TIM2_IRQHandler 调用（不要在用户代码中直接调用）       */
/* ------------------------------------------------------------------ */
void arm_servo_tim2_isr(void);

#endif /* ARM_SERVO_H */
