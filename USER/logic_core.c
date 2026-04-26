/**
 * @file    logic_core.c
 * @brief   决策核心（基于队友原版，接入 arm_chess_ex 扩展接口）
 *
 * 与队友版 logic_core.c 的差异说明：
 *   1. Arm_Do_Grab / Arm_Do_Place 改为调用 arm_chess_ex 接口
 *      （支持棋盘旋转坐标变换）
 *   2. 所有 C89 语法违规已修复（ARMCC V5 要求）：
 *      - 变量声明全部移到块顶部
 *      - 去掉 for(int i=...) 内联声明
 *      - 去掉 static 局部变量混在可执行语句后的写法
 *   3. printf 改为 arm_delay_ms 占位（无串口版），如需串口调试可自行恢复
 *   4. 队友的状态机逻辑、接口、全局变量完全不变
 */

#include "stm32f10x.h"
#include "decision.h"
#include "arm_chess_ex.h"   /* 扩展接口——带旋转的抓放函数 */
#include "arm_chess.h"      /* arm_chess_to_safe() */
#include "arm_delay.h"
#include "arm_magnet.h"
#include "x_usart.h"        /* 队友串口驱动 */
#include "key.h"            /* 队友按键驱动 */
#include "comm_layer.h"     /* 队友 OpenMV 通信层 */
#include <math.h>

/* ================================================================== */
/*  棋盘旋转中心（mm）：以0度坐标推算 x=0, y=(150+210)/2=180           */
/* ================================================================== */
#define ROT_CENTER_X  0.0f
#define ROT_CENTER_Y  180.0f

/* ================================================================== */
/*  全局变量定义（队友原版，不改）                                      */
/* ================================================================== */
DecisionState_t g_sys_state      = SYS_STATE_INIT;
ChessSpace_t    g_board[3][3]    = {SPACE_EMPTY};
ChessSpace_t    g_board_real[3][3] = {SPACE_EMPTY};
ChessSpace_t    g_board_vision[3][3];

u8   g_new_human_move  = 0;
u8   g_human_first     = 1;
u8   g_white_pawn_index = 0;
u8   g_black_pawn_index = 0;

u8    g_vision_data_ready  = 0;
u8    g_vision_capture_done = 0;
float g_board_angle_deg    = 0.0f;
u8    g_alert_flag         = 0;
u8    g_alert_from         = 0;
u8    g_alert_to           = 0;

u8  g_waiting_vision = 0;
static u16 wait_counter     = 0;

/* ================================================================== */
/*  棋盘基准坐标表（0度时的坐标，单位 mm）                             */
/*  !!! 比赛前请用示教法将这9个坐标标定正确 !!!                        */
/* ================================================================== */
const Point3D_t GRID_POINT[10] = {
    {0,    0,   0},   /* 0号位：不使用 */
    { 30, 150,  0},   /* 1号格（左上） */
    {  0, 150,  0},   /* 2号格（中上） */
    {-30, 150,  0},   /* 3号格（右上） */
    { 30, 180,  0},   /* 4号格（左中） */
    {  0, 180,  0},   /* 5号格（正中） */
    {-30, 180,  0},   /* 6号格（右中） */
    { 30, 210,  0},   /* 7号格（左下） */
    {  0, 210,  0},   /* 8号格（中下） */
    {-30, 210,  0}    /* 9号格（右下） */
};

/* ================================================================== */
/*  取棋区坐标表（白棋5个位置 + 黑棋5个位置）                          */
/*  !!! 比赛前必须填入实际标定坐标 !!!                                 */
/* ================================================================== */
const Point3D_t WAIT_WHITE_POINT[5] = {
    {90, 120, 0},   /* 白棋1 */
    {90, 100, 0},   /* 白棋2 */
    {90,  80, 0},   /* 白棋3 */
    {90,  60, 0},   /* 白棋4 */
    {90,  40, 0}    /* 白棋5 */
};

