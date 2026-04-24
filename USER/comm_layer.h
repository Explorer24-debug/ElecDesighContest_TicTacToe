/**
 * @file    comm_layer.h
 * @brief   通信协议层 —— 调试串口 + OpenMV 预留
 *
 * ============ USART1 调试协议（你现在用的） ============
 * PC 串口助手发送（ASCII，回车结尾）：
 *   '1' ~ '9'    人在 pos 1~9 落子（状态机驱动时有效）
 *   'M'           机器先手（在 SELECT 状态下生效）
 *   'H'           人先手（在 SELECT 状态下生效）
 *   'R'           重置游戏
 *   'B'           打印当前棋盘
 *   'G' <pos>     执行 do_move 动作到 pos（手动测试用）
 *   'P' <pos>     执行 place 动作到 pos（手动测试用）
 *   'X' <pos>     执行 remove 动作到 pos（手动测试用）
 *   'S'           回到安全位置
 *   'T'           电磁铁 ON/OFF 切换
 *   '?'           打印帮助菜单
 *
 * ============ OpenMV 预留接口 ============
 * 以下函数供队友后续实现 OpenMV 通信时调用：
 *   Vision_OnBoardUpdate()  —— 视觉更新棋盘后调用
 *   Vision_NotifyCapture()  —— 通知视觉拍照
 *   Vision_NotifyAction()   —— 通知视觉动作完成
 *
 * TODO(队友): 在本文件中实现 OpenMV 通信的具体收发逻辑。
 *            可以复用 USART1 或新增 USART2，根据硬件决定。
 */

#ifndef COMM_LAYER_H
#define COMM_LAYER_H

#include "stm32f10x.h"
#include "decision.h"

/* ================================================================== */
/*  调试串口接口（当前实现）                                            */
/* ================================================================== */

/**
 * @brief  初始化调试通信（打印欢迎信息和帮助菜单）
 */
void Comm_Init(void);

/**
 * @brief  解析串口接收到的一个字符
 * @param  ch  ASCII 字符（主循环中从 USART1 轮询读取后传入）
 *
 * 在 SELECT / WAIT_HUMAN 状态下：
 *   '1'~'9' → Logic_SetHumanMove()
 *   'M'     → 强制机器先手
 *   'H'     → 强制人先手
 *
 * 在任何状态下：
 *   'R' → 重置游戏
 *   'B' → 打印棋盘
 *   'G'/'P'/'X' → 手动动作（需要后续跟一个位置字符 '1'~'9'）
 *   'S' → 安全位
 *   'T' → 电磁铁切换
 *   '?' → 帮助
 */
void Comm_ParseChar(uint8_t ch);

/* ================================================================== */
/*  OpenMV 预留接口（队友实现）                                        */
/* ================================================================== */

/**
 * @brief  视觉模块更新了棋盘状态，调用此函数注入
 * @param  board  3x3 棋盘数组
 *
 * TODO(队友): 在 OpenMV 通信回调中调用本函数，将视觉识别到的棋盘
 *            状态同步到决策系统。
 */
void Vision_OnBoardUpdate(ChessSpace_t board[3][3]);

/**
 * @brief  请求视觉拍照（STM32 → OpenMV）
 *
 * TODO(队友): 实现向 OpenMV 发送"请拍照"指令的逻辑。
 *            例如通过 USART 发送约定好的命令。
 */
void Vision_NotifyCapture(void);

/**
 * @brief  通知视觉：动作已完成（STM32 → OpenMV）
 *
 * TODO(队友): 实现向 OpenMV 发送"动作完成"指令的逻辑。
 */
void Vision_NotifyActionDone(void);

/**
 * @brief  通知视觉：发生错误（STM32 → OpenMV）
 * @param  err_code  错误码
 */
void Vision_NotifyError(uint8_t err_code);

#endif /* COMM_LAYER_H */
