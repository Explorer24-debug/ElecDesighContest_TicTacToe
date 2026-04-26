/**
 * @file    arm_config.h
 * @brief   WeArm 机械臂 + 井字棋 全局硬件配置
 *
 * 引脚分配（STM32F103C8T6，72 MHz）
 * ---------------------------------
 *   舵机 0 底座  PA6  TIM3_CH1   硬件 PWM 50Hz
 *   舵机 1 大臂  PA7  TIM3_CH2
 *   舵机 2 小臂  PB6  TIM4_CH1
 *   舵机 3 腕部  PB7  TIM4_CH2
 *
 *   电磁铁      PB5  GPIO 推挽（驱动继电器线圈）
 *   USART1 TX   PA9  OpenMV 通信
 *   USART1 RX   PA10
 *   KEY1        PA8  先后手选择
 *   KEY2        PA11 防篡改测试
 *   SWD         PA13/PA14
 *
 * 不使用 AFIO 重映射。
 */

#ifndef ARM_CONFIG_H
#define ARM_CONFIG_H

#include "stm32f10x.h"

/* ================================================================== */
/*  连杆长度 (mm)                                                      */
/* ================================================================== */
#define ARM_L0   90.0f
#define ARM_L1  105.0f
#define ARM_L2   98.0f
#define ARM_L3  150.0f

/* ================================================================== */
/*  舵机数量                                                            */
/* ================================================================== */
#define ARM_SERVO_NUM   4

/* ================================================================== */
/*  PWM 脉宽范围 (us)                                                  */
/* ================================================================== */
#define ARM_PWM_MIN   500
#define ARM_PWM_MAX  2500
#define ARM_PWM_MID  1500

/* ================================================================== */
/*  硬件 PWM 定时器配置（TIM3 + TIM4，50 Hz）                          */
/* ================================================================== */
#define ARM_PWM_PSC      (72 - 1)
#define ARM_PWM_ARR      (20000 - 1)

/* ================================================================== */
/*  缓动定时器 TIM2（每 20ms 中断一次更新 CCR）                          */
/* ================================================================== */
#define ARM_EASE_PSC     (7200 - 1)
#define ARM_EASE_ARR     (200 - 1)

/* ================================================================== */
/*  USART1 波特率                                                       */
/* ================================================================== */
#define ARM_USART1_BAUD  115200U

/* ================================================================== */
/*  电磁铁配置                                                          */
/* ================================================================== */
#define MAGNET_GPIO_RCC  RCC_APB2Periph_GPIOB
#define MAGNET_GPIO_PORT GPIOB
#define MAGNET_GPIO_PIN  GPIO_Pin_5

/* ================================================================== */
/*  按键配置                                                            */
/* ================================================================== */
#define KEY1_GPIO_RCC    RCC_APB2Periph_GPIOA
#define KEY1_GPIO_PORT   GPIOA
#define KEY1_GPIO_PIN    GPIO_Pin_8

#define KEY2_GPIO_RCC    RCC_APB2Periph_GPIOA
#define KEY2_GPIO_PORT   GPIOA
#define KEY2_GPIO_PIN    GPIO_Pin_11

/* ================================================================== */
/*  棋盘物理坐标（需根据实际标定修改！）                                  */
/*  编号 1~9 对应棋盘 3x3 格子                                         */
/*  每个坐标：(x_mm, y_mm)                                             */
/*  z 坐标由动作函数根据悬停/放子高度动态计算                             */
/* ================================================================== */
#define BOARD_Z_SAFE      70.0f   /* 安全悬停高度 mm（下降前先悬停）       */
#define BOARD_Z_CRUISE    40.0f   /* 巡航高度 mm（平移时保持，避免碰到摄像头）*/
#define BOARD_Z_DOWN      25.0f   /* 放子高度 mm（电磁铁断电释放棋子）     */
#define BOARD_Z_PICKUP    25.0f   /* 取子高度 mm（电磁铁通电吸住棋子）     */

/*
 * 过渡点坐标（远距离移动时先经过此点，收缩手臂避开摄像头）
 * x/y 设为机械臂正前方近处，z 用巡航高度
 */
#define WAYPOINT_X    0.0f
#define WAYPOINT_Y   60.0f

/*
 * 以下坐标需要用示教法标定！
 * 坐标系：Y 轴指向机械臂正前方，X 轴向右，Z 轴向上。
 */
#define GRID_P1_X    30.0f    /* 左上 */
#define GRID_P1_Y   150.0f
#define GRID_P2_X     0.0f    /* 中上 */
#define GRID_P2_Y   150.0f
#define GRID_P3_X   -30.0f    /* 右上 */
#define GRID_P3_Y   150.0f
#define GRID_P4_X    30.0f    /* 左中 */
#define GRID_P4_Y   180.0f
#define GRID_P5_X     0.0f    /* 正中 */
#define GRID_P5_Y   180.0f
#define GRID_P6_X   -30.0f    /* 右中 */
#define GRID_P6_Y   180.0f
#define GRID_P7_X    30.0f    /* 左下 */
#define GRID_P7_Y   210.0f
#define GRID_P8_X     0.0f    /* 中下 */
#define GRID_P8_Y   210.0f
#define GRID_P9_X   -30.0f    /* 右下 */
#define GRID_P9_Y   210.0f

/* 机器方棋子备用位置（从哪取白棋） */
#define STOCK_X      90.0f
#define STOCK_Y     120.0f

#endif /* ARM_CONFIG_H */