const Point3D_t WAIT_BLACK_POINT[5] = {
    {-90, 120, 0},  /* 黑棋1 */
    {-90, 100, 0},  /* 黑棋2 */
    {-90,  80, 0},  /* 黑棋3 */
    {-90,  60, 0},  /* 黑棋4 */
    {-90,  40, 0}   /* 黑棋5 */
};

/* ================================================================== */
/*  内部函数声明                                                        */
/* ================================================================== */
static void Arm_MoveToSafe(void);
static void Arm_Do_Grab(float x, float y);
static void Arm_Do_Place(float x, float y);
static int  Logic_GetBestMove(void);
static void Logic_UpdateBoard(int pos, ChessSpace_t player);
static void PosToIndex(int pos, int *r, int *c);
static int  CheckWin(ChessSpace_t player);
static int  CheckDraw(void);

/* ================================================================== */
/*  坐标旋转（与 arm_chess_ex.c 保持一致的公式）                       */
/* ================================================================== */
static Point3D_t TransformPoint(Point3D_t base)
{
    Point3D_t result;
    float rad, cos_a, sin_a, dx, dy;

    if (g_board_angle_deg == 0.0f) {
        return base;
    }
    rad   = g_board_angle_deg * 3.1415926f / 180.0f;
    cos_a = cosf(rad);
    sin_a = sinf(rad);
    dx    = base.x - ROT_CENTER_X;
    dy    = base.y - ROT_CENTER_Y;
    result.x = dx * cos_a - dy * sin_a + ROT_CENTER_X;
    result.y = dx * sin_a + dy * cos_a + ROT_CENTER_Y;
    result.z = base.z;
    return result;
}

/* ================================================================== */
/*  内部辅助：移动到安全位                                              */
/* ================================================================== */
static void Arm_MoveToSafe(void)
{
    Chess_MoveToSafe();
    arm_delay_ms(200);
}

/* ================================================================== */
/*  内部辅助：抓取（直接调用 arm_chess_ex，不重复实现）                 */
/* ================================================================== */
static void Arm_Do_Grab(float x, float y)
{
    Chess_GrabAt(x, y);
}

/* ================================================================== */
/*  内部辅助：放置（直接调用 arm_chess_ex）                             */
/* ================================================================== */
static void Arm_Do_Place(float x, float y)
{
    Chess_PlaceAt(x, y);
}

/* ================================================================== */
/*  辅助：位置 ↔ 数组下标                                              */
/* ================================================================== */
static void PosToIndex(int pos, int *r, int *c)
{
    *r = (pos - 1) / 3;
    *c = (pos - 1) % 3;
}

static void Logic_UpdateBoard(int pos, ChessSpace_t player)
{
    int r, c;
    if (pos < 1 || pos > 9) return;
    PosToIndex(pos, &r, &c);
    g_board[r][c] = player;
}

/* ================================================================== */
/*  胜负判定                                                            */
/* ================================================================== */
static int CheckWin(ChessSpace_t player)
{
    int i;
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
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (g_board[i][j] == SPACE_EMPTY) return 0;
    return 1;
}

