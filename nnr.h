#ifndef NNR_H
#define NNR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winscard.h>
#include <time.h>
#include <stdarg.h>

typedef enum LogType {
    log_info,
    log_warn,
    log_error,
}LogType;

// 日志输出
void print_log(LogType type, const char *format, ...);

typedef struct nnr_device
{
    SCARDHANDLE hCard;
    SCARDCONTEXT hContext;
    char *readerName;
    SCARD_IO_REQUEST pioSendPci;
    SCARD_READERSTATE rgReaderStates[2];
}nnr_device;

// 初始化读卡器
nnr_device *nnr_init();

// 关闭读卡器，释放资源
void nnr_close(nnr_device *pnd);

// 等待校园卡插入（阻塞）
int nnr_wait_for_new_card(nnr_device *pnd);

// 取消正被阻塞的等待
int nnr_cancel_wait(nnr_device *pnd);

// 连接校园卡
int nnr_card_connect(nnr_device *pnd);

// 断开与校园卡的连接
int nnr_card_disconnect(nnr_device *pnd);

// 向校园卡发送指令（程序发送时自动加上校验部分）
int nnr_transceive_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx);

// 向校园卡发送原始指令（程序按指令原样发送，不做任何处理）
int nnr_transceive_raw_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx);

#endif // NNR_H
