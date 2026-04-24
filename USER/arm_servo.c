/**
 * @file    arm_servo.c
 * @brief   舵机硬件 PWM 驱动实现
 *
 * 关键设计要点
 * ------------
 * 1. TIM3 CH1/CH2 + TIM4 CH1/CH2 输出 4 路 50 Hz 硬件 PWM。
 * 2. 定时器配置：PSC=72-1 → 1 MHz，ARR=20000-1 → 20 ms 周期。
 * 3. CCR 直接写入脉宽值（500~2500），硬件自动输出对应占空比。
 * 4. 缓动由 TIM2 每 20 ms 中断一次实现，线性插值更新 CCR。
 * 5. 不需要 AFIO 重映射，不占用 JTAG/SWD 引脚。
 */

#include "arm_servo.h"
#include "arm_config.h"
#include "arm_delay.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/*  私有数据结构                                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    uint16_t aim_pwm;   /* 目标脉宽 µs                */
    float    cur_pwm;   /* 当前脉宽 µs（浮点以保精度）*/
    float    inc;       /* 每次 TIM2 中断的增量       */
} servo_state_t;

static servo_state_t s_servo[ARM_SERVO_NUM];

/* ------------------------------------------------------------------ */
/*  内部工具                                                            */
/* ------------------------------------------------------------------ */
static float _my_fabsf(float v) { return v >= 0.0f ? v : -v; }

/* ------------------------------------------------------------------ */
/*  引脚映射表：每个通道对应的 (GPIO端口, 引脚号, TIMx, 通道, OCInit)  */
/* ------------------------------------------------------------------ */

/**
 * @brief  直接设置指定通道的 CCR 值（立即生效）
 */
static void _set_ccr(uint8_t index, uint16_t pwm)
{
    switch (index) {
    case 0: TIM_SetCompare1(TIM3, pwm); break;  /* PA6  TIM3_CH1 */
    case 1: TIM_SetCompare2(TIM3, pwm); break;  /* PA7  TIM3_CH2 */
    case 2: TIM_SetCompare1(TIM4, pwm); break;  /* PB6  TIM4_CH1 */
    case 3: TIM_SetCompare2(TIM4, pwm); break;  /* PB7  TIM4_CH2 */
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/*  GPIO 初始化                                                         */
/* ------------------------------------------------------------------ */
void arm_servo_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    uint8_t i;

    /* 开启 GPIOA、GPIOB 时钟（不需要 AFIO，不重映射） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /*
     * PA6  → TIM3_CH1（底座舵机）
     * PA7  → TIM3_CH2（大臂舵机）
     * 复用推挽输出，50 MHz
     */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*
     * PB6  → TIM4_CH1（小臂舵机）
     * PB7  → TIM4_CH2（腕部舵机）
     * 复用推挽输出，50 MHz
     */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 初始化舵机状态：全部中位 */
    for (i = 0; i < ARM_SERVO_NUM; i++) {
        s_servo[i].aim_pwm = ARM_PWM_MID;
        s_servo[i].cur_pwm = (float)ARM_PWM_MID;
        s_servo[i].inc     = 0.0f;
    }
}

/* ------------------------------------------------------------------ */
/*  硬件 PWM 定时器初始化（TIM3 + TIM4）                               */
/* ------------------------------------------------------------------ */
static void _pwm_timer_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef      TIM_OCStruct;

    /* ========== TIM3：CH1(PA6) + CH2(PA7) ========== */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseStruct.TIM_Period        = ARM_PWM_ARR;
    TIM_TimeBaseStruct.TIM_Prescaler     = ARM_PWM_PSC;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);

    /* PWM 模式 1：CNT < CCR 时输出高电平 */
    TIM_OCStruct.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_Pulse       = ARM_PWM_MID;  /* 初始中位 */
    TIM_OCStruct.TIM_OCPolarity  = TIM_OCPolarity_High;

    TIM_OC1Init(TIM3, &TIM_OCStruct);   /* CH1 → PA6 */
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC2Init(TIM3, &TIM_OCStruct);   /* CH2 → PA7 */
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    /* ========== TIM4：CH1(PB6) + CH2(PB7) ========== */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStruct);

    TIM_OC1Init(TIM4, &TIM_OCStruct);   /* CH1 → PB6 */
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2Init(TIM4, &TIM_OCStruct);   /* CH2 → PB7 */
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}

