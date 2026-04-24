/**
 * @file    logic_core.c
 * @brief   三子棋 AI 决策核心
 *
 * 策略（保证不输的经典井字棋算法）：
 *   1. 能赢 → 下这里
 *   2. 对手下一步能赢 → 堵住
 *   3. 占中心（5号位）
 *   4. 占角落（1,3,7,9）
 *   5. 占边（2,4,6,8）
 *
 * 调试模式：
 *   通过串口助手交互，不依赖 OpenMV。
 *   视觉通信接口已预留（Vision_xxx），队友后续补充。
 */

#include "decision.h"
#include "arm_chess.h"
#include "arm_ctrl.h"
#include "arm_delay.h"
#include "arm_magnet.h"
#include "arm_usart.h"
#include "comm_layer.h"
#include "arm_key.h"
#include <stdio.h>
#include <string.h>

/* ================================================================== */
/*  全局变量定义                                                        */
/* ================================================================== */
DecisionState_t g_sys_state = SYS_STATE_INIT;
ChessSpace_t    g_board[3][3]        = { {SPACE_EMPTY} };
ChessSpace_t    g_board_vision[3][3] = { {SPACE_EMPTY} };
uint8_t         g_new_human_move     = 0;
uint8_t         g_human_first        = 1;

/* ================================================================== */
/*  内部变量                                                            */
/* ================================================================== */
static ChessSpace_t s_human_color;
static ChessSpace_t s_machine_color;
static uint8_t      s_best_move     = 0;   /* 机器本次落子位置 1~9 */
static uint16_t     s_select_timer  = 0;   /* 先后手选择倒计时 x100ms */
static char         s_msg[64];             /* 调试消息缓冲区 */

/* ================================================================== */
/*  内部辅助：应用颜色分配                                              */
/* ================================================================== */
static void _apply_colors(void)
{
    if (g_human_first) {
        s_human_color   = SPACE_BLACK;
        s_machine_color = SPACE_WHITE;
    } else {
        s_human_color   = SPACE_WHITE;
        s_machine_color = SPACE_BLACK;
    }
}

/* ================================================================== */
/*  内部辅助：位置 ↔ 下标转换                                           */
/* ================================================================== */
static void PosToIndex(uint8_t pos, uint8_t *r, uint8_t *c)
{
    *r = (pos - 1) / 3;
    *c = (pos - 1) % 3;
}

static uint8_t IndexToPos(uint8_t r, uint8_t c)
{
    return (uint8_t)(r * 3 + c + 1);
}

/* ================================================================== */
/*  内部辅助：胜负判定                                                  */
/* ================================================================== */
static int CheckWin(ChessSpace_t player)
{
    uint8_t i;
    for (i = 0; i < 3; i++) {
        if (g_board[i][0] == player &&
            g_board[i][1] == player &&
            g_board[i][2] == player) return 1;
    }
    for (i = 0; i < 3; i++) {
        if (g_board[0][i] == player &&
            g_board[1][i] == player &&
            g_board[2][i] == player) return 1;
    }
    if (g_board[0][0] == player &&
        g_board[1][1] == player &&
        g_board[2][2] == player) return 1;
    if (g_board[0][2] == player &&
        g_board[1][1] == player &&
        g_board[2][0] == player) return 1;
    return 0;
}

static int CheckDraw(void)
{
    uint8_t i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (g_board[i][j] == SPACE_EMPTY) return 0;
    return 1;
}

/* ================================================================== */
/*  内部辅助：棋盘一致性检测                                            */
/* ================================================================== */
static int CheckBoardConsistent(void)
{
    uint8_t i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (g_board[i][j] != g_board_vision[i][j])
                return 0;
    return 1;
}

/* ================================================================== */
/*  AI 核心算法：计算最优落子位置                                       */
/* ================================================================== */
static int Logic_GetBestMove(void)
{
    uint8_t i, j;

    /* 1. 能赢就下 */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (g_board[i][j] == SPACE_EMPTY) {
                g_board[i][j] = s_machine_color;
                if (CheckWin(s_machine_color)) {
                    g_board[i][j] = SPACE_EMPTY;
                    return IndexToPos(i, j);
                }
                g_board[i][j] = SPACE_EMPTY;
            }
        }
    }

    /* 2. 堵住对手 */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (g_board[i][j] == SPACE_EMPTY) {
                g_board[i][j] = s_human_color;
                if (CheckWin(s_human_color)) {
                    g_board[i][j] = SPACE_EMPTY;
                    return IndexToPos(i, j);
                }
                g_board[i][j] = SPACE_EMPTY;
            }
        }
    }

    /* 3. 占中心 */
    if (g_board[1][1] == SPACE_EMPTY) return 5;

    /* 4. 占角落 */
    {
        static const uint8_t corners[4] = {1, 3, 7, 9};
        uint8_t k, cr, cc;
        for (k = 0; k < 4; k++) {
            PosToIndex(corners[k], &cr, &cc);
            if (g_board[cr][cc] == SPACE_EMPTY) return corners[k];
        }
    }

    /* 5. 占边 */
    {
        static const uint8_t edges[4] = {2, 4, 6, 8};
        uint8_t k, er, ec;
        for (k = 0; k < 4; k++) {
            PosToIndex(edges[k], &er, &ec);
            if (g_board[er][ec] == SPACE_EMPTY) return edges[k];
        }
    }

    return 0; /* 棋盘满了 */
}

