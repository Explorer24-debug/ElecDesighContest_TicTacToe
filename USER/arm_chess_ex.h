/**
 * @file    arm_chess_ex.h
 * @brief   机械臂取放棋扩展接口（支持棋盘旋转 + 按XY坐标直接下棋）
 *
 * ============================================================
 *  本文件是 arm_chess.c（固定坐标）和 logic_core.c（决策+旋转）
 *  之间的中间层，对外提供以下三类接口：
 *
 *  1. 基础动作接口（带坐标旋转）
 *     - Chess_GrabAt(x, y)        从指定XY坐标抓取棋子
 *     - Chess_PlaceAt(x, y)       把手中棋子放到指定XY坐标
 *     - Chess_MoveToSafe()        回安全待机位
 *
 *  2. 格子号接口（带旋转，供外部直接用格子编号）
 *     - Chess_GrabFromPos(pos)    从棋盘格子号抓取（自动套用旋转角度）
 *     - Chess_PlaceToPos(pos)     放到棋盘格子号（自动套用旋转角度）
 *     - Chess_MoveToStock(white)  去取棋区取一枚棋子（white=1白棋,0黑棋）
 *     - Chess_PutToStock(white)   把手中棋子放回取棋区
 *
 *  3. 完整下棋动作（供 logic_core.c 决策状态机调用）
 *     - Chess_DoMoveByPos(pos)    机器落一子到棋盘pos位（取+放+回安全）
 *     - Chess_DoMoveByXY(x, y)   机器落一子到指定XY坐标（取+放+回安全）
 *     - Chess_RestorePiece(pos, is_white)  恢复一枚棋子到pos位
 *     - Chess_RemovePiece(pos, is_white)   从pos位拿走一枚棋子放回取棋区
 *
 * ============================================================
 *  使用前提：
 *    - arm_ctrl_init() 和 arm_chess_init() 已调用完成
 *    - g_board_angle_deg 已由视觉层更新（未旋转时保持 0.0f）
 *    - WAIT_WHITE_POINT[] / WAIT_BLACK_POINT[] 已在 logic_core.c 中标定
 *    - g_white_pawn_index / g_black_pawn_index 已正确维护
 *
 * ============================================================
 *  对外暴露的全局变量（在 logic_core.c 中定义，此处仅声明）：
 *    extern float       g_board_angle_deg;
 *    extern u8          g_white_pawn_index;
 *    extern u8          g_black_pawn_index;
 */

#ifndef ARM_CHESS_EX_H
#define ARM_CHESS_EX_H

#include "stm32f10x.h"
#include "decision.h"   /* Point3D_t, ChessSpace_t */

/* ================================================================== */
/*  1. 基础动作接口（带旋转，所有 XY 已经是旋转后的实际坐标）            */
/* ================================================================== */

/**
 * @brief  移动到指定 XY 坐标上方，下降，电磁铁吸住，抬起
 *         —— 执行完后手中已持有棋子
 * @param  x  目标 X 坐标 mm（已经过旋转变换）
 * @param  y  目标 Y 坐标 mm
 */
void Chess_GrabAt(float x, float y);

/**
 * @brief  移动到指定 XY 坐标上方，下降，电磁铁断电放下，抬起
 *         —— 执行完后手中已空
 * @param  x  目标 X 坐标 mm（已经过旋转变换）
 * @param  y  目标 Y 坐标 mm
 */
void Chess_PlaceAt(float x, float y);

/**
 * @brief  机械臂回到安全待机位（不遮挡摄像头视野）
 */
void Chess_MoveToSafe(void);

/* ================================================================== */
/*  2. 格子号接口（内部自动查表 + 应用旋转角度）                        */
/* ================================================================== */

/**
 * @brief  从棋盘 pos 号格子抓取棋子（内部自动旋转坐标）
 * @param  pos  格子编号 1~9
 */
void Chess_GrabFromPos(uint8_t pos);

/**
 * @brief  把手中棋子放到棋盘 pos 号格子（内部自动旋转坐标）
 * @param  pos  格子编号 1~9
 */
void Chess_PlaceToPos(uint8_t pos);

/**
 * @brief  去取棋区取一枚棋子，电磁铁吸住，抬起
 *         —— 执行完后手中已持有棋子，对应索引自动后移
 * @param  is_white  1=取白棋(机器), 0=取黑棋(人)
 * @return 1=成功, 0=库存耗尽
 */
uint8_t Chess_MoveToStock(uint8_t is_white);

/**
 * @brief  把手中棋子放回取棋区的当前空位
 *         —— 执行完后手中已空，对应索引自动调整
 * @param  is_white  1=放回白棋区, 0=放回黑棋区
 */
void Chess_PutToStock(uint8_t is_white);

/* ================================================================== */
/*  3. 完整下棋动作（供 logic_core.c 状态机直接调用）                   */
/* ================================================================== */

/**
 * @brief  机器落一子：去白棋区取棋 → 放到棋盘pos格 → 回安全位
 *         （白棋索引自动后移）
 * @param  pos  目标格子编号 1~9
 */
void Chess_DoMoveByPos(uint8_t pos);

/**
 * @brief  机器落一子到任意 XY 坐标（供视觉直接传坐标时使用）
 *         —— 从白棋区取棋 → 放到 (x,y) → 回安全位
 * @param  x   目标 X 坐标 mm（已是最终物理坐标，无需再旋转）
 * @param  y   目标 Y 坐标 mm
 */
void Chess_DoMoveByXY(float x, float y);

/**
 * @brief  恢复一枚棋子到指定格子（从对应颜色取棋区取 → 放回pos格）
 *         用于检测到棋盘被篡改后的还原
 * @param  pos       目标格子编号 1~9
 * @param  is_white  1=补白棋, 0=补黑棋
 */
void Chess_RestorePiece(uint8_t pos, uint8_t is_white);

/**
 * @brief  从指定格子拿走棋子，放回对应颜色取棋区
 *         用于检测到多余棋子时的清除
 * @param  pos       要拿走的格子编号 1~9
 * @param  is_white  1=拿走的是白棋, 0=拿走的是黑棋
 */
void Chess_RemovePiece(uint8_t pos, uint8_t is_white);

#endif /* ARM_CHESS_EX_H */
