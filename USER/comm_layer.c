#include "stm32f10x.h"
#include "comm_layer.h"
#include "x_usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* 外部变量引用（全部放在文件顶部，C89 要求） */
extern ChessSpace_t g_board[3][3];        /* 逻辑棋盘                  */
extern ChessSpace_t g_board_real[3][3];   /* 真实棋盘（视觉检测到的）  */
extern u8  g_new_human_move;              /* 人的新落子位置 (1-9)      */
extern u8  g_human_first;                 /* 1=人先手，0=机器先手      */
extern DecisionState_t g_sys_state;       /* 系统状态                  */
extern float g_board_angle_deg;           /* 棋盘旋转角度（度）        */
extern u8  g_vision_data_ready;           /* 视觉数据就绪标志          */
extern u8  g_alert_from, g_alert_to;      /* 棋子异常：原位/目标位     */
extern u8  g_alert_flag;                  /* 棋子异常报警标志          */

/* ========== 协议映射 ========== */
static char VisionEncode(ChessSpace_t piece)
{
    if (piece == SPACE_BLACK) return '2';
    if (piece == SPACE_WHITE) return '1';
    return '0';
}

static ChessSpace_t VisionDecode(char c)
{
    if (c == '1') return SPACE_WHITE;   /* 1 -> 机器白棋 */
    if (c == '2') return SPACE_BLACK;   /* 2 -> 人黑棋   */
    return SPACE_EMPTY;
}

/* ========== 校验和计算 ========== */
static u8 Calculate_XOR(const char *data)
{
    u8 xor_result = 0;
    while (*data) {
        xor_result ^= (u8)(*data);
        data++;
    }
    return xor_result;
}

static void ByteToHex(u8 val, char *hex_out)
{
    sprintf(hex_out, "%02X", val);
}

/* ========== 替代 strcasecmp（ARMCC 标准库无此函数） ========== */
static int comm_strcasecmp(const char *a, const char *b)
{
    while (*a && *b) {
        int diff = toupper((unsigned char)*a) - toupper((unsigned char)*b);
        if (diff != 0) return diff;
        a++; b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

/* ========== 发送接口 ========== */
static void Send_Cmd(char cmd)
{
    char buf[2];
    buf[0] = cmd;
    buf[1] = '\0';
    uart2_send_str((u8 *)buf);
    uart2_send_str((u8 *)"\n");
    printf("COMM(TX): %c\\n\r\n", cmd);
}

void Comm_Send_RequestCapture(void)  { Send_Cmd('S'); }
void Comm_Send_RequestAngle(void)    { Send_Cmd('R'); }
void Comm_Send_RequestPositions(void){ Send_Cmd('P'); }
void Comm_Send_Handshake(void)       { Send_Cmd('H'); }

/* ========== 解析 VR 消息（旋转角度） ========== */
/* 格式: VR<角度×10整数><校验2字节>\n  例如 VR300AB\n = 30.0° */
static void Parse_VR_Message(char *data_part)
{
    g_board_angle_deg = (float)atoi(data_part) / 10.0f;
    printf("COMM: Board angle = %.1f deg\r\n", g_board_angle_deg);
}

/* ========== 解析 VB 消息（棋盘状态） ========== */
/* 格式: VB<9位数字><校验2字节>\n  data_part 已去掉首字符'B' */
static void Parse_VB_Message(char *data_part)
{
    /* ---- 所有声明在块顶部（ARMCC C89 要求） ---- */
    ChessSpace_t temp_board[3][3];
    int i, j, row, col, found_new_move;
    char c;

    if (strlen(data_part) < 9) return;

    for (i = 0; i < 9; i++) {
        c   = data_part[i];
        row = i / 3;
        col = i % 3;
        if (c != '0' && c != '1' && c != '2') return;
        temp_board[row][col] = VisionDecode(c);
    }

    /* 更新真实棋盘 g_board_real */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            g_board_real[i][j] = temp_board[i][j];

    /* 与逻辑棋盘对比，找新增的黑棋（人刚下的） */
    found_new_move = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (temp_board[i][j] == SPACE_BLACK &&
                g_board[i][j]    == SPACE_EMPTY) {
                g_new_human_move = (u8)(i * 3 + j + 1);
                found_new_move = 1;
                break;
            }
        }
        if (found_new_move) break;
    }

    /* 通知决策层视觉数据已就绪 */
    g_vision_data_ready = 1;

    if (found_new_move)
        printf("COMM: Human move at %d\r\n", g_new_human_move);
    else
        printf("COMM: No new human move\r\n");
}

/* ========== 解析 VA 消息（棋子异常报警） ========== */
static void Parse_VA_Message(char *data_part)
{
    int from_pos, to_pos;
    if (sscanf(data_part, "%d,%d", &from_pos, &to_pos) != 2) return;

    printf("COMM: ALARM! Piece %d -> %d\r\n", from_pos, to_pos);
    g_alert_from = (u8)from_pos;
    g_alert_to   = (u8)to_pos;
    g_alert_flag = 1;
}

/* ========== 解析 VP 消息（格子坐标） ========== */
static void Parse_VP_Message(char *data_part)
{
    printf("COMM: Positions: %s\r\n", data_part);
    /* 如需动态更新坐标表，在此解析 */
}

/* ========== 主解析函数 ========== */
void Comm_Parse_Received_Data(char *frame)
{
    /* ---- 所有声明在块顶部 ---- */
    char *pos;
    int   len, data_len;
    char  expected_hex[3];
    char  received_hex[3];
    char  type_and_data[COMM_BUF_LEN];
    char  msg_type;
    char *data_field;
    u8    calc_xor;

    /* 1. 去掉末尾 \r\n */
    if ((pos = strchr(frame, '\n')) != NULL) *pos = '\0';
    if ((pos = strchr(frame, '\r')) != NULL) *pos = '\0';

    printf("COMM(RX): %s\r\n", frame);

    /* 2. 基本检查 */
    len = (int)strlen(frame);
    if (len < 3) return;
    if (frame[0] != 'V') {
        printf("COMM: Not from Vision\r\n");
        return;
    }

    /* 3. 提取末尾2字节校验码 */
    strncpy(received_hex, frame + len - 2, 2);
    received_hex[2] = '\0';

    /* 4. 提取"类型+数据"部分（V之后、校验码之前） */
    data_len = len - 3;
    strncpy(type_and_data, frame + 1, data_len);
    type_and_data[data_len] = '\0';

    /* 5. 计算并比较校验 */
    calc_xor = Calculate_XOR(type_and_data);
    ByteToHex(calc_xor, expected_hex);

    if (comm_strcasecmp(expected_hex, received_hex) != 0) {
        printf("COMM: Checksum Error! Calc=%s Recv=%s\r\n",
               expected_hex, received_hex);
        return;
    }

    /* 6. 分发 */
    msg_type   = type_and_data[0];
    data_field = type_and_data + 1;

    switch (msg_type) {
        case 'H': printf("COMM: Handshake OK\r\n");        break;
        case 'R': Parse_VR_Message(data_field);            break;
        case 'B': Parse_VB_Message(data_field);            break;
        case 'P': Parse_VP_Message(data_field);            break;
        case 'A': Parse_VA_Message(data_field);            break;
        case 'E': printf("COMM: Vision err: %s\r\n", data_field); break;
        default:  printf("COMM: Unknown: %c\r\n", msg_type);      break;
    }
}

/* ========== 初始化 ========== */
void Comm_Init(void)
{
    printf("COMM Layer Initialized (USART2)\r\n");
}