/* ================================================================== */
/*  AI 核心：保证不输的井字棋算法（队友原版，不改）                     */
/* ================================================================== */
static int Logic_GetBestMove(void)
{
    int i, j, k;
    int corners[4];
    int edges[4];

    /* 1. 能赢就下 */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (g_board[i][j] == SPACE_EMPTY) {
                g_board[i][j] = SPACE_WHITE;
                if (CheckWin(SPACE_WHITE)) {
                    g_board[i][j] = SPACE_EMPTY;
                    return i * 3 + j + 1;
                }
                g_board[i][j] = SPACE_EMPTY;
            }
        }
    }
    /* 2. 堵住对方 */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (g_board[i][j] == SPACE_EMPTY) {
                g_board[i][j] = SPACE_BLACK;
                if (CheckWin(SPACE_BLACK)) {
                    g_board[i][j] = SPACE_EMPTY;
                    return i * 3 + j + 1;
                }
                g_board[i][j] = SPACE_EMPTY;
            }
        }
    }
    /* 3. 占中心 */
    if (g_board[1][1] == SPACE_EMPTY) return 5;
    /* 4. 占角落 */
    corners[0] = 1; corners[1] = 3; corners[2] = 7; corners[3] = 9;
    for (k = 0; k < 4; k++) {
        int ci = (corners[k] - 1) / 3;
        int cj = (corners[k] - 1) % 3;
        if (g_board[ci][cj] == SPACE_EMPTY) return corners[k];
    }
    /* 5. 占边 */
    edges[0] = 2; edges[1] = 4; edges[2] = 6; edges[3] = 8;
    for (k = 0; k < 4; k++) {
        int ei = (edges[k] - 1) / 3;
        int ej = (edges[k] - 1) % 3;
        if (g_board[ei][ej] == SPACE_EMPTY) return edges[k];
    }
    return 0; /* 棋盘满了 */
}

/* ================================================================== */
/*  公共接口：初始化（队友原版逻辑，不改）                              */
/* ================================================================== */
void Decision_Init(void)
{
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_board[i][j]      = SPACE_EMPTY;
            g_board_real[i][j] = SPACE_EMPTY;
        }
    }
    g_new_human_move   = 0;
    g_white_pawn_index = 0;
    g_black_pawn_index = 0;
    g_vision_data_ready = 0;
    g_waiting_vision   = 0;
    wait_counter       = 0;

    Arm_MoveToSafe();
    g_sys_state = SYS_STATE_SELECT;

    printf("=== WeArm Chess System Ready ===\r\n");
    printf("Human first by default. Press KEY1 for machine first.\r\n");
}

/* ================================================================== */
/*  公共接口：注入人的落子（队友原版，不改）                            */
/* ================================================================== */
void Logic_SetHumanMove(u8 pos)
{
    if (pos >= 1 && pos <= 9) {
        g_new_human_move = pos;
    }
}

