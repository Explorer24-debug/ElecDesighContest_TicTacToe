/**
 * @file    vision_interface.h
 * @brief   OpenMV 视觉通信接口（预留，队友实现）
 *
 * 本文件定义了 STM32 与 OpenMV 之间的通信接口。
 * 当前版本所有函数为空实现，队友在 vision_interface.c 中补充具体逻辑。
 *
 * 使用方式：
 *   - Vision_NotifyCapture()    通知 OpenMV 拍照
 *   - Vision_NotifyActionDone() 通知 OpenMV 机械臂动作完成
 *   - Vision_NotifyError()       通知 OpenMV 发生错误
 *   - Vision_OnBoardUpdate()     OpenMV 返回棋盘数据后调用
 */

#ifndef VISION_INTERFACE_H
#define VISION_INTERFACE_H

#include "stm32f10x.h"
#include "decision.h"

/* ================================================================== */
/*  STM32 → OpenMV（队友实现发送逻辑）                                   */
/* ================================================================== */

/**
 * @brief  通知 OpenMV 拍照识别棋盘
 *
 * TODO(队友): 通过 USART 发送拍照指令帧给 OpenMV
 */
void Vision_NotifyCapture(void);

/**
 * @brief  通知 OpenMV：机械臂动作已完成
 *
 * TODO(队友): 通过 USART 发送完成通知帧
 */
void Vision_NotifyActionDone(void);

/**
 * @brief  通知 OpenMV：发生错误
 * @param  err_code  错误码（自定义）
 *
 * TODO(队友): 通过 USART 发送错误通知帧
 */
void Vision_NotifyError(uint8_t err_code);

/* ================================================================== */
/*  OpenMV → STM32（队友收到视觉数据后调用）                             */
/* ================================================================== */

/**
 * @brief  视觉模块更新棋盘数据
 * @param  board  3x3 棋盘状态数组
 *
 * TODO(队友): 在 USART 接收回调中解析 OpenMV 返回的棋盘数据，
 *            然后调用本函数注入到决策系统。
 */
void Vision_OnBoardUpdate(ChessSpace_t board[3][3]);

/**
 * @brief  视觉模块通知人的落子位置
 * @param  pos  1~9（视觉识别到人下了哪一格）
 *
 * TODO(队友): OpenMV 识别到人落子后，通过 USART 传回位置，
 *            队友在回调中调用此函数。
 */
void Vision_OnHumanMove(uint8_t pos);

#endif /* VISION_INTERFACE_H */
