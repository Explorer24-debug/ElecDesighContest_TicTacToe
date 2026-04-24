/**
 * @file    arm_ctrl.c
 * @brief   机械臂高层控制实现
 *
 * 注意：arm_ctrl.c 的控制逻辑与软件 PWM 版本完全相同，
 * 无需修改——arm_servo 模块的外部接口保持兼容。
 */

#include "arm_ctrl.h"
#include "arm_config.h"
#include "arm_delay.h"
#include "arm_servo.h"
#include "arm_kinematics.h"
#include "arm_usart.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  私有：RCC 初始化（HSE + PLL → 72 MHz，与商家原始一致）            */
/* ------------------------------------------------------------------ */
static void _rcc_init(void)
{
    ErrorStatus hse_status;

    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    hse_status = RCC_WaitForHSEStartUp();
    /* HSE 启动失败时死循环（硬件问题，无法继续） */
    while (hse_status == ERROR);

    RCC_HCLKConfig(RCC_SYSCLK_Div1);   /* AHB  = 72 MHz */
    RCC_PCLK1Config(RCC_HCLK_Div2);    /* APB1 = 36 MHz */
    RCC_PCLK2Config(RCC_HCLK_Div1);    /* APB2 = 72 MHz */

    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08);
}

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
/* ------------------------------------------------------------------ */
void arm_ctrl_init(void)
{
    /* 1. 时钟 */
    _rcc_init();

    /* 2. 舵机 GPIO + TIM3/TIM4 硬件 PWM + TIM2 缓动 */
    arm_servo_gpio_init();
    arm_servo_timer_init();

    /* 3. 串口1（调试输出，115200）*/
    arm_usart1_init();

    /* 4. 逆运动学连杆参数 */
    arm_kinematics_init(ARM_L0, ARM_L1, ARM_L2, ARM_L3);

    arm_usart1_print("[ARM] init done (HW PWM)\r\n");
}

/* ------------------------------------------------------------------ */
/*  内部：解算 + 驱动舵机                                              */
/* ------------------------------------------------------------------ */
static int _apply_kin_result(const arm_kin_result_t *res, uint32_t time_ms)
{
    uint8_t i;
    for (i = 0; i < ARM_SERVO_NUM; i++) {
        uint16_t pwm = (uint16_t)res->servo_pwm[i];
        arm_servo_set_pwm_time(i, pwm, (uint16_t)time_ms);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  arm_ctrl_move_xyz                                                   */
/*  策略：Alpha 从 0° 向 -135° 扫描，取使腕部与水平夹角最大的解       */
/*  （与商家 kinematics_move 原始逻辑完全一致）                         */
/* ------------------------------------------------------------------ */
int arm_ctrl_move_xyz(float x, float y, float z, uint32_t time_ms)
{
    arm_kin_result_t result;
    int alpha;
    int best_alpha = 0;
    int found      = 0;
    char dbg[64];

    if (y < -80.0f) {
        arm_usart1_print("[ARM] y < -80, out of range\r\n");
        return 0;
    }

    /* 扫描最优俯仰角 */
    for (alpha = 0; alpha >= -135; alpha--) {
        if (arm_kinematics_solve(x, y, z, (float)alpha, &result) == 0) {
            if (alpha < best_alpha) best_alpha = alpha;
            found = 1;
        }
    }

    if (!found) {
        arm_usart1_print("[ARM] no IK solution found\r\n");
        return 0;
    }

    /* 用最优 Alpha 重新解算并应用 */
    arm_kinematics_solve(x, y, z, (float)best_alpha, &result);
    _apply_kin_result(&result, time_ms);

    snprintf(dbg, sizeof(dbg),
             "[ARM] move (%.1f,%.1f,%.1f) alpha=%d t=%lu\r\n",
             x, y, z, best_alpha, time_ms);
    arm_usart1_print(dbg);

    return 1;
}

/* ------------------------------------------------------------------ */
/*  arm_ctrl_move_xyz_alpha                                             */
/* ------------------------------------------------------------------ */
int arm_ctrl_move_xyz_alpha(float x, float y, float z,
                             float alpha, uint32_t time_ms)
{
    arm_kin_result_t result;
    char dbg[64];

    int ret = arm_kinematics_solve(x, y, z, alpha, &result);
    if (ret != 0) {
        snprintf(dbg, sizeof(dbg),
                 "[ARM] IK failed, err=%d\r\n", ret);
        arm_usart1_print(dbg);
        return 0;
    }

    _apply_kin_result(&result, time_ms);

    snprintf(dbg, sizeof(dbg),
             "[ARM] move alpha (%.1f,%.1f,%.1f) a=%.1f t=%lu\r\n",
             x, y, z, alpha, time_ms);
    arm_usart1_print(dbg);

    return 1;
}

/* ------------------------------------------------------------------ */
/*  arm_ctrl_reset / stop                                               */
/* ------------------------------------------------------------------ */
void arm_ctrl_reset(uint32_t time_ms)
{
    arm_servo_reset_all((uint16_t)time_ms);
    arm_usart1_print("[ARM] reset to center\r\n");
}

void arm_ctrl_stop(void)
{
    arm_servo_stop_all();
    arm_usart1_print("[ARM] stopped\r\n");
}

/* ------------------------------------------------------------------ */
/*  状态查询                                                            */
/* ------------------------------------------------------------------ */
uint8_t arm_ctrl_is_done(void)
{
    return arm_servo_all_done();
}

uint8_t arm_ctrl_wait_done(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (!arm_servo_all_done()) {
        arm_delay_ms(10);
        elapsed += 10;
        if (timeout_ms > 0 && elapsed >= timeout_ms) return 0;
    }
    return 1;
}
