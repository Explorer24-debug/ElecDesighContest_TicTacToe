/**
 * @file    arm_chess.h
 * @brief   三子棋（井字棋）机械臂动作封装
 *
 * 本模块封装了完整的下棋动作序列：
 *   - arm_chess_grab(pos)    从指定格子抓走棋子（用于恢复被篡改的棋盘）
 *   - arm_chess_place(pos)   把棋子放到指定格子（用于落子和恢复）
 *   - arm_chess_do_move(pos) 完整的"取棋→移动→放子"流程（机器正常落子）
 *   - arm_chess_remove(pos)  把指定格子的棋子拿走放到棋盘外
 *   - arm_chess_to_safe()    回到安全待机位置
 *
 * 所有动作均为阻塞式（内部等待机械臂到位），调用者无需额外等待。
 */

#ifndef ARM_CHESS_H
#define ARM_CHESS_H

#include "stm32f10x.h"

/**
 * @brief  初始化棋盘坐标表（从 arm_config.h 读取标定坐标）
 *         必须在 arm_ctrl_init() 之后调用。
 */
void arm_chess_init(void);

/**
 * @brief  机器完整落子流程：取棋 → 移动到目标格 → 放子 → 回到安全位
 *         1. 移到备用棋子上方 → 下降 → 电磁铁吸住 → 抬起
 *         2. 移到目标格上方 → 下降 → 电磁铁松开 → 抬起
 *         3. 回到安全待机位置
 * @param  pos  目标格子编号 1~9
 */
void arm_chess_do_move(uint8_t pos);

/**
 * @brief  把棋子放到指定格子（从备用棋位置取子，用于恢复操作）
 * @param  pos  目标格子编号 1~9
 */
void arm_chess_place(uint8_t pos);

/**
 * @brief  从指定格子移除棋子（吸住 → 移到棋盘外 → 松开）
 * @param  pos  目标格子编号 1~9
 */
void arm_chess_remove(uint8_t pos);

/**
 * @brief  回到安全待机位置（棋盘上方，不遮挡视野）
 */
void arm_chess_to_safe(void);

#endif /* ARM_CHESS_H */
