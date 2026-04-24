/**
 * @file    comm_layer.c
 * @brief   通信协议实现 —— 调试串口 + OpenMV 预留（空实现）
 */

#include "comm_layer.h"
#include "arm_chess.h"
#include "arm_magnet.h"
#include "arm_usart.h"
#include <stdio.h>
#include <string.h>

/* ================================================================== */
/*  内部状态                                                            */
/* ================================================================== */

/** 手动动作命令的子状态：收到 'G'/'P'/'X' 后等待 '1'~'9' */
static uint8_t s_manual_cmd = 0;  /* 0=无, 'G'=do_move, 'P'=place, 'X'=remove */

/* ================================================================== */
/*  内部辅助：打印棋盘                                                  */
/* ================================================================== */
static void PrintBoard(void)
{
    uint8_t i;
    static const char *symbol[] = {". ", "O ", "X "};
    arm_usart1_print("\r\n  1 2 3\r\n");
    for (i = 0; i < 3; i++) {
        char line[32];
        sprintf(line, "%d %s%s%s\r\n", i + 1,
                symbol[g_board[i][0]],
                symbol[g_board[i][1]],
                symbol[g_board[i][2]]);
        arm_usart1_print(line);
    }
    arm_usart1_print("\r\n");
}

/* ================================================================== */
/*  内部辅助：帮助菜单                                                  */
/* ================================================================== */
static void PrintHelp(void)
{
    arm_usart1_print("\r\n=== WeArm Chess Debug ===\r\n");
    arm_usart1_print("1-9  : Human move (during game)\r\n");
    arm_usart1_print("M    : Machine first\r\n");
    arm_usart1_print("H    : Human first\r\n");
    arm_usart1_print("R    : Reset game\r\n");
    arm_usart1_print("B    : Print board\r\n");
    arm_usart1_print("G<n> : do_move to pos n\r\n");
    arm_usart1_print("P<n> : place to pos n\r\n");
    arm_usart1_print("X<n> : remove from pos n\r\n");
    arm_usart1_print("S    : Go to safe pos\r\n");
    arm_usart1_print("T    : Toggle magnet\r\n");
    arm_usart1_print("?    : This help\r\n");
    arm_usart1_print("========================\r\n");
}

/* ================================================================== */
/*  内部辅助：打印状态名                                                */
/* ================================================================== */
static const char *StateName(DecisionState_t s)
{
    switch (s) {
    case SYS_STATE_INIT:       return "INIT";
    case SYS_STATE_SELECT:     return "SELECT";
    case SYS_STATE_WAIT_HUMAN: return "WAIT_HUMAN";
    case SYS_STATE_CALCULATE:  return "CALCULATE";
    case SYS_STATE_DO_MOVE:    return "DO_MOVE";
    case SYS_STATE_GAME_OVER:  return "GAME_OVER";
    case SYS_STATE_RESTORE:    return "RESTORE";
    default:                   return "???";
    }
}

/* ================================================================== */
/*  调试串口：初始化                                                    */
/* ================================================================== */
void Comm_Init(void)
{
    arm_usart1_print("\r\n\r\n");
    arm_usart1_print("================================\r\n");
    arm_usart1_print("  WeArm Chess v2 - Debug Mode\r\n");
    arm_usart1_print("================================\r\n");
    PrintHelp();
}

