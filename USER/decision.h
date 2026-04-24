/**
 * @file    decision.h
 * @brief   三子棋决策系统接口
 *
 * 状态机流程：
 *   INIT → SELECT → WAIT_HUMAN / CALCULATE → DO_MOVE → WAIT_HUMAN → ... → GAME_OVER
 *
 * 全局变量：
 *   g_sys_state        当前状态
 *   g_board[3][3]      逻辑棋盘
 *   g_board_vision[3][3] 视觉棋盘（队友填充）
 *   g_new_human_move   人的新落子位置（1~9），0 表示无
 *   g_human_first      1=人先手（默认），0=机器先手
 */

#ifndef DECISION_H
#define DECISION_H

#include "stm32f10x.h"

/* ================================================================== */
/*  棋盘状态枚举                                                        */
/* ================================================================== */
typedef enum {
    SPACE_EMPTY = 0,
    SPACE_BLACK,   /* 黑棋 */
    SPACE_WHITE    /* 白棋 */
} ChessSpace_t;

/* ================================================================== */
/*  决策状态机                                                          */
/* ================================================================== */
typedef enum {
    SYS_STATE_INIT,        /* 初始化 */
    SYS_STATE_SELECT,      /* 先后手选择 */
    SYS_STATE_WAIT_HUMAN,  /* 等待人落子 */
    SYS_STATE_CALCULATE,   /* 机器计算最优位置 */
    SYS_STATE_DO_MOVE,     /* 执行落子动作 */
    SYS_STATE_GAME_OVER,   /* 游戏结束 */
    SYS_STATE_RESTORE      /* 恢复被篡改的棋盘 */
} DecisionState_t;

/* ================================================================== */
/*  全局变量声明（在 logic_core.c 中定义）                               */
/* ================================================================== */
extern DecisionState_t g_sys_state;
extern ChessSpace_t    g_board[3][3];
extern ChessSpace_t    g_board_vision[3][3];
extern uint8_t         g_new_human_move;
extern uint8_t         g_human_first;

/* ================================================================== */
/*  接口函数                                                            */
/* ================================================================== */

/**
 * @brief  初始化决策系统（清空棋盘、进入 SELECT 状态）
 */
void Decision_Init(void);

/**
 * @brief  决策主循环（在 main 的 while(1) 中循环调用）
 *
 * 内部状态机自动推进：
 *   SELECT → 等待按键/串口选择先后手 → 超时默认人先手
 *   WAIT_HUMAN → 等待人落子 → 检查胜负
 *   CALCULATE → AI 计算最优位置
 *   DO_MOVE → 执行机械臂动作 → 回到 WAIT_HUMAN 或 GAME_OVER
 *   GAME_OVER → 等待重置
 *   RESTORE → 恢复被篡改的棋盘
 */
void Decision_Update(void);

/**
 * @brief  注入人的落子位置
 * @param  pos  1~9
 *
 * 供队友在视觉回调中调用，或供测试代码直接注入。
 */
void Logic_SetHumanMove(uint8_t pos);

/**
 * @brief  供视觉层调用：更新视觉识别到的棋盘
 * @param  board  3x3 棋盘数组
 */
void Logic_UpdateVisionBoard(ChessSpace_t board[3][3]);

/**
 * @brief  获取机器方颜色
 */
ChessSpace_t Logic_GetMachineColor(void);

/**
 * @brief  获取人方颜色
 */
ChessSpace_t Logic_GetHumanColor(void);

/**
 * @brief  检查指定位置是否被占用
 * @param  pos  1~9
 * @return 1 = 被占用，0 = 空位
 */
uint8_t Logic_IsOccupied(uint8_t pos);

/**
 * @brief  检查游戏是否结束
 * @return 0=进行中, 1=人赢, 2=机器赢, 3=平局
 */
uint8_t Logic_GetGameResult(void);

#endif /* DECISION_H */
