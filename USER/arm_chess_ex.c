/**
 * @file    arm_chess_ex.c
 * @brief   机械臂取放棋扩展接口实现
 *
 * 本文件实现 arm_chess_ex.h 中声明的所有函数。
 *
 * 依赖关系：
 *   arm_ctrl_move_xyz()  ← 底层运动控制
 *   arm_ctrl_wait_done() ← 阻塞等待到位
 *   arm_magnet_on/off()  ← 电磁铁控制
 *   g_board_angle_deg    ← 由视觉层写入的棋盘旋转角度
 *   GRID_POINT[]         ← 在 logic_core.c 中定义的基准坐标表
 *   WAIT_WHITE/BLACK_POINT[] ← 取棋区坐标表
 *   g_white/black_pawn_index ← 取棋索引
 */

#include "arm_chess_ex.h"
#include "arm_ctrl.h"
#include "arm_magnet.h"
#include "arm_delay.h"
#include "arm_config.h"
#include "decision.h"
#include <math.h>

/* ================================================================== */
/*  外部变量引用（在 logic_core.c 中定义）                              */
/* ================================================================== */
extern const Point3D_t GRID_POINT[10];
extern const Point3D_t WAIT_WHITE_POINT[5];
extern const Point3D_t WAIT_BLACK_POINT[5];

/* 棋盘旋转角度和取棋索引（在 decision.h 中 extern 声明） */
/* g_board_angle_deg, g_white_pawn_index, g_black_pawn_index */

/* ================================================================== */
/*  内部：坐标旋转（与 logic_core.c 中完全相同的变换公式）               */
/* ================================================================== */
#define ROT_CENTER_X  0.0f
#define ROT_CENTER_Y  180.0f

static Point3D_t _transform(Point3D_t base)
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
/*  内部：移动到 XYZ 并阻塞等待到位                                     */
/* ================================================================== */
static void _go(float x, float y, float z, uint32_t t_ms)
{
    arm_ctrl_move_xyz(x, y, z, t_ms);
    arm_ctrl_wait_done(t_ms + 2000);
}

/* ================================================================== */
/*  内部：经过过渡点移动（远距离移动时先收缩手臂避开摄像头）              */
/*                                                                     */
/*  轨迹：当前位置 → 过渡点(收缩+低头) → 目标上方(巡航高度) → 目标      */
/* ================================================================== */
static void _go_via(float x, float y, float z, uint32_t t_ms)
{
    /* 第一步：先去过渡点（收缩手臂，降低高度） */
    _go(WAYPOINT_X, WAYPOINT_Y, BOARD_Z_CRUISE, 800);

    /* 第二步：移到目标位置上方（保持巡航高度平移） */
    _go(x, y, BOARD_Z_CRUISE, 1000);

    /* 第三步：上升到目标高度 */
    _go(x, y, z, 600);
}

/* ================================================================== */
/*  1. 基础动作接口                                                     */
/* ================================================================== */

void Chess_GrabAt(float x, float y)
{
    /* 先经过过渡点，降到巡航高度，再移到目标上方 */
    _go_via(x, y, BOARD_Z_SAFE, 1000);

    _go(x, y, BOARD_Z_PICKUP,  800);   /* 下降 */
    arm_magnet_on();
    arm_delay_ms(300);                  /* 等待磁力稳定 */
    _go(x, y, BOARD_Z_SAFE,    800);   /* 抬起 */
}

void Chess_PlaceAt(float x, float y)
{
    /* 先经过过渡点，降到巡航高度，再移到目标上方 */
    _go_via(x, y, BOARD_Z_SAFE, 1000);

    _go(x, y, BOARD_Z_DOWN,   800);    /* 下降 */
    arm_magnet_off();
    arm_delay_ms(300);                  /* 等待释放 */
    _go(x, y, BOARD_Z_SAFE,   800);    /* 抬起 */
}

void Chess_MoveToSafe(void)
{
    /* 回安全位也走过渡点：先收缩再移到安全位 */
    _go(WAYPOINT_X, WAYPOINT_Y, BOARD_Z_CRUISE, 800);
    _go(100.0f, 30.0f, BOARD_Z_SAFE, 1200);
}

/* ================================================================== */
/*  2. 格子号接口                                                       */
/* ================================================================== */

void Chess_GrabFromPos(uint8_t pos)
{
    Point3D_t pt;
    if (pos < 1 || pos > 9) return;
    pt = _transform(GRID_POINT[pos]);
    Chess_GrabAt(pt.x, pt.y);
}

