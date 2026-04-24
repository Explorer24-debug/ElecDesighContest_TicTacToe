/**
 * @file    main.c
 * @brief   WeArm 三子棋 TEST 版本 —— 硬件功能自动测试
 *
 * 本版本不依赖串口调试，上电后自动执行以下测试序列：
 *   测试 1：机械臂归中 + 安全位
 *   测试 2：逆运动学解算 + 移动到 3 个不同坐标
 *   测试 3：电磁铁 ON/OFF
 *   测试 4：完整落子动作（do_move / place / remove）
 *   测试 5：AI 决策 + 完整对弈演示
 *
 * 每个测试之间有 1 秒间隔，方便观察。
 * 测试通过后进入主循环：Decision_Update() 持续运行。
 * 之后队友接入 OpenMV 后，状态机会自动与视觉配合。
 *
 * ============================================================
 *  测试开关（注释掉不需要的测试）
 * ============================================================
 */

#include "stm32f10x.h"
#include "arm_ctrl.h"
#include "arm_magnet.h"
#include "arm_key.h"
#include "arm_chess.h"
#include "arm_delay.h"
#include "decision.h"
#include "vision_interface.h"

/* ---- 测试开关：设为 0 跳过对应测试 ---- */
#define TEST_MOVE_IK        1   /* 测试 1：归中 + 逆运动学 */
#define TEST_MAGNET         1   /* 测试 2：电磁铁 */
#define TEST_CHESS_ACTIONS  1   /* 测试 3：落子/放子/移除 */
#define TEST_AI_GAME        1   /* 测试 4：AI 完整对弈 */

/* ---- 测试间隔（毫秒） ---- */
#define TEST_GAP_MS  1000

/* ================================================================== */
/*  内部辅助：测试间等待                                                 */
/* ================================================================== */
static void test_gap(void)
{
    arm_delay_ms(TEST_GAP_MS);
}

/* ================================================================== */
/*  测试 1：机械臂归中 + 逆运动学 + 安全位                               */
/* ================================================================== */
static void test_move_and_ik(void)
{
    /* 1a. 归中 */
    arm_ctrl_reset(2000);
    arm_ctrl_wait_done(3000);
    test_gap();

    /* 1b. 安全位 */
    arm_chess_to_safe();
    test_gap();

    /* 1c. 逆运动学：移动到 3 个不同坐标，验证解算和舵机联动 */
    arm_ctrl_move_xyz(0.0f,  200.0f, 100.0f, 1500);
    arm_ctrl_wait_done(3000);
    test_gap();

    arm_ctrl_move_xyz(100.0f, 150.0f, 80.0f,  1500);
    arm_ctrl_wait_done(3000);
    test_gap();

    arm_ctrl_move_xyz(-80.0f, 180.0f, 120.0f, 1500);
    arm_ctrl_wait_done(3000);
    test_gap();

    /* 回安全位 */
    arm_chess_to_safe();
    test_gap();
}

/* ================================================================== */
/*  测试 2：电磁铁 ON/OFF（咔嗒声确认）                                  */
/* ================================================================== */
static void test_magnet(void)
{
    arm_magnet_on();
    arm_delay_ms(500);
    arm_magnet_off();
    arm_delay_ms(500);

    /* 再来一次确认 */
    arm_magnet_on();
    arm_delay_ms(500);
    arm_magnet_off();
    arm_delay_ms(500);
}

/* ================================================================== */
/*  测试 3：落子 / 放子 / 移除 动作                                     */
/* ================================================================== */
static void test_chess_actions(void)
{
    /* 3a. 完整落子到 5 号位（中心） */
    arm_chess_do_move(5);
    test_gap();

    /* 3b. 完整落子到 1 号位（左上角） */
    arm_chess_do_move(1);
    test_gap();

    /* 3c. 从 5 号位移除棋子 */
    arm_chess_remove(5);
    test_gap();

    /* 3d. 补放到 5 号位 */
    arm_chess_place(5);
    test_gap();

    /* 回安全位 */
    arm_chess_to_safe();
    test_gap();
}

/* ================================================================== */
/*  测试 4：AI 决策完整对弈演示                                          */
/*  模拟人先手，机器自动回应，下完一盘自动重置                            */
/* ================================================================== */
static void test_ai_game(void)
{
    /* ---- 所有声明放在块顶部（ARMCC C89 要求） ---- */
    static const uint8_t human_moves[] = {1, 2, 3, 6, 9};
    static const uint8_t num_moves = sizeof(human_moves);
    uint8_t  step;
    uint16_t timeout;
    uint16_t timeout2;

    /* 人先手 */
    g_human_first = 1;

    step = 0;

    while (step < num_moves) {
        /* 注入人的落子 */
        Logic_SetHumanMove(human_moves[step]);
        step++;

        /* 推进状态机直到回到 WAIT_HUMAN 或 GAME_OVER */
        timeout = 0;
        while (g_sys_state != SYS_STATE_WAIT_HUMAN &&
               g_sys_state != SYS_STATE_GAME_OVER &&
               timeout < 500) {
            Decision_Update();
            timeout++;
        }

        if (g_sys_state == SYS_STATE_GAME_OVER) {
            break;
        }

        test_gap();
    }

    /* 如果没结束，把剩余步骤跑完 */
    timeout2 = 0;
    while (g_sys_state != SYS_STATE_GAME_OVER && timeout2 < 500) {
        Decision_Update();
        timeout2++;
    }

    test_gap();

    /* 重置 */
    Decision_Init();
    test_gap();
}

/* ================================================================== */
/*  主函数                                                              */
/* ================================================================== */
int main(void)
{
    /* ---- 1. 硬件初始化 ---- */
    arm_ctrl_init();

    /* ---- 2. 开总中断（TIM2 缓动需要） ---- */
    __enable_irq();

    /* ---- 3. 外设初始化 ---- */
    arm_magnet_init();
    arm_key_init();
    arm_chess_init();

    /* ---- 4. 上电归中 ---- */
    arm_ctrl_reset(2000);
    arm_ctrl_wait_done(3000);
    arm_delay_ms(500);

    /* ---- 5. 安全位 ---- */
    arm_chess_to_safe();
    arm_ctrl_wait_done(3000);

    /* ================================================================
     *  自动测试序列
     *  每个测试之间有 1 秒间隔
     * ================================================================ */

#if TEST_MOVE_IK
    test_move_and_ik();
#endif

#if TEST_MAGNET
    test_magnet();
#endif

#if TEST_CHESS_ACTIONS
    test_chess_actions();
#endif

#if TEST_AI_GAME
    test_ai_game();
#endif

    /* ================================================================
     *  测试完成 → 进入正常运行主循环
     *  队友接入 OpenMV 后，Decision_Update() 会自动与视觉配合
     * ================================================================ */
    Decision_Init();

    while (1)
    {
        Decision_Update();
    }
}
