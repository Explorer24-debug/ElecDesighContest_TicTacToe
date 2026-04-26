/**
 * @file    main.c
 * @brief   WeArm 三子棋 v3 —— 正式运行版主函数
 *
 * ============================================================
 *  测试/运行模式说明（通过顶部宏切换）
 * ============================================================
 *
 *  TEST_MODE = 0  ：正式运行模式
 *                   上电归中 → Decision_Init() → 进入主循环
 *                   等待队友视觉通信层协作完成正常对弈
 *
 *  TEST_MODE = 1  ：测试模式（分三个测试段）
 *                   可以通过 TEST_CASE 宏选择要跑哪个测试
 *
 *  TEST_CASE = 1  ：普通对弈测试
 *                   模拟：机器先手，下完一盘，观察机械臂动作
 *
 *  TEST_CASE = 2  ：作弊恢复测试
 *                   先下两步，然后通过修改全局变量模拟
 *                   g_board_real 被篡改，让状态机进入 HUIFU 流程
 *
 *  TEST_CASE = 3  ：棋盘旋转后对弈测试
 *                   设置 g_board_angle_deg = 45.0f（模拟棋盘旋转45度）
 *                   然后正常下棋，观察落子是否正确命中旋转后的格子
 *
 * ============================================================
 *  正式运行时的主循环（TEST_MODE=0）
 * ============================================================
 *  1. 视觉数据接收：
 *       if (USART2_Vision_GetRxFlag()) {
 *           USART2_Vision_ReadString(rx_buf);
 *           Comm_Parse_Received_Data(rx_buf);  ← 队友实现
 *       }
 *
 *  2. 按键检测（KEY2 = 人确认落子，触发拍照请求）：
 *       if (Key_GetNum2()) {
 *           Comm_Send_RequestCapture();
 *           g_waiting_vision = 1;
 *           g_vision_data_ready = 0;
 *           g_sys_state = SYS_STATE_WAIT_VISION;
 *       }
 *
 *  3. 先后手选择（KEY1 = 机器先手）：
 *       if (Key_GetNum1() && g_sys_state == SYS_STATE_SELECT) {
 *           g_human_first = 0;
 *           g_sys_state = SYS_STATE_CALCULATE;
 *       }
 *
 *  4. Decision_Update() 状态机推进
 *
 * ============================================================
 *  注意：comm_layer.h / key.h / x_usart.h 是队友提供的文件，
 *        需要由队友拷贝到本工程 USER 目录，并加入 Keil 工程。
 *        测试模式下注释掉了对这些头文件的引用。
 * ============================================================
 */

#include "stm32f10x.h"
#include "arm_ctrl.h"
#include "arm_magnet.h"
#include "arm_chess.h"
#include "arm_chess_ex.h"
#include "arm_delay.h"
#include "decision.h"
#include "comm_layer.h"
#include "key.h"
#include "x_usart.h"

/* ============================================================
 *  运行模式宏
 * ============================================================ */
#define TEST_MODE   1   /* 1=测试模式, 0=正式运行 */
#define TEST_CASE   1   /* 1=普通对弈, 2=作弊恢复, 3=旋转对弈 */

/* ============================================================
 *  测试用辅助函数（仅 TEST_MODE=1 时使用）
 * ============================================================ */
#if TEST_MODE
static void delay_gap(void)
{
    arm_delay_ms(1500);
}
#endif

/* ============================================================
 *  测试 1：普通对弈
 *  流程：机器先手 → 机器落3子 → 结束
 *  观察：机械臂是否正确按序下子到5/1/9位
 * ============================================================ */
#if TEST_MODE && (TEST_CASE == 1)
static void test_normal_game(void)
{
    /* 初始化决策系统 */
    Decision_Init();
    delay_gap();

    /* 设置机器先手（跳过 SELECT 倒计时） */
    g_human_first = 0;
    g_sys_state   = SYS_STATE_CALCULATE;

    /* 机器落第1子（AI自动选5号位） */
    Decision_Update();   /* CALCULATE → DO_MOVE 或 WAIT_HUMAN */
    delay_gap();

    /* 模拟人落子到1号位 */
    Logic_SetHumanMove(1);
    g_board_real[0][0] = SPACE_BLACK;  /* 同步真实棋盘（模拟视觉已更新） */
    g_vision_data_ready = 1;
    g_sys_state = SYS_STATE_WAIT_VISION;
    Decision_Update();
    delay_gap();

    /* 机器落第2子 */
    Decision_Update();
    delay_gap();

    /* 模拟人落子到3号位 */
    Logic_SetHumanMove(3);
    g_board_real[0][2] = SPACE_BLACK;
    g_vision_data_ready = 1;
    g_sys_state = SYS_STATE_WAIT_VISION;
    Decision_Update();
    delay_gap();

    /* 机器落第3子（AI应在7号位堵住，或赢） */
    Decision_Update();
    delay_gap();

    /* 等待游戏结束 */
    while (g_sys_state != SYS_STATE_GAME_OVER) {
        Decision_Update();
    }
}
#endif

/* ============================================================
 *  测试 2：作弊恢复
 *  流程：先下两步 → 模拟人把白棋从5号位拿走 → 状态机恢复
 * ============================================================ */
