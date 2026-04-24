/**
 * @file    logic_core.c
 * @brief   三子棋 AI 决策核心（纯逻辑，无串口依赖）
 *
 * 策略（保证不输的经典井字棋算法）：
 *   1. 能赢 → 下这里
 *   2. 对手下一步能赢 → 堵住
 *   3. 占中心（5号位）
 *   4. 占角落（1,3,7,9）
 *   5. 占边（2,4,6,8）
 *
 * 视觉通信接口已预留（Vision_xxx），队友后续补充。
 */

#include "decision.h"
#include "arm_chess.h"
#include "arm_ctrl.h"
#include "arm_delay.h"
#include "arm_magnet.h"
#include "arm_key.h"
#include "vision_interface.h"

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
static uint8_t      s_best_move     = 0;
static uint16_t     s_select_timer  = 0;

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

    return 0;
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
}

/* ================================================================== */
/*  公共接口：注入人的落子                                              */
/* ================================================================== */
void Logic_SetHumanMove(uint8_t pos)
{
    if (pos >= 1 && pos <= 9) {
        g_new_human_move = pos;
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
}

ChessSpace_t Logic_GetMachineColor(void) { return s_machine_color; }
ChessSpace_t Logic_GetHumanColor(void)   { return s_human_color; }

/* ================================================================== */
/*  公共接口：辅助查询                                                  */
/* ================================================================== */
uint8_t Logic_IsOccupied(uint8_t pos)
{
    uint8_t r, c;
    if (pos < 1 || pos > 9) return 1;
    PosToIndex(pos, &r, &c);
    return (g_board[r][c] != SPACE_EMPTY) ? 1 : 0;
}

uint8_t Logic_GetGameResult(void)
{
    if (CheckWin(s_human_color))   return 1;
    if (CheckWin(s_machine_color)) return 2;
    if (CheckDraw())               return 3;
    return 0;
}

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

        /* 按键选择：KEY1 = 机器先手 */
        if (arm_key1_pressed()) {
            g_human_first = 0;
            _apply_colors();
            s_select_timer = 0;
            g_sys_state = SYS_STATE_CALCULATE;
            break;
        }

        /* 外部（测试代码或视觉回调）设置了先后手 */
        if (g_human_first == 0) {
            _apply_colors();
            s_select_timer = 0;
            g_sys_state = SYS_STATE_CALCULATE;
            break;
        }
        if (g_human_first == 1 && s_select_timer > 0) {
            _apply_colors();
            s_select_timer = 0;
            g_sys_state = SYS_STATE_WAIT_HUMAN;
            break;
        }

        /* 超时自动选择人先手 */
        arm_delay_ms(100);
        s_select_timer++;
        if (s_select_timer >= 30) {  /* 3 秒 */
            g_human_first = 1;
            _apply_colors();
            s_select_timer = 0;
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;

    /* ---- 等待人落子 ---- */
    case SYS_STATE_WAIT_HUMAN:

        /*
         * TODO(队友): 接入 OpenMV 后在此请求视觉拍照
         * Vision_NotifyCapture();
         * arm_delay_ms(2000);
         * if (!CheckBoardConsistent()) {
         *     g_sys_state = SYS_STATE_RESTORE;
         *     break;
         * }
         */

        /* 防篡改测试：KEY2 模拟篡改 */
        if (arm_key2_pressed()) {
            g_board_vision[1][1] = SPACE_EMPTY;
            if (!CheckBoardConsistent()) {
                g_sys_state = SYS_STATE_RESTORE;
                break;
            }
        }

        /* 等待人落子（通过 Logic_SetHumanMove 或 Vision_OnHumanMove 注入） */
        if (g_new_human_move != 0) {
            PosToIndex(g_new_human_move, &r, &c);

            if (g_board[r][c] != SPACE_EMPTY) {
                g_new_human_move = 0;
                break;
            }

            Logic_UpdateBoard(g_new_human_move, s_human_color);
            g_new_human_move = 0;

            /* TODO(队友): 通知视觉"人的动作完成" */
            Vision_NotifyActionDone();

            if (CheckWin(s_human_color)) {
                g_sys_state = SYS_STATE_GAME_OVER;
            } else if (CheckDraw()) {
                g_sys_state = SYS_STATE_GAME_OVER;
            } else {
                g_sys_state = SYS_STATE_CALCULATE;
            }
        }
        break;

    /* ---- 机器计算 ---- */
    case SYS_STATE_CALCULATE:
        best = Logic_GetBestMove();
        if (best == 0) {
            g_sys_state = SYS_STATE_GAME_OVER;
            break;
        }

        s_best_move = (uint8_t)best;
        Logic_UpdateBoard(s_best_move, s_machine_color);

        g_sys_state = SYS_STATE_DO_MOVE;
        break;

    /* ---- 执行落子动作 ---- */
    case SYS_STATE_DO_MOVE:
        arm_chess_do_move(s_best_move);

        /* TODO(队友): 通知视觉"机器动作完成" */
        Vision_NotifyActionDone();

        if (CheckWin(s_machine_color)) {
            g_sys_state = SYS_STATE_GAME_OVER;
        } else if (CheckDraw()) {
            g_sys_state = SYS_STATE_GAME_OVER;
        } else {
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;

    /* ---- 恢复被篡改的棋盘 ---- */
    case SYS_STATE_RESTORE: {
        uint8_t done = 1;
        uint8_t pi, pj, pos;

        for (pi = 0; pi < 3; pi++) {
            for (pj = 0; pj < 3; pj++) {
                if (g_board_vision[pi][pj] != g_board[pi][pj]) {
                    pos = IndexToPos(pi, pj);

                    if (g_board_vision[pi][pj] != SPACE_EMPTY &&
                        g_board[pi][pj] == SPACE_EMPTY) {
                        arm_chess_place(pos);
                        done = 0;
                        break;
                    }
                    if (g_board_vision[pi][pj] == SPACE_EMPTY &&
                        g_board[pi][pj] != SPACE_EMPTY) {
                        arm_chess_remove(pos);
                        done = 0;
                        break;
                    }
                }
            }
            if (!done) break;
        }

        if (done) {
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;
    }

    /* ---- 游戏结束 ---- */
    case SYS_STATE_GAME_OVER:
        arm_magnet_off();
        /* 等待外部调用 Decision_Init() 重置 */
        arm_delay_ms(100);
        break;

    default:
        break;
    }

    arm_delay_ms(10);
}