/* ================================================================== */
/*  内部辅助：更新逻辑棋盘                                              */
/* ================================================================== */
static void Logic_UpdateBoard(uint8_t pos, ChessSpace_t player)
{
    uint8_t r, c;
    if (pos < 1 || pos > 9) return;
    PosToIndex(pos, &r, &c);
    g_board[r][c] = player;
}

/* ================================================================== */
/*  内部辅助：打印棋盘                                                  */
/* ================================================================== */
static void PrintBoard(void)
{
    uint8_t i;
    static const char *sym[] = {". ", "O ", "X "};
    arm_usart1_print("\r\n  1 2 3\r\n");
    for (i = 0; i < 3; i++) {
        char line[32];
        sprintf(line, "%d %s%s%s\r\n", i + 1,
                sym[g_board[i][0]],
                sym[g_board[i][1]],
                sym[g_board[i][2]]);
        arm_usart1_print(line);
    }
    arm_usart1_print("\r\n");
}

/* ================================================================== */
/*  公共接口：初始化                                                    */
/* ================================================================== */
void Decision_Init(void)
{
    uint8_t i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            g_board[i][j]        = SPACE_EMPTY;
            g_board_vision[i][j] = SPACE_EMPTY;
        }

    g_new_human_move  = 0;
    s_best_move       = 0;
    s_select_timer    = 0;

    g_sys_state = SYS_STATE_SELECT;

    arm_usart1_print("[AI] init done\r\n");
    arm_usart1_print("[AI] send 'M'=machine first, 'H'=human first, or wait 3s\r\n");
}

/* ================================================================== */
/*  公共接口：注入人的落子                                              */
/* ================================================================== */
void Logic_SetHumanMove(uint8_t pos)
{
    if (pos >= 1 && pos <= 9) {
        g_new_human_move = pos;
        sprintf(s_msg, "[AI] human move %d\r\n", pos);
        arm_usart1_print(s_msg);
    }
}

/* ================================================================== */
/*  公共接口：更新视觉棋盘                                              */
/* ================================================================== */
void Logic_UpdateVisionBoard(ChessSpace_t board[3][3])
{
    uint8_t i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            g_board_vision[i][j] = board[i][j];
    arm_usart1_print("[AI] vision board updated\r\n");
}

ChessSpace_t Logic_GetMachineColor(void) { return s_machine_color; }
ChessSpace_t Logic_GetHumanColor(void)   { return s_human_color; }

