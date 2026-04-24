/**
 * @file    arm_magnet.h
 * @brief   电磁铁控制接口（通过继电器驱动）
 *
 * 接线：PB5 → 继电器控制端
 *       继电器常开端/常闭端 → 电磁铁
 *       继电器电源由外部供电
 *
 * 默认逻辑：GPIO 置高 → 继电器吸合 → 电磁铁通电（吸住棋子）
 *           GPIO 置低 → 继电器释放 → 电磁铁断电（松开棋子）
 */

#ifndef ARM_MAGNET_H
#define ARM_MAGNET_H

#include "stm32f10x.h"

/**
 * @brief  初始化电磁铁 GPIO（PB5 推挽输出，默认低电平=断电）
 */
void arm_magnet_init(void);

/**
 * @brief  电磁铁通电（吸住棋子）
 */
void arm_magnet_on(void);

/**
 * @brief  电磁铁断电（松开棋子）
 */
void arm_magnet_off(void);

/**
 * @brief  查询电磁铁当前状态
 * @return 1 = 通电中；0 = 断电
 */
uint8_t arm_magnet_state(void);

#endif /* ARM_MAGNET_H */
