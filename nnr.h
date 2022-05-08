#ifndef NNR_H
#define NNR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winscard.h>
#include <time.h>
#include <stdarg.h>

typedef enum LogType
{
    log_info,
    log_warn,
    log_error,
    log_debug,
} LogType;

// 日志输出
void print_log(LogType type, const char *format, ...);

typedef struct nnr_device
{
    SCARDHANDLE hCard;
    SCARDCONTEXT hContext;
    LPSTR mszReaders;
    LPSTR szReader;
    SCARD_IO_REQUEST pioSendPci;
    SCARD_READERSTATE rgReaderState;
} nnr_device;

// 初始化读卡器
nnr_device *nnr_init(const char *readerName);

// 关闭读卡器连接，释放所有资源
LONG nnr_close(nnr_device *pnd);

// 等待校园卡插入（阻塞）
LONG nnr_wait_for_new_card(nnr_device *pnd);

// 取消正被阻塞的等待
LONG nnr_cancel_wait(nnr_device *pnd);

// 连接校园卡
LONG nnr_card_connect(nnr_device *pnd);

// 断开与校园卡的连接
LONG nnr_card_disconnect(nnr_device *pnd);

// 向校园卡发送指令
LONG nnr_transceive_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx);

#endif // NNR_H
