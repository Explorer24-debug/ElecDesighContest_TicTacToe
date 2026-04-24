/**
 * @file    arm_ctrl.h
 * @brief   机械臂高层控制接口
 *
 * 这一层封装了"给我一个目标坐标，帮我搞定"的逻辑，
 * 用户代码只需调用这里的函数，不必关心逆运动学细节。
 *
 * 初始化顺序（必须严格遵守）：
 *   1. arm_ctrl_init()       ← 初始化所有硬件 + 运动学参数
 *   2. __enable_irq()        ← 开总中断（在 main 中调用）
 *   3. arm_ctrl_reset(2000)  ← 可选：上电后归中
 *   4. arm_ctrl_move_xyz(…)  ← 正常控制
 */

#ifndef ARM_CTRL_H
#define ARM_CTRL_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief  初始化机械臂控制系统
 *         内部依次初始化：RCC、GPIO（舵机复用推挽）、TIM3/TIM4（硬件PWM）、
 *         TIM2（缓动调度）、USART1（115200）、运动学参数。
 *         调用后务必执行 __enable_irq() 开总中断。
 */
void arm_ctrl_init(void);

/* ------------------------------------------------------------------ */
/*  运动控制                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief  控制机械臂末端运动到指定空间坐标（异步，立即返回）
 *
 * 内部逻辑：
 *   1. 在 Alpha ∈ [0, -135]° 范围内遍历，找到使腕部与水平夹角最大（最优姿态）
 *      的 Alpha 值（与商家原始 kinematics_move 逻辑完全一致）。
 *   2. 调用 arm_kinematics_solve() 解算各关节角。
 *   3. 调用 arm_servo_set_pwm_time() 驱动舵机。
 *
 * @param  x        末端 X 坐标 mm
 * @param  y        末端 Y 坐标 mm（机械臂正前方为正）
 * @param  z        末端 Z 坐标 mm（距地面高度）
 * @param  time_ms  运动时间 ms（建议 500~3000）
 * @return 1 = 目标可达，已发送运动指令；0 = 目标不可达
 */
int arm_ctrl_move_xyz(float x, float y, float z, uint32_t time_ms);

/**
 * @brief  带指定末端姿态角的运动（精确控制末端俯仰）
 *
 * @param  x        末端 X 坐标 mm
 * @param  y        末端 Y 坐标 mm
 * @param  z        末端 Z 坐标 mm
 * @param  alpha    末端与水平面夹角 °（负值表示向下，如 -45.0f）
 * @param  time_ms  运动时间 ms
 * @return 1 = 可达；0 = 不可达
 */
int arm_ctrl_move_xyz_alpha(float x, float y, float z,
                             float alpha, uint32_t time_ms);

/**
 * @brief  所有舵机归中（回到安全初始姿态）
 * @param  time_ms  归中时间 ms
 */
void arm_ctrl_reset(uint32_t time_ms);

/**
 * @brief  立即停止所有舵机（原地保持）
 */
void arm_ctrl_stop(void);

/* ------------------------------------------------------------------ */
/*  状态查询                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief  查询机械臂是否已到达上一次指令的目标位置
 * @return 1 = 到位；0 = 仍在运动
 */
uint8_t arm_ctrl_is_done(void);

/**
 * @brief  阻塞等待机械臂到位（配合 arm_delay_ms 轮询，不使用 RTOS）
 * @param  timeout_ms  超时 ms（0 = 永久等待）
 * @return 1 = 到位；0 = 超时
 */
uint8_t arm_ctrl_wait_done(uint32_t timeout_ms);

#endif /* ARM_CTRL_H */
