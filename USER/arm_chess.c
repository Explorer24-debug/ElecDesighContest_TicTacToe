/**
 * @file    arm_chess.c
 * @brief   三子棋机械臂动作实现（纯驱动，无串口依赖）
 *
 * 动作参数说明：
 *   z_safe  —— 悬停高度，下降前先到这个高度
 *   z_down  —— 抓子/放子高度，电磁铁在此高度操作
 *   所有移动时间均为经验值，可根据实际调整
 */

#include "arm_chess.h"
#include "arm_config.h"
#include "arm_ctrl.h"
#include "arm_magnet.h"
#include "arm_delay.h"

/* ================================================================== */
/*  内部：棋盘坐标表 [pos 0~9]，0 号位不使用                             */
/* ================================================================== */
typedef struct {
    float x;
    float y;
} BoardPoint_t;

static BoardPoint_t s_grid[10];

/* ================================================================== */
/*  内部：移动到指定坐标并阻塞等待到位                                   */
/* ================================================================== */
static void _move_and_wait(float x, float y, float z, uint32_t time_ms)
{
    arm_ctrl_move_xyz(x, y, z, time_ms);
    arm_ctrl_wait_done(time_ms + 2000);
}

/* ================================================================== */
/*  初始化                                                              */
/* ================================================================== */
void arm_chess_init(void)
{
    s_grid[0].x = 0;   s_grid[0].y = 0;   /* 不使用 */

    s_grid[1].x = GRID_P1_X; s_grid[1].y = GRID_P1_Y;
    s_grid[2].x = GRID_P2_X; s_grid[2].y = GRID_P2_Y;
    s_grid[3].x = GRID_P3_X; s_grid[3].y = GRID_P3_Y;
    s_grid[4].x = GRID_P4_X; s_grid[4].y = GRID_P4_Y;
    s_grid[5].x = GRID_P5_X; s_grid[5].y = GRID_P5_Y;
    s_grid[6].x = GRID_P6_X; s_grid[6].y = GRID_P6_Y;
    s_grid[7].x = GRID_P7_X; s_grid[7].y = GRID_P7_Y;
    s_grid[8].x = GRID_P8_X; s_grid[8].y = GRID_P8_Y;
    s_grid[9].x = GRID_P9_X; s_grid[9].y = GRID_P9_Y;
}

/* ================================================================== */
/*  完整落子流程（机器正常落子）                                          */
/*  流程：备用位取子 → 移到目标格 → 放子 → 回安全位                      */
/* ================================================================== */
void arm_chess_do_move(uint8_t pos)
{
    if (pos < 1 || pos > 9) return;

    /* ---- 步骤 1：从备用棋位置取子 ---- */

    /* 1a. 悬停在备用棋上方 */
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_SAFE, 1000);

    /* 1b. 下降到取子高度 */
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_PICKUP, 800);

    /* 1c. 电磁铁通电吸住棋子 */
    arm_magnet_on();
    arm_delay_ms(300);

    /* 1d. 抬起 */
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_SAFE, 800);

    /* ---- 步骤 2：移动到目标格放子 ---- */

    /* 2a. 悬停在目标格上方 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 1000);

    /* 2b. 下降到放子高度 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_DOWN, 800);

    /* 2c. 电磁铁断电松开棋子 */
    arm_magnet_off();
    arm_delay_ms(300);

    /* 2d. 抬起 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 800);

    /* ---- 步骤 3：回到安全位置 ---- */
    arm_chess_to_safe();
}

/* ================================================================== */
/*  在指定格子放子（从备用位置取，用于恢复被篡改的棋盘）                    */
/* ================================================================== */
void arm_chess_place(uint8_t pos)
{
    if (pos < 1 || pos > 9) return;

    /* 取子 */
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_SAFE, 1000);
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_PICKUP, 800);
    arm_magnet_on();
    arm_delay_ms(300);
    _move_and_wait(STOCK_X, STOCK_Y, BOARD_Z_SAFE, 800);

    /* 放子 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 1000);
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_DOWN, 800);
    arm_magnet_off();
    arm_delay_ms(300);
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 800);

    arm_chess_to_safe();
}

/* ================================================================== */
/*  从指定格子移除棋子（拿到棋盘外丢弃区）                                 */
/* ================================================================== */
void arm_chess_remove(uint8_t pos)
{
    if (pos < 1 || pos > 9) return;

    /* 移到目标格上方 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 1000);

    /* 下降吸住 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_DOWN, 800);
    arm_magnet_on();
    arm_delay_ms(300);

    /* 抬起 */
    _move_and_wait(s_grid[pos].x, s_grid[pos].y, BOARD_Z_SAFE, 800);

    /* 移到丢弃区域 */
    _move_and_wait(STOCK_X, STOCK_Y - 30.0f, BOARD_Z_SAFE, 1000);
    _move_and_wait(STOCK_X, STOCK_Y - 30.0f, BOARD_Z_DOWN, 800);
    arm_magnet_off();
    arm_delay_ms(300);
    _move_and_wait(STOCK_X, STOCK_Y - 30.0f, BOARD_Z_SAFE, 800);

    arm_chess_to_safe();
}

/* ================================================================== */
/*  回到安全待机位置                                                     */
/* ================================================================== */
void arm_chess_to_safe(void)
{
    _move_and_wait(0.0f, 150.0f, BOARD_Z_SAFE, 1200);
}