#if TEST_MODE && (TEST_CASE == 2)
static void test_cheat_restore(void)
{
    /* 初始化 */
    Decision_Init();
    delay_gap();

    /* 机器先手，落子到5号位 */
    g_human_first = 0;
    g_sys_state   = SYS_STATE_CALCULATE;
    Decision_Update();   /* 机器落到5号 */
    delay_gap();

    /* 模拟人落子到1号 */
    Logic_SetHumanMove(1);
    g_board_real[0][0] = SPACE_BLACK;
    g_vision_data_ready = 1;
    g_sys_state = SYS_STATE_WAIT_VISION;
    Decision_Update();
    delay_gap();

    /*
     * 此时 g_board[1][1] = SPACE_WHITE（5号位有白棋）
     * 模拟人作弊：把5号位白棋拿走
     * → g_board_real[1][1] = SPACE_EMPTY（视觉看到5号位空了）
     * → g_board[1][1]      = SPACE_WHITE（逻辑棋盘认为有白棋）
     * → 这正是作弊场景
     */
    g_board_real[1][1] = SPACE_EMPTY;  /* 视觉更新：5号位被拿走 */
    g_vision_data_ready = 1;           /* 触发视觉数据就绪 */
    g_sys_state = SYS_STATE_WAIT_VISION;

    /* 状态机检测到篡改，跳转到 HUIFU */
    Decision_Update();
    delay_gap();

    /*
     * 进入 HUIFU 状态，机械臂应该：
     * 1. 从白棋区取一枚白棋
     * 2. 放到5号位
     * 观察：机械臂是否正确补回5号位的棋子
     */
    while (g_sys_state == SYS_STATE_HUIFU) {
        Decision_Update();
    }
    delay_gap();

    /* 恢复完毕，继续对弈 */
    while (g_sys_state != SYS_STATE_GAME_OVER) {
        Decision_Update();
    }
}
#endif

/* ============================================================
 *  测试 3：棋盘旋转后对弈
 *  流程：设置旋转角度 45° → 机器先手下棋 → 观察落子坐标是否旋转
 * ============================================================ */
#if TEST_MODE && (TEST_CASE == 3)
static void test_rotated_board(void)
{
    /* 初始化 */
    Decision_Init();
    delay_gap();

    /*
     * 模拟视觉传来的棋盘旋转角度：45度
     * 正式运行时这个值由 Comm_Parse_Received_Data() 解析视觉数据后写入
     */
    g_board_angle_deg = 45.0f;

    /* 机器先手（AI会落到5号位，但实际坐标已旋转45度） */
    g_human_first = 0;
    g_sys_state   = SYS_STATE_CALCULATE;
    Decision_Update();
    delay_gap();

    /*
     * 观察机械臂是否落到以 (ROT_CENTER_X=0, ROT_CENTER_Y=180) 为中心
     * 旋转45度后的5号位位置（正中心不变，仍在 (0,180)）
     */

    /* 模拟人落子 */
    Logic_SetHumanMove(2);
    g_board_real[0][1] = SPACE_BLACK;
    g_vision_data_ready = 1;
    g_sys_state = SYS_STATE_WAIT_VISION;
    Decision_Update();
    delay_gap();

    /* 机器再下一步 */
    Decision_Update();
    delay_gap();

    /* 继续跑完对弈 */
    while (g_sys_state != SYS_STATE_GAME_OVER) {
        Decision_Update();
    }

    /* 恢复旋转角度 */
    g_board_angle_deg = 0.0f;
}
#endif

/* ============================================================
 *  主函数
 * ============================================================ */
int main(void)
{
    /* ---- 1. 硬件初始化 ---- */
    arm_ctrl_init();

    /* ---- 2. 开总中断（TIM2 缓动需要） ---- */
    __enable_irq();

    /* ---- 3. 外设初始化 ---- */
    arm_magnet_init();
    arm_chess_init();   /* 初始化棋盘基准坐标表（从 arm_config.h 读取） */

    /* ---- 4. 上电归中 ---- */
    arm_ctrl_reset(2000);
    arm_ctrl_wait_done(3000);
    arm_delay_ms(500);

    /* ---- 5. 移到安全位 ---- */
    Chess_MoveToSafe();
    arm_ctrl_wait_done(3000);

#if TEST_MODE

    /* ================================================================
     *  测试模式：根据 TEST_CASE 跑对应测试
     * ================================================================ */

#if TEST_CASE == 1
    test_normal_game();
#elif TEST_CASE == 2
    test_cheat_restore();
#elif TEST_CASE == 3
    test_rotated_board();
#endif

    /* 所有测试完成后，回安全位停住 */
    Chess_MoveToSafe();
    while (1) { arm_delay_ms(1000); }

#else   /* TEST_MODE == 0：正式运行 */

    /* ================================================================
     *  正式运行模式
     *  需要队友将以下文件加入工程：
     *    - comm_layer.h / comm_layer.c
     *    - key.h / key.c
     *    - x_usart.h / x_usart.c
     *  并打开文件顶部注释的 #include
     * ================================================================ */

    Decision_Init();
    Key_Init();
    Comm_Init();
	tb_usart_init();

    {
        char rx_buf[64];

        while (1)
        {
            /* -- 视觉数据接收 -- */
            if (USART2_Vision_GetRxFlag()) {
                USART2_Vision_ReadString(rx_buf);
                Comm_Parse_Received_Data(rx_buf);
            }

            /* -- KEY1：机器先手 -- */
            if (Key_GetNum1() && g_sys_state == SYS_STATE_SELECT) {
                g_human_first = 0;
                g_sys_state   = SYS_STATE_CALCULATE;
            }

            /* -- KEY2：人确认落子，触发视觉拍照 -- */
            if (Key_GetNum2() && g_sys_state == SYS_STATE_WAIT_HUMAN) {
                Comm_Send_RequestCapture();
                g_waiting_vision    = 1;
                g_vision_data_ready = 0;
                g_sys_state         = SYS_STATE_WAIT_VISION;
            }

            Decision_Update();
        }
    }
#endif  /* TEST_MODE */
}
