/**
 * @file    decision.h
 * @brief   决策系统对外接口（队友原版接口保持不变）
 *
 * 本文件与队友的 decision.h 接口完全兼容，
 * 仅去掉了对 comm_layer.h 的依赖（comm_layer 由 main.c 单独引入）。
 */

#ifndef __DECISION_H
#define __DECISION_H

#include "stm32f10x.h"

/* ================================================================== */
/*  棋盘与状态枚举（队友原版，不改）                                    */
/* ================================================================== */
typedef enum {
    SPACE_EMPTY = 0,
    SPACE_BLACK,    /* 黑棋（人） */
    SPACE_WHITE     /* 白棋（机器） */
} ChessSpace_t;

typedef enum {
    SYS_STATE_INIT,
    SYS_STATE_WAIT_HUMAN,
    SYS_STATE_CALCULATE,
    SYS_STATE_DO_MOVE,
    SYS_STATE_GAME_OVER,
    SYS_STATE_HUIFU,       /* 恢复被篡改的棋盘 */
    SYS_STATE_CHECK_BOARD,
    SYS_STATE_SELECT,      /* 先后手选择 */
    SYS_STATE_WAIT_VISION  /* 等待视觉返回结果 */
} DecisionState_t;

/* ================================================================== */
/*  坐标点结构体（队友原版，不改）                                      */
/* ================================================================== */
typedef struct {
    float x;
    float y;
    float z;
} Point3D_t;

/* ================================================================== */
/*  全局变量声明（队友原版，不改）                                      */
/* ================================================================== */
extern DecisionState_t g_sys_state;
extern ChessSpace_t    g_board[3][3];
extern ChessSpace_t    g_board_real[3][3];
extern u8  g_new_human_move;
extern u8  g_human_first;
extern u8  g_white_pawn_index;
extern u8  g_black_pawn_index;

/* 视觉通信标志（队友原版，不改） */
extern u8    g_vision_data_ready;
extern u8    g_vision_capture_done;
extern float g_board_angle_deg;
extern u8    g_alert_flag;
extern u8    g_alert_from;
extern u8    g_alert_to;
extern u8    g_waiting_vision;

/* ================================================================== */
/*  公共接口函数（队友原版，不改）                                      */
/* ================================================================== */
void Decision_Init(void);
void Decision_Update(void);
void Logic_SetHumanMove(u8 pos);

#endif /* __DECISION_H */
