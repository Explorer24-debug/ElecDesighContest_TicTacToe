/**
 * @file    vision_interface.c
 * @brief   OpenMV 视觉通信接口实现（当前为空实现）
 *
 * TODO(队友): 补充以下函数的具体通信逻辑
 *
 * 建议实现方式：
 *   1. 在此文件中添加 USART 初始化（可复用 USART1 或新增 USART2/3）
 *   2. 定义通信协议帧格式（帧头 + 命令 + 数据 + 校验）
 *   3. 实现 Vision_NotifyCapture()：发送拍照请求帧
 *   4. 实现 Vision_NotifyActionDone()：发送动作完成帧
 *   5. 实现 USART 接收中断：解析 OpenMV 返回的数据
 *   6. 在接收回调中调用 Vision_OnBoardUpdate() / Vision_OnHumanMove()
 */

#include "vision_interface.h"

/* ================================================================== */
/*  STM32 → OpenMV（空实现，队友补充）                                   */
/* ================================================================== */

void Vision_NotifyCapture(void)
{
    /* TODO(队友): 向 OpenMV 发送拍照指令 */
}

void Vision_NotifyActionDone(void)
{
    /* TODO(队友): 向 OpenMV 发送动作完成通知 */
}

void Vision_NotifyError(uint8_t err_code)
{
    (void)err_code;
    /* TODO(队友): 向 OpenMV 发送错误码 */
}

/* ================================================================== */
/*  OpenMV → STM32（空实现，队友在接收回调中调用）                       */
/* ================================================================== */

void Vision_OnBoardUpdate(ChessSpace_t board[3][3])
{
    /* TODO(队友): 将视觉识别到的棋盘状态同步到决策系统
     * 示例：遍历 board 数组更新 g_board_vision
     */
    (void)board;
}

void Vision_OnHumanMove(uint8_t pos)
{
    /* TODO(队友): 视觉识别到人落子后，调用 Logic_SetHumanMove(pos) */
    (void)pos;
}
