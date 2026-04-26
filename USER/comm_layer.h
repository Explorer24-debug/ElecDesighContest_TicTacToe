#ifndef __COMM_LAYER_H
#define __COMM_LAYER_H

#include "stm32f10x.h"
#include "decision.h"

// 协议相关定义
#define COMM_BUF_LEN 64          // 接收缓冲区长度
#define COMM_HEARTBEAT_TIMEOUT 5 // 心跳超时时间(秒)

// ========== 初始化函数 ==========
void Comm_Init(void);

// ========== 发送函数 (C→V) ==========
// 新协议：单字符命令 + \n

// 请求拍照 (S\n)
void Comm_Send_RequestCapture(void);

// 请求旋转角度 (R\n)
void Comm_Send_RequestAngle(void);

// 请求格子毫米坐标 (P\n)
void Comm_Send_RequestPositions(void);

// 握手查询 (H\n)
void Comm_Send_Handshake(void);

// ========== 接收处理函数 ==========

// 解析接收到的数据帧 (由串口中断或轮询调用)
// frame: 完整的一帧数据 (以 \n 结尾)，例如 "VR300AB\n"
void Comm_Parse_Received_Data(char *frame);

// ========== 状态查询函数 ==========

// 检查视觉是否在线 (心跳超时判断)
u8 Comm_IsVisionAlive(void);

// 更新心跳时间戳 (由定时器中断调用，每秒+1)
void Comm_UpdateHeartbeatTimer(void);

// 获取最后一次收到视觉数据的时间戳
u32 Comm_GetLastRxTime(void);

#endif