/* ------------------------------------------------------------------ */
/*  缓动定时器初始化（TIM2，每 20 ms 中断一次）                         */
/* ------------------------------------------------------------------ */
static void _ease_timer_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    NVIC_InitTypeDef        NVIC_InitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStruct.TIM_Period        = ARM_EASE_ARR;
    TIM_TimeBaseStruct.TIM_Prescaler     = ARM_EASE_PSC;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStruct);

    TIM_ARRPreloadConfig(TIM2, DISABLE);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* NVIC：优先级 1-2 */
    NVIC_InitStruct.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 2;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    TIM_Cmd(TIM2, ENABLE);
}

/* ------------------------------------------------------------------ */
/*  总初始化入口                                                        */
/* ------------------------------------------------------------------ */
void arm_servo_timer_init(void)
{
    _pwm_timer_init();    /* TIM3 + TIM4：硬件 PWM 输出 */
    _ease_timer_init();   /* TIM2：缓动调度 */
}

/* ------------------------------------------------------------------ */
/*  TIM2 中断服务函数（缓动更新核心）                                    */
/*  在 stm32f10x_it.c 中的 TIM2_IRQHandler() 调用此函数即可            */
/* ------------------------------------------------------------------ */
void arm_servo_tim2_isr(void)
{
    uint8_t  i;
    float    aim, step, diff;
    uint16_t pwm_us;

    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == RESET) return;
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    for (i = 0; i < ARM_SERVO_NUM; i++) {
        if (s_servo[i].inc == 0.0f) continue;

        aim  = (float)s_servo[i].aim_pwm;
        step = s_servo[i].inc;

        diff = _my_fabsf(aim - s_servo[i].cur_pwm);
        if (diff <= _my_fabsf(step) * 2.0f) {
            /* 到达目标 */
            s_servo[i].cur_pwm = aim;
            s_servo[i].inc     = 0.0f;
        } else {
            s_servo[i].cur_pwm += step;
        }

        /* 写入硬件 PWM CCR */
        pwm_us = (uint16_t)s_servo[i].cur_pwm;
        if (pwm_us > ARM_PWM_MAX) pwm_us = ARM_PWM_MAX;
        if (pwm_us < ARM_PWM_MIN) pwm_us = ARM_PWM_MIN;
        _set_ccr(i, pwm_us);
    }
}

/* ------------------------------------------------------------------ */
/*  公开控制接口（与软件 PWM 版本完全兼容）                            */
/* ------------------------------------------------------------------ */

void arm_servo_set_pwm(uint8_t index, uint16_t pwm)
{
    if (index >= ARM_SERVO_NUM) return;
    if (pwm > ARM_PWM_MAX) pwm = ARM_PWM_MAX;
    if (pwm < ARM_PWM_MIN) pwm = ARM_PWM_MIN;

    s_servo[index].aim_pwm = pwm;
    s_servo[index].cur_pwm = (float)pwm;
    s_servo[index].inc     = 0.0f;

    _set_ccr(index, pwm);
}

void arm_servo_set_pwm_time(uint8_t index, uint16_t pwm, uint16_t time_ms)
{
    float steps;

    if (index >= ARM_SERVO_NUM) return;
    if (pwm > ARM_PWM_MAX) pwm = ARM_PWM_MAX;
    if (pwm < ARM_PWM_MIN) pwm = ARM_PWM_MIN;

    s_servo[index].aim_pwm = pwm;

    if (time_ms < 20u) {
        /* 时间过短，直接到位 */
        s_servo[index].cur_pwm = (float)pwm;
        s_servo[index].inc     = 0.0f;
        _set_ccr(index, pwm);
    } else {
        /*
         * TIM2 每 20 ms 触发一次中断。
         * steps = time_ms / 20.0，与软件 PWM 版本公式一致。
         */
        steps = (float)time_ms / 20.0f;
        s_servo[index].inc = ((float)pwm - s_servo[index].cur_pwm) / steps;
    }
}

void arm_servo_stop_all(void)
{
    uint8_t i;
    for (i = 0; i < ARM_SERVO_NUM; i++) {
        s_servo[i].aim_pwm = (uint16_t)s_servo[i].cur_pwm;
        s_servo[i].inc     = 0.0f;
    }
}

void arm_servo_reset_all(uint16_t time_ms)
{
    uint8_t i;
    for (i = 0; i < ARM_SERVO_NUM; i++) {
        arm_servo_set_pwm_time(i, ARM_PWM_MID, time_ms);
    }
}

uint8_t arm_servo_is_done(uint8_t index)
{
    if (index >= ARM_SERVO_NUM) return 1;
    return (s_servo[index].inc == 0.0f) ? 1u : 0u;
}

uint8_t arm_servo_all_done(void)
{
    uint8_t i;
    for (i = 0; i < ARM_SERVO_NUM; i++) {
        if (s_servo[i].inc != 0.0f) return 0;
    }
    return 1;
}
