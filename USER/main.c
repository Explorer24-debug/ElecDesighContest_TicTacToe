/**
 * @file    main.c
 * @brief   WeArm 三子棋 v2 —— 调试模式主程序
 *
 * 本版本为调试/开发版本，通过 PC 串口助手交互：
 *   - 发送 1~9 落子
 *   - 发送 M/H 选择先后手
 *   - 发送 G/P/X 手动测试机械臂动作
 *   - 发送 R 重置、B 查看棋盘、? 帮助
 *
 * OpenMV 通信接口已预留（comm_layer.h 中 Vision_xxx 函数），
 * 队友在对应 TODO 位置补充实现即可。
 *
 * 启动流程：
 *   1. arm_ctrl_init()    硬件初始化（时钟/GPIO/TIM3/TIM4/TIM2/USART1）
 *   2. __enable_irq()     开总中断
 *   3. arm_magnet_init()  电磁铁
 *   4. arm_key_init()     按键
 *   5. arm_chess_init()   棋盘坐标
 *   6. Comm_Init()        串口调试协议
 *   7. 机械臂归中
 *   8. Decision_Init()    决策系统
 *
 * 主循环：
 *   - 轮询 USART1 收到的字符 → Comm_ParseChar()
 *   - Decision_Update()    状态机推进
 */

#include "stm32f10x.h"
#include "arm_ctrl.h"
#include "arm_magnet.h"
#include "arm_key.h"
#include "arm_chess.h"
#include "arm_usart.h"
#include "arm_delay.h"
#include "comm_layer.h"
#include "decision.h"

int main(void)
{
    /* ---- 1. 硬件初始化（包含 GPIO/TIM/USART1） ---- */
    arm_ctrl_init();

    /* ---- 2. 开总中断 ---- */
    __enable_irq();

    /* ---- 3. 外设初始化 ---- */
    arm_magnet_init();
    arm_key_init();
    arm_chess_init();
    Comm_Init();

    /* ---- 4. 上电归中 ---- */
    arm_ctrl_reset(2000);
    arm_ctrl_wait_done(3000);
    arm_delay_ms(500);

    /* ---- 5. 初始位置：安全待机 ---- */
    arm_chess_to_safe();
    arm_ctrl_wait_done(3000);

    /* ---- 6. 决策系统初始化 ---- */
    Decision_Init();

    /* ================================================================
     *  主循环：串口轮询 + 决策状态机
     * ================================================================ */
    while (1)
    {
        /* ---- 串口接收：逐字符轮询 ---- */
        {
            uint16_t data = arm_usart1_getchar();
            if (data != 0) {
                Comm_ParseChar((uint8_t)data);
            }
        }

        /* ---- 决策系统状态机 ---- */
        Decision_Update();
    }
}