/* ================================================================== */
/*  决策主循环                                                          */
/* ================================================================== */
void Decision_Update(void)
{
    uint8_t r, c;
    int    best;

    switch (g_sys_state) {

    /* ---- 初始化 ---- */
    case SYS_STATE_INIT:
        Decision_Init();
        break;

    /* ---- 先后手选择 ---- */
    case SYS_STATE_SELECT:

        /* 按键也可以选择 */
        if (arm_key1_pressed()) {
            g_human_first = 0;
            _apply_colors();
            arm_usart1_print("[AI] KEY1 -> MACHINE first\r\n");
            s_select_timer = 0;
            g_sys_state = SYS_STATE_CALCULATE;
            break;
        }

        /* 串口命令 'M'/'H' 直接改 g_human_first 并触发（在 comm_layer 中处理） */
        if (g_human_first == 0) {
            /* 串口选了机器先手 */
            _apply_colors();
            arm_usart1_print("[AI] MACHINE first (from serial)\r\n");
            s_select_timer = 0;
            g_sys_state = SYS_STATE_CALCULATE;
            break;
        }
        if (g_human_first == 1 && s_select_timer > 0) {
            /* 串口选了人先手 */
            _apply_colors();
            arm_usart1_print("[AI] HUMAN first (from serial)\r\n");
            s_select_timer = 0;
            g_sys_state = SYS_STATE_WAIT_HUMAN;
            PrintBoard();
            break;
        }

        /* 超时自动选择人先手 */
        arm_delay_ms(100);
        s_select_timer++;
        if (s_select_timer >= 30) {  /* 3 秒 */
            g_human_first = 1;
            _apply_colors();
            arm_usart1_print("[AI] timeout -> HUMAN first\r\n");
            s_select_timer = 0;
            g_sys_state = SYS_STATE_WAIT_HUMAN;
            PrintBoard();
        }
        break;

    /* ---- 等待人落子 ---- */
    case SYS_STATE_WAIT_HUMAN:

        /*
         * TODO(队友): 在此处请求视觉拍照，检查棋盘一致性。
         * 调试模式下跳过视觉检测，直接等人下。
         * 接入 OpenMV 后，取消注释以下代码：
         *
         * Vision_NotifyCapture();
         * arm_delay_ms(2000);  // 等待视觉返回
         * if (!CheckBoardConsistent()) {
         *     arm_usart1_print("[AI] cheating detected! restoring...\r\n");
         *     g_sys_state = SYS_STATE_RESTORE;
         *     break;
         * }
         */

        /* 检测篡改（调试用：KEY2 模拟） */
        if (arm_key2_pressed()) {
            arm_usart1_print("[AI] CHEAT SIM: clearing center\r\n");
            g_board_vision[1][1] = SPACE_EMPTY;
            if (!CheckBoardConsistent()) {
                arm_usart1_print("[AI] inconsistency detected!\r\n");
                g_sys_state = SYS_STATE_RESTORE;
                break;
            }
        }

        /* 等待人落子 */
        if (g_new_human_move != 0) {
            PosToIndex(g_new_human_move, &r, &c);

            if (g_board[r][c] != SPACE_EMPTY) {
                arm_usart1_print("[AI] occupied! try another\r\n");
                g_new_human_move = 0;
                break;
            }

            Logic_UpdateBoard(g_new_human_move, s_human_color);
            sprintf(s_msg, "[AI] human -> pos %d\r\n", g_new_human_move);
            arm_usart1_print(s_msg);
            g_new_human_move = 0;

            /* TODO(队友): 通知视觉"人的动作完成" */
            Vision_NotifyActionDone();

            PrintBoard();

            if (CheckWin(s_human_color)) {
                arm_usart1_print("[AI] *** HUMAN WINS ***\r\n");
                g_sys_state = SYS_STATE_GAME_OVER;
            } else if (CheckDraw()) {
                arm_usart1_print("[AI] *** DRAW ***\r\n");
                g_sys_state = SYS_STATE_GAME_OVER;
            } else {
                g_sys_state = SYS_STATE_CALCULATE;
            }
        }
        break;

    /* ---- 机器计算 ---- */
    case SYS_STATE_CALCULATE:
        arm_usart1_print("[AI] thinking...\r\n");

        best = Logic_GetBestMove();
        if (best == 0) {
            arm_usart1_print("[AI] *** DRAW ***\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
            break;
        }

        s_best_move = (uint8_t)best;
        Logic_UpdateBoard(s_best_move, s_machine_color);
        sprintf(s_msg, "[AI] machine -> pos %d\r\n", s_best_move);
        arm_usart1_print(s_msg);

        g_sys_state = SYS_STATE_DO_MOVE;
        break;

    /* ---- 执行落子动作 ---- */
    case SYS_STATE_DO_MOVE:
        arm_usart1_print("[AI] executing move...\r\n");
        arm_chess_do_move(s_best_move);

        /* TODO(队友): 通知视觉"机器动作完成" */
        Vision_NotifyActionDone();

        PrintBoard();

        if (CheckWin(s_machine_color)) {
            arm_usart1_print("[AI] *** MACHINE WINS ***\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
        } else if (CheckDraw()) {
            arm_usart1_print("[AI] *** DRAW ***\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
        } else {
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;

    /* ---- 恢复被篡改的棋盘 ---- */
    case SYS_STATE_RESTORE: {
        uint8_t done = 1;
        uint8_t pi, pj;

        for (pi = 0; pi < 3; pi++) {
            for (pj = 0; pj < 3; pj++) {
                if (g_board_vision[pi][pj] != g_board[pi][pj]) {
                    uint8_t pos = IndexToPos(pi, pj);

                    if (g_board_vision[pi][pj] != SPACE_EMPTY &&
                        g_board[pi][pj] == SPACE_EMPTY) {
                        /* 视觉有棋但逻辑没有 → 拿走 */
                        sprintf(s_msg, "[AI] restore: remove %d\r\n", pos);
                        arm_usart1_print(s_msg);
                        arm_chess_remove(pos);
                        done = 0;
                        break;
                    }
                    if (g_board_vision[pi][pj] == SPACE_EMPTY &&
                        g_board[pi][pj] != SPACE_EMPTY) {
                        /* 视觉没有但逻辑有 → 补上 */
                        sprintf(s_msg, "[AI] restore: place %d\r\n", pos);
                        arm_usart1_print(s_msg);
                        arm_chess_place(pos);
                        done = 0;
                        break;
                    }
                }
            }
            if (!done) break;
        }

        if (done) {
            arm_usart1_print("[AI] board restored\r\n");
            g_sys_state = SYS_STATE_WAIT_HUMAN;
            PrintBoard();
        }
        break;
    }

    /* ---- 游戏结束 ---- */
    case SYS_STATE_GAME_OVER:
        arm_magnet_off();
        arm_usart1_print("[AI] game over. send 'R' to restart.\r\n");
        /* 不再死循环，允许串口发 'R' 重置 */
        arm_delay_ms(100);
        break;

    default:
        break;
    }

    arm_delay_ms(10);
}
