/**
 * @file    arm_kinematics.h
 * @brief   WeArm 逆运动学解算接口
 *
 * 坐标系定义
 * ----------
 *                Z (向上)
 *                |
 *                |_____ Y (机械臂正前方)
 *               /
 *              X (向右)
 *
 * 参数说明
 * --------
 *  x, y, z  — 末端目标位置，单位 mm
 *  Alpha     — 末端与水平面夹角，单位 °（负值 = 向下抓取，-25~-65 较佳）
 *
 * 函数返回值（kinematics_solve 的错误码）
 * ----------------------------------------
 *  0  — 成功，servo_pwm[] 已填充
 *  1  — z 过低，低于底座高度
 *  2  — 目标超出大臂+小臂伸展极限
 *  3  — 余弦定理超界（theta4 无解）
 *  4  — theta4 超出 ±135° 机械限位
 *  5  — 余弦定理超界（theta5 无解）
 *  6  — theta5 超出 0~180° 范围
 *  7  — theta3 超出 ±90° 范围
 */

#ifndef ARM_KINEMATICS_H
#define ARM_KINEMATICS_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------ */
/*  结果结构体                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    float servo_angle[4];   /* 0:底座  1:大臂  2:小臂  3:腕部 (°)   */
    float servo_pwm[4];     /* 对应 PWM 脉宽 µs                      */
} arm_kin_result_t;

/* ------------------------------------------------------------------ */
/*  初始化（设置连杆长度）                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief  设置机械臂连杆参数
 * @param  L0  底座高度 mm
 * @param  L1  大臂长度 mm
 * @param  L2  小臂长度 mm
 * @param  L3  腕部到末端 mm
 *
 * 必须在调用 kinematics_solve 前执行一次。
 */
void arm_kinematics_init(float L0, float L1, float L2, float L3);

/* ------------------------------------------------------------------ */
/*  核心解算函数                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief  逆运动学解算（保留商家原始算法）
 * @param  x       末端 X 坐标 mm
 * @param  y       末端 Y 坐标 mm
 * @param  z       末端 Z 坐标 mm（相对地面）
 * @param  alpha   末端俯仰角 °（通常为负）
 * @param  result  输出：各舵机角度与 PWM 值
 * @return 0 = 成功；其他 = 错误码（见上方说明）
 */
int arm_kinematics_solve(float x, float y, float z, float alpha,
                         arm_kin_result_t *result);

#endif /* ARM_KINEMATICS_H */
