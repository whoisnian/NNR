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
#include <openssl/des.h>
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

    DES_cblock blockKey = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF";
    DES_key_schedule scheduleKey;
    DES_set_key_unchecked(&blockKey, &scheduleKey);

    LONG res;
    while (!quit_flag)
    {
        // 等待卡插入（阻塞）
        res = nnr_wait_for_new_card(pnd);
        if (quit_flag)
            break;
        printf("==================== new card ====================\n");

        res = nnr_card_connect(pnd);
        print_log(log_info, "Card connected\n");

        uint8_t tx[1024], rx[1024];
        size_t tx_size, rx_size;

        // FMCOS command: GET CHALLENGE (8 byte)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\x84\x00\x00\x08", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "RAND: ");
        print_hex(rx, rx_size, false);

        // Single DES (ECB mode)
        DES_cblock blockRaw, blockRes;
        memcpy(blockRaw, rx, 8);
        DES_ecb_encrypt(&blockRaw, &blockRes, &scheduleKey, DES_ENCRYPT);
        print_hex(blockRes, 8, false);

        // FMCOS command: EXTERNAL AUTHENTICATE (default key FFFFFFFFFFFFFFFF)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\x82\x00\x00\x08", tx_size);
        memcpy(tx + tx_size, blockRes, 8);
        tx_size += 8;
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "AUTH: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: ERASE DF
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\x0E\x00\x00\x00", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "ERASE: ");
        print_hex(rx, rx_size, false);

        // // FMCOS command: CREATE FILE (MF)
        // tx_size = 18;
        // rx_size = sizeof(rx);
        // memcpy(tx, "\x80\xE0\x3F\x00\x0D\x38\xFF\xFF\xF0\xF0\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFF", tx_size);
        // res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        // print_log(log_info, "CREATE MF: ");
        // print_hex(rx, rx_size, false);

        // FMCOS command: CREATE FILE (KEY)
        tx_size = 12;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\xE0\x00\x00\x07\x3F\x00\x50\x01\xF0\xFF\xFF", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "CREATE KEY: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: WRITE KEY (auth)
        tx_size = 18;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\xD4\x01\x00\x0D\x39\xF0\xF0\xAA\x33\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "WRITE KEY: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: CREATE FILE (DF: NC.eCard.DDF01)
        tx_size = 27;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\xE0\x00\x01\x16\x38\x00\x00\xF0\xF0\x96\xFF\xFF\x4E\x43\x2E\x65\x43\x61\x72\x64\x2E\x44\x44\x46\x30\x31", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "CREATE DF: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: SELECT NC.eCard.DDF01
        tx_size = 19;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xA4\x04\x00\x0E\x4E\x43\x2E\x65\x43\x61\x72\x64\x2E\x44\x44\x46\x30\x31", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "EMV: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: GET CHALLENGE (8 byte)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\x84\x00\x00\x08", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "RAND: ");
        print_hex(rx, rx_size, false);

        // Single DES (ECB mode)
        memcpy(blockRaw, rx, 8);
        DES_ecb_encrypt(&blockRaw, &blockRes, &scheduleKey, DES_ENCRYPT);
        print_hex(blockRes, 8, false);

        // FMCOS command: EXTERNAL AUTHENTICATE (default key FFFFFFFFFFFFFFFF)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\x82\x00\x00\x08", tx_size);
        memcpy(tx + tx_size, blockRes, 8);
        tx_size += 8;
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "AUTH: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: CREATE FILE (BINARY)
        tx_size = 12;
        rx_size = sizeof(rx);
        memcpy(tx, "\x80\xE0\x00\x16\x07\x28\x00\x05\xF0\xF0\xFF\xFF", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "CREATE FILE: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: UPDATE BINARY
        tx_size = 10;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xD6\x96\x00\x05\x01\x02\x03\x04\x05", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "WRITE FILE: ");
        print_hex(rx, rx_size, false);

        // PICC command: Get UID APDU
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\xFF\xCA\x00\x00\x00", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "Card UID: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: SELECT NC.eCard.DDF01
        tx_size = 19;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xA4\x04\x00\x0E\x4E\x43\x2E\x65\x43\x61\x72\x64\x2E\x44\x44\x46\x30\x31", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "EMV: ");
        print_hex(rx, rx_size, false);

        // FMCOS command: READ BINARY sfi(0x16) offset(0x00) length(0x00)
        tx_size = 5;
        rx_size = sizeof(rx);
        memcpy(tx, "\x00\xB0\x96\x00\x00", tx_size);
        res = nnr_transceive_bytes(pnd, tx, tx_size, rx, &rx_size);
        print_log(log_info, "DATA(%x): ", rx_size);
        print_hex(rx, rx_size, false);

        res = nnr_card_disconnect(pnd);
        print_log(log_info, "Disconnect\n");
    }
    nnr_close(pnd);
    return 0;
}
