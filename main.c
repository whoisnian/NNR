/*************************************************************************
    > File Name: main.c
    > Author: nian
    > Blog: https://whoisnian.com
    > Mail: zhuchangbao1998@gmail.com
    > Created Time: 2019年07月13日 星期六 16时42分21秒
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "nnr.h"

// 十六进制输出
void print_hex(const uint8_t *data, const size_t size, bool escape)
{
    size_t pos;
    for (pos = 0; pos < size; pos++)
        printf(escape ? "\\x%02x" : "%02x ", data[pos]);
    printf("\n");
}

nnr_device *pnd;
bool quit_flag = false;

// 结束程序
void stop_read(int sig)
{
    print_log(log_warn, "Stop reader... (sig: %d)\n", sig);
    if (pnd != NULL)
    {
        quit_flag = true;
        nnr_cancel_wait(pnd);
    }
}

int main(void)
{
    // Ctrl + C 时结束
    signal(SIGINT, stop_read);

    // 初始化
    pnd = nnr_init("ACR122U");
    if (pnd == NULL)
    {
        print_log(log_error, "Failed to init reader.\n");
        return -1;
    }
    print_log(log_info, "Init reader '%s'.\n", pnd->szReader);

    LONG res;
    while (!quit_flag)
    {
        // 等待交通卡插入（阻塞）
        res = nnr_wait_for_new_card(pnd);
        if (quit_flag)
            break;
        printf("==================== new card ====================\n");

        res = nnr_card_connect(pnd);
        print_log(log_info, "Card connected\n");

        uint8_t tx[1024], rx[1024];
        size_t tx_size, rx_size;

        // PICC command: Get UID APDU
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\xFF\xCA\x00\x00\x00", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "Card UID: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: SELECT 2PAY.SYS.DDF01
        tx_size = 19;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xA4\x04\x00\x0E\x32\x50\x41\x59\x2E\x53\x59\x53\x2E\x44\x44\x46\x30\x31", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "EMV: "); // https://emvlab.org/tlvutils/
        print_hex(rx, rx_size, false);

        // FMCOS command: SELECT A000000632010105
        tx_size = 13;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xA4\x04\x00\x08\xA0\x00\x00\x06\x32\x01\x01\x05", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "Card File: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: READ BINARY sfi(0x15) offset(0x00) length(0x1E)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xB0\x95\x00\x1E", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "公共应用信息(0x15): ");
        print_hex(rx, rx_size, false);

        // FMCOS command: READ BINARY sfi(0x16) offset(0x00) length(0x37)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xB0\x96\x00\x37", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "持卡人基本信息(0x16): ");
        print_hex(rx, rx_size, false);

        // FMCOS command: READ BINARY sfi(0x17) offset(0x00) length(0x3C)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xB0\x97\x00\x3C", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "管理信息(0x17): ");
        print_hex(rx, rx_size, false);

        // FMCOS command: READ RECORD sfi(0x18) N(0x01-0x0A) length(0x17)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xB2\x01\xC4\x17", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "交易明细(0x18): ");
        print_hex(rx, rx_size, false);

        // FMCOS command: GET BALANCE
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\x5C\x00\x02\x04", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "余额: ");
        print_hex(rx, rx_size, false);

        res = nnr_card_disconnect(pnd);
        print_log(log_info, "Disconnect\n");
    }
    nnr_close(pnd);
    return 0;
}