/* ================================================================== */
/*  调试串口：字符解析                                                  */
/* ================================================================== */
void Comm_ParseChar(uint8_t ch)
{
    /* ---- 如果正在等待手动动作的位置参数 ---- */
    if (s_manual_cmd != 0) {
        if (ch >= '1' && ch <= '9') {
            uint8_t pos = ch - '0';
            char msg[32];
            switch (s_manual_cmd) {
            case 'G':
                sprintf(msg, "[CMD] do_move(%d)\r\n", pos);
                arm_usart1_print(msg);
                arm_chess_do_move(pos);
                break;
            case 'P':
                sprintf(msg, "[CMD] place(%d)\r\n", pos);
                arm_usart1_print(msg);
                arm_chess_place(pos);
                break;
            case 'X':
                sprintf(msg, "[CMD] remove(%d)\r\n", pos);
                arm_usart1_print(msg);
                arm_chess_remove(pos);
                break;
            }
        } else {
            arm_usart1_print("[CMD] canceled\r\n");
        }
        s_manual_cmd = 0;
        return;
    }

    /* ---- 正常命令解析 ---- */
    switch (ch) {

    /* '1'~'9'：人落子（仅在 WAIT_HUMAN 状态有效） */
    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
        if (g_sys_state == SYS_STATE_WAIT_HUMAN) {
            Logic_SetHumanMove(ch - '0');
        } else {
            arm_usart1_print("[CMD] not in WAIT_HUMAN state\r\n");
        }
        break;

    /* M：机器先手 */
    case 'M':
    case 'm':
        if (g_sys_state == SYS_STATE_SELECT) {
            g_human_first = 0;
            arm_usart1_print("[CMD] MACHINE first selected\r\n");
        } else {
            arm_usart1_print("[CMD] not in SELECT state\r\n");
        }
        break;

    /* H：人先手 */
    case 'H':
    case 'h':
        if (g_sys_state == SYS_STATE_SELECT) {
            g_human_first = 1;
            arm_usart1_print("[CMD] HUMAN first selected\r\n");
        } else {
            arm_usart1_print("[CMD] not in SELECT state\r\n");
        }
        break;

    /* R：重置游戏 */
    case 'R':
    case 'r':
        arm_usart1_print("[CMD] reset game\r\n");
        Decision_Init();
        PrintBoard();
        break;

    /* B：打印棋盘 */
    case 'B':
    case 'b':
        {
            char msg[48];
            sprintf(msg, "[CMD] state=%s\r\n", StateName(g_sys_state));
            arm_usart1_print(msg);
        }
        PrintBoard();
        break;

    /* G：手动 do_move（等下一个字符为位置） */
    case 'G':
    case 'g':
        s_manual_cmd = 'G';
        arm_usart1_print("[CMD] do_move -> send pos 1~9: ");
        break;

    /* P：手动 place */
    case 'P':
    case 'p':
        s_manual_cmd = 'P';
        arm_usart1_print("[CMD] place -> send pos 1~9: ");
        break;

    /* X：手动 remove */
    case 'X':
    case 'x':
        s_manual_cmd = 'X';
        arm_usart1_print("[CMD] remove -> send pos 1~9: ");
        break;

    /* S：安全位 */
    case 'S':
    case 's':
        arm_usart1_print("[CMD] go to safe\r\n");
        arm_chess_to_safe();
        break;

    /* T：电磁铁切换 */
    case 'T':
    case 't':
        arm_usart1_print("[CMD] toggle magnet\r\n");
        arm_magnet_on();
        arm_delay_ms(500);
        arm_magnet_off();
        break;

    /* ?：帮助 */
    default:
        PrintHelp();
        break;
    }
}

/* ================================================================== */
/*  OpenMV 预留接口 —— 空实现，队友补充                                 */
/* ================================================================== */

void Vision_OnBoardUpdate(ChessSpace_t board[3][3])
{
    uint8_t i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            g_board_vision[i][j] = board[i][j];
    /* TODO(队友): 可在此处添加向 OpenMV 发送 ACK 的逻辑 */
}

void Vision_NotifyCapture(void)
{
    /* TODO(队友): 实现向 OpenMV 发送"请拍照"指令
     * 例如：通过 USART 发送约定好的命令帧 */
}

void Vision_NotifyActionDone(void)
{
    /* TODO(队友): 实现向 OpenMV 发送"动作完成"通知 */
}

void Vision_NotifyError(uint8_t err_code)
{
    (void)err_code;
    /* TODO(队友): 实现向 OpenMV 发送错误码 */
}