void Chess_PlaceToPos(uint8_t pos)
{
    Point3D_t pt;
    if (pos < 1 || pos > 9) return;
    pt = _transform(GRID_POINT[pos]);
    Chess_PlaceAt(pt.x, pt.y);
}

uint8_t Chess_MoveToStock(uint8_t is_white)
{
    Point3D_t src;
    if (is_white) {
        if (g_white_pawn_index > 4) return 0;  /* 库存耗尽 */
        src = WAIT_WHITE_POINT[g_white_pawn_index];
        Chess_GrabAt(src.x, src.y);
        if (g_white_pawn_index < 4) g_white_pawn_index++;
    } else {
        if (g_black_pawn_index > 4) return 0;
        src = WAIT_BLACK_POINT[g_black_pawn_index];
        Chess_GrabAt(src.x, src.y);
        if (g_black_pawn_index < 4) g_black_pawn_index++;
    }
    return 1;
}

void Chess_PutToStock(uint8_t is_white)
{
    Point3D_t dst;
    if (is_white) {
        /* 放回到当前索引前一个位置（刚才取走的那个位置） */
        uint8_t idx = (g_white_pawn_index > 0) ? (g_white_pawn_index - 1) : 0;
        dst = WAIT_WHITE_POINT[idx];
        Chess_PlaceAt(dst.x, dst.y);
    } else {
        /* 黑棋放回当前空位，索引前移 */
        if (g_black_pawn_index > 0) g_black_pawn_index--;
        dst = WAIT_BLACK_POINT[g_black_pawn_index];
        Chess_PlaceAt(dst.x, dst.y);
    }
}

/* ================================================================== */
/*  3. 完整下棋动作                                                     */
/* ================================================================== */

void Chess_DoMoveByPos(uint8_t pos)
{
    Point3D_t target;
    if (pos < 1 || pos > 9) return;

    /* 1. 去白棋区取棋 */
    if (!Chess_MoveToStock(1)) return;  /* 库存耗尽则放弃 */

    /* 2. 移到目标格放子（旋转坐标在 PlaceToPos 内部处理） */
    target = _transform(GRID_POINT[pos]);
    Chess_PlaceAt(target.x, target.y);

    /* 3. 回安全位 */
    Chess_MoveToSafe();
}

void Chess_DoMoveByXY(float x, float y)
{
    /* 1. 去白棋区取棋（坐标已是旋转后的值，无需再变换） */
    if (!Chess_MoveToStock(1)) return;

    /* 2. 直接放到传入的 XY 坐标 */
    Chess_PlaceAt(x, y);

    /* 3. 回安全位 */
    Chess_MoveToSafe();
}

void Chess_RestorePiece(uint8_t pos, uint8_t is_white)
{
    Point3D_t src;
    Point3D_t dst;

    if (pos < 1 || pos > 9) return;

    /* 1. 从对应颜色取棋区取棋 */
    if (is_white) {
        if (g_white_pawn_index > 4) return;
        src = WAIT_WHITE_POINT[g_white_pawn_index];
        Chess_GrabAt(src.x, src.y);
        if (g_white_pawn_index < 4) g_white_pawn_index++;
    } else {
        if (g_black_pawn_index > 4) return;
        src = WAIT_BLACK_POINT[g_black_pawn_index];
        Chess_GrabAt(src.x, src.y);
        if (g_black_pawn_index < 4) g_black_pawn_index++;
    }

    /* 2. 放到目标格（应用棋盘旋转） */
    dst = _transform(GRID_POINT[pos]);
    Chess_PlaceAt(dst.x, dst.y);

    /* 3. 回安全位 */
    Chess_MoveToSafe();
}

void Chess_RemovePiece(uint8_t pos, uint8_t is_white)
{
    Point3D_t src;
    Point3D_t dst;

    if (pos < 1 || pos > 9) return;

    /* 1. 从棋盘格子抓走棋子 */
    src = _transform(GRID_POINT[pos]);
    Chess_GrabAt(src.x, src.y);

    /* 2. 放回对应颜色取棋区 */
    if (is_white) {
        if (g_white_pawn_index > 0) g_white_pawn_index--;
        dst = WAIT_WHITE_POINT[g_white_pawn_index];
    } else {
        if (g_black_pawn_index > 0) g_black_pawn_index--;
        dst = WAIT_BLACK_POINT[g_black_pawn_index];
    }
    Chess_PlaceAt(dst.x, dst.y);

    /* 3. 回安全位 */
    Chess_MoveToSafe();
}
