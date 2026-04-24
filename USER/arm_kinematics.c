/**
 * @file    arm_kinematics.c
 * @brief   WeArm 逆运动学解算实现
 *
 * 算法来源：商家原始代码 x_kinematics.c，保留原始逻辑，
 * 仅做以下改动：
 *   1. 去除所有与串口/PS2/W25Q64 的依赖
 *   2. 连杆参数改为模块内静态变量，通过 arm_kinematics_init() 设置
 *   3. 增加入参有效性说明注释
 *
 * 舵机角度 → PWM 换算公式（商家原始）：
 *   servo[0] (底座)  pwm = 1500 - 2000 * angle / 270
 *   servo[1] (大臂)  pwm = 1500 + 2000 * angle / 270
 *   servo[2] (小臂)  pwm = 1500 + 2000 * angle / 270
 *   servo[3] (腕部)  pwm = 1500 - 2000 * angle / 270
 */

#include "arm_kinematics.h"
#include <math.h>

#define PI 3.14159265f

/* ------------------------------------------------------------------ */
/*  连杆参数（内部，放大 10 倍以减小浮点误差，与原始代码一致）         */
/* ------------------------------------------------------------------ */
static float s_L0 = 0.0f;
static float s_L1 = 0.0f;
static float s_L2 = 0.0f;
static float s_L3 = 0.0f;

void arm_kinematics_init(float L0, float L1, float L2, float L3)
{
    /* 与原始代码相同：存储前放大 10 倍 */
    s_L0 = L0 * 10.0f;
    s_L1 = L1 * 10.0f;
    s_L2 = L2 * 10.0f;
    s_L3 = L3 * 10.0f;
}

/* ------------------------------------------------------------------ */
/*  逆运动学核心                                                        */
/* ------------------------------------------------------------------ */
int arm_kinematics_solve(float x, float y, float z, float alpha,
                         arm_kin_result_t *result)
{
    float theta3, theta4, theta5, theta6;
    float aaa, bbb, ccc, zf_flag;

    /* 坐标同样放大 10 倍 */
    x *= 10.0f;
    y *= 10.0f;
    z *= 10.0f;

    /* ---------- 底座旋转角 theta6 ---------- */
    if (x == 0.0f) {
        theta6 = 0.0f;
    } else if (x > 0.0f && y < 0.0f) {
        theta6 = atanf(x / y);
        theta6 = 180.0f + (theta6 * 180.0f / PI);
    } else {
        theta6 = atanf(x / y) * 180.0f / PI;
        if (y < 0.0f) theta6 -= 180.0f;
    }

    /* ---------- 投影到 YZ 平面 ---------- */
    y = sqrtf(x * x + y * y);                          /* 水平距离    */
    y = y - s_L3 * cosf(alpha * PI / 180.0f);          /* 减去末端水平投影 */
    z = z - s_L0 - s_L3 * sinf(alpha * PI / 180.0f);  /* 减去底座+末端垂直投影 */

    if (z < -s_L0)                          return 1;  /* z 过低      */
    if (sqrtf(y*y + z*z) > (s_L1 + s_L2))  return 2;  /* 超出伸展极限 */

    /* ---------- 大臂角 theta5 ---------- */
    ccc = acosf(y / sqrtf(y * y + z * z));
    bbb = (y*y + z*z + s_L1*s_L1 - s_L2*s_L2) /
          (2.0f * s_L1 * sqrtf(y * y + z * z));
    if (bbb > 1.0f || bbb < -1.0f) return 5;

    zf_flag = (z < 0.0f) ? -1.0f : 1.0f;
    theta5  = (ccc * zf_flag + acosf(bbb)) * 180.0f / PI;
    if (theta5 > 180.0f || theta5 < 0.0f) return 6;

    /* ---------- 小臂角 theta4 ---------- */
    aaa = -(y*y + z*z - s_L1*s_L1 - s_L2*s_L2) / (2.0f * s_L1 * s_L2);
    if (aaa > 1.0f || aaa < -1.0f) return 3;
    theta4 = 180.0f - acosf(aaa) * 180.0f / PI;
    if (theta4 > 135.0f || theta4 < -135.0f) return 4;

    /* ---------- 腕部角 theta3 ---------- */
    theta3 = alpha - theta5 + theta4;
    if (theta3 > 90.0f || theta3 < -90.0f) return 7;

    /* ---------- 输出角度 ---------- */
    result->servo_angle[0] = theta6;
    result->servo_angle[1] = theta5 - 90.0f;
    result->servo_angle[2] = theta4;
    result->servo_angle[3] = theta3;

    /* ---------- 角度 → PWM（商家原始换算）---------- */
    result->servo_pwm[0] = 1500.0f - 2000.0f * result->servo_angle[0] / 270.0f;
    result->servo_pwm[1] = 1500.0f + 2000.0f * result->servo_angle[1] / 270.0f;
    result->servo_pwm[2] = 1500.0f + 2000.0f * result->servo_angle[2] / 270.0f;
    result->servo_pwm[3] = 1500.0f - 2000.0f * result->servo_angle[3] / 270.0f;

    return 0;
}
