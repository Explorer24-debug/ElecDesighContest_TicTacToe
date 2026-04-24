/**
 * @file    arm_ctrl.c
 * @brief   机械臂高层控制实现（纯驱动，无串口依赖）
 *
 * 功能：
 *   - RCC 时钟初始化（HSE + PLL → 72 MHz，HSE 失败回退 HSI）
 *   - 舵机 GPIO + TIM3/TIM4 硬件 PWM + TIM2 缓动 初始化
 *   - 逆运动学解算 → 舵机驱动
 *   - 归中、停止、等待到位
 */

#include "arm_ctrl.h"
#include "arm_config.h"
#include "arm_delay.h"
#include "arm_servo.h"
#include "arm_kinematics.h"

/* ------------------------------------------------------------------ */
/*  私有：RCC 初始化（HSE + PLL → 72 MHz）                            */
/* ------------------------------------------------------------------ */
static void _rcc_init(void)
{
    ErrorStatus hse_status;

    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    hse_status = RCC_WaitForHSEStartUp();

    if (hse_status == ERROR) {
        /* HSE 失败 → HSI 8MHz（波特率不准，但系统不会死） */
        RCC_HSICmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        RCC_PCLK1Config(RCC_HCLK_Div2);
        RCC_PCLK2Config(RCC_HCLK_Div1);
        RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
        while (RCC_GetSYSCLKSource() != 0x00);
    } else {
        /* HSE 正常 → PLL x9 = 72MHz */
        RCC_HCLKConfig(RCC_SYSCLK_Div1);   /* AHB  = 72 MHz */
        RCC_PCLK1Config(RCC_HCLK_Div2);    /* APB1 = 36 MHz */
        RCC_PCLK2Config(RCC_HCLK_Div1);    /* APB2 = 72 MHz */

        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        RCC_PLLCmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        while (RCC_GetSYSCLKSource() != 0x08);
    }
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

    /* 3. 逆运动学连杆参数 */
    arm_kinematics_init(ARM_L0, ARM_L1, ARM_L2, ARM_L3);
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
/* ------------------------------------------------------------------ */
int arm_ctrl_move_xyz(float x, float y, float z, uint32_t time_ms)
{
    arm_kin_result_t result;
    int alpha;
    int best_alpha = 0;
    int found      = 0;

    if (y < -80.0f) {
        return 0;
    }

    for (alpha = 0; alpha >= -135; alpha--) {
        if (arm_kinematics_solve(x, y, z, (float)alpha, &result) == 0) {
            if (alpha < best_alpha) best_alpha = alpha;
            found = 1;
        }
    }

    if (!found) {
        return 0;
    }

    arm_kinematics_solve(x, y, z, (float)best_alpha, &result);
    _apply_kin_result(&result, time_ms);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  arm_ctrl_move_xyz_alpha                                             */
/* ------------------------------------------------------------------ */
int arm_ctrl_move_xyz_alpha(float x, float y, float z,
                             float alpha, uint32_t time_ms)
{
    arm_kin_result_t result;

    if (arm_kinematics_solve(x, y, z, alpha, &result) != 0) {
        return 0;
    }

    _apply_kin_result(&result, time_ms);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  arm_ctrl_reset / stop                                               */
/* ------------------------------------------------------------------ */
void arm_ctrl_reset(uint32_t time_ms)
{
    arm_servo_reset_all((uint16_t)time_ms);
}

void arm_ctrl_stop(void)
{
    arm_servo_stop_all();
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