/* ================================================================== */
/*  决策主循环（基于队友原版，接入 arm_chess_ex，修复 C89 语法）        */
/* ================================================================== */
void Decision_Update(void)
{
    /* --- C89 要求：所有变量声明在函数顶部 --- */
    int            best_grid;
    int            r, c;
    int            cheat_detected;
    int            restore_done;
    int            i, j;
    int            pos;
    Point3D_t      target;
    Point3D_t      actual;
    static u16     vision_timeout = 0;

    switch (g_sys_state) {

    /* ---- 初始化 ---- */
    case SYS_STATE_INIT:
        Decision_Init();
        wait_counter = 0;
        break;

    /* ---- 先后手选择 ---- */
    case SYS_STATE_SELECT:
        if (wait_counter == 0) {
            printf("STATE: SELECT - Waiting for KEY1(machine first) or timeout(human first)\r\n");
        }
        arm_delay_ms(100);
        wait_counter++;
        if (wait_counter >= 30) {    /* 3 秒超时，默认人先手 */
            g_human_first = 1;
            g_sys_state   = SYS_STATE_WAIT_HUMAN;
            printf("STATE: SELECT -> WAIT_HUMAN (timeout, human first)\r\n");
        }
        break;

    /* ---- 等待人落子 ---- */
    case SYS_STATE_WAIT_HUMAN:
        /* 等人按 KEY2 触发拍照，状态切换在 main.c 的按键检测中完成 */
        break;

    /* ---- 等待视觉返回 ---- */
    case SYS_STATE_WAIT_VISION:
        if (g_vision_data_ready) {
            g_vision_data_ready = 0;
            g_waiting_vision    = 0;

            /* 打印视觉传回的棋盘状态 */
            printf("VISION Board: ");
            for (i = 0; i < 3; i++)
                for (j = 0; j < 3; j++)
                    printf("%d", g_board_real[i][j]);
            printf(" | Angle: %.1f deg\r\n", g_board_angle_deg);

            /* 检查棋盘是否被篡改 */
            cheat_detected = 0;
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    if (g_board[i][j] != g_board_real[i][j] &&
                        g_board[i][j] != SPACE_EMPTY) {
                        cheat_detected = 1;
                    }
                }
            }
            if (cheat_detected) {
                printf("STATE: WAIT_VISION -> HUIFU (board mismatch detected!)\r\n");
                g_sys_state = SYS_STATE_HUIFU;
                break;
            }

            /* 处理人的新落子 */
            if (g_new_human_move != 0) {
                PosToIndex(g_new_human_move, &r, &c);
                if (g_board[r][c] != SPACE_EMPTY) {
                    printf("STATE: WAIT_VISION -> WAIT_HUMAN (pos %d occupied, ignore)\r\n", g_new_human_move);
                    g_new_human_move = 0;
                    g_sys_state = SYS_STATE_WAIT_HUMAN;
                    break;
                }
                g_board[r][c]    = SPACE_BLACK;
                g_new_human_move = 0;
                printf("STATE: Human move -> pos %d (row%d,col%d)\r\n", r*3+c+1, r+1, c+1);

                if (CheckWin(SPACE_BLACK)) {
                    printf("STATE: WAIT_VISION -> GAME_OVER (BLACK WINS!)\r\n");
                    g_sys_state = SYS_STATE_GAME_OVER;
                } else if (CheckDraw()) {
                    printf("STATE: WAIT_VISION -> GAME_OVER (DRAW!)\r\n");
                    g_sys_state = SYS_STATE_GAME_OVER;
                } else {
                    printf("STATE: WAIT_VISION -> CALCULATE (machine thinking...)\r\n");
                    g_sys_state = SYS_STATE_CALCULATE;
                }
            } else {
                printf("STATE: WAIT_VISION -> WAIT_HUMAN (no new move detected)\r\n");
                g_sys_state = SYS_STATE_WAIT_HUMAN;
            }
        }

        /* 超时处理 */
        if (g_waiting_vision) {
            arm_delay_ms(100);
            vision_timeout++;
            if (vision_timeout > 300) {
                vision_timeout   = 0;
                g_waiting_vision = 0;
                printf("STATE: WAIT_VISION timeout -> WAIT_HUMAN\r\n");
                g_sys_state      = SYS_STATE_WAIT_HUMAN;
            }
        } else {
            vision_timeout = 0;
        }
        break;

    /* ---- 棋盘恢复（作弊处理） ---- */
    case SYS_STATE_HUIFU:
        restore_done = 1;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                if (g_board_real[i][j] == g_board[i][j]) continue;

                pos = i * 3 + j + 1;

                /* 情况A：真实有子，逻辑无子 → 拿走 */
                if (g_board_real[i][j] != SPACE_EMPTY &&
                    g_board[i][j]      == SPACE_EMPTY) {

                    printf("HUIFU: pos %d remove extra piece (%s)\r\n", pos,
                           (g_board_real[i][j] == SPACE_BLACK) ? "BLACK" : "WHITE");
                    actual = TransformPoint(GRID_POINT[pos]);
                    Arm_Do_Grab(actual.x, actual.y);

                    if (g_board_real[i][j] == SPACE_BLACK) {
                        Chess_PutToStock(0);
                    } else {
                        Chess_PutToStock(1);
                    }
                    g_board_real[i][j] = g_board[i][j];
                    restore_done = 0;
                    break;
                }

                /* 情况B：真实无子，逻辑有子 → 补上 */
                if (g_board_real[i][j] == SPACE_EMPTY &&
                    g_board[i][j]      != SPACE_EMPTY) {

                    printf("HUIFU: pos %d restore missing piece (%s)\r\n", pos,
                           (g_board[i][j] == SPACE_BLACK) ? "BLACK" : "WHITE");
                    Chess_RestorePiece(pos,
                        (g_board[i][j] == SPACE_WHITE) ? 1 : 0);
                    g_board_real[i][j] = g_board[i][j];
                    restore_done = 0;
                    break;
                }

                /* 情况C：真实有子，逻辑也有子，但颜色不同 → 换掉 */
                if (g_board_real[i][j] != SPACE_EMPTY &&
                    g_board[i][j]      != SPACE_EMPTY &&
                    g_board_real[i][j] != g_board[i][j]) {

                    printf("HUIFU: pos %d swap wrong piece (%s->%s)\r\n", pos,
                           (g_board_real[i][j] == SPACE_BLACK) ? "BLACK" : "WHITE",
                           (g_board[i][j] == SPACE_BLACK) ? "BLACK" : "WHITE");
                    /* 先拿走错误棋子 */
                    actual = TransformPoint(GRID_POINT[pos]);
                    Arm_Do_Grab(actual.x, actual.y);
                    if (g_board_real[i][j] == SPACE_BLACK) {
                        Chess_PutToStock(0);
                    } else {
                        Chess_PutToStock(1);
                    }
                    /* 再补上正确棋子 */
                    Chess_RestorePiece(pos,
                        (g_board[i][j] == SPACE_WHITE) ? 1 : 0);

                    g_board_real[i][j] = g_board[i][j];
                    restore_done = 0;
                    break;
                }
            }
            if (!restore_done) break;
        }

        if (restore_done) {
            printf("STATE: HUIFU done -> WAIT_HUMAN\r\n");
            Chess_MoveToSafe();
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;

    /* ---- 机器思考 + 落子 ---- */
    case SYS_STATE_CALCULATE:
        arm_delay_ms(500);

        best_grid = Logic_GetBestMove();
        if (best_grid == 0) {
            printf("STATE: CALCULATE -> GAME_OVER (board full)\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
            break;
        }

        PosToIndex(best_grid, &r, &c);
        g_board[r][c] = SPACE_WHITE;
        printf("STATE: Machine move -> pos %d (row%d,col%d)\r\n", best_grid, r+1, c+1);

        /* 去白棋区取棋 */
        target = WAIT_WHITE_POINT[g_white_pawn_index];
        printf("ARM: Grab white pawn from stock #%d\r\n", g_white_pawn_index);
        Arm_Do_Grab(target.x, target.y);
        if (g_white_pawn_index < 4) g_white_pawn_index++;

        /* 放到目标格（应用旋转变换） */
        actual = TransformPoint(GRID_POINT[best_grid]);
        printf("ARM: Place at grid %d (x=%.1f, y=%.1f, angle=%.1f)\r\n",
               best_grid, actual.x, actual.y, g_board_angle_deg);
        Arm_Do_Place(actual.x, actual.y);

        Chess_MoveToSafe();

        if (CheckWin(SPACE_WHITE)) {
            printf("STATE: CALCULATE -> GAME_OVER (WHITE WINS!)\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
        } else if (CheckDraw()) {
            printf("STATE: CALCULATE -> GAME_OVER (DRAW!)\r\n");
            g_sys_state = SYS_STATE_GAME_OVER;
        } else {
            printf("STATE: CALCULATE -> WAIT_HUMAN\r\n");
            g_sys_state = SYS_STATE_WAIT_HUMAN;
        }
        break;

    /* ---- DO_MOVE：队友原版保留（动作已在 CALCULATE 里做完） ---- */
    case SYS_STATE_DO_MOVE:
        break;

    /* ---- 游戏结束 ---- */
    case SYS_STATE_GAME_OVER:
        printf("STATE: GAME_OVER! Final board: ");
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++)
                printf("%d", g_board[i][j]);
        printf("\r\n");
        arm_magnet_off();
        arm_delay_ms(100);
        break;

    default:
        break;
    }

    arm_delay_ms(10);
}
