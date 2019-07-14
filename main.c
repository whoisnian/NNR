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
void print_hex(const uint8_t *data, const size_t size)
{
    size_t  pos;
    for(pos = 0;pos < size;pos++)
        printf("%02x ", data[pos]);
    printf("\n");
}

nnr_device *pnd;
bool quit_flag = false;

// 结束程序
void stop_read(int sig)
{ 
    print_log(log_info, "Stop read... (sig: %d)\n", sig);
    if(pnd != NULL)
    {
        quit_flag = true;
        nnr_cancel_wait(pnd);
    }
}

int main(void)
{
    // Ctrl + C 时结束
    signal(SIGINT, stop_read);
    // kill xxx 时结束
    signal(SIGTERM, stop_read);

    // 初始化
    pnd = nnr_init();
    if(pnd == NULL)
    {
        print_log(log_error, "Failed to init reader.\n");
        return -1;
    }
    print_log(log_info, "Init reader '%s'.\n", pnd->readerName);

    int res;
    while(!quit_flag)
    {
        // 等待校园卡插入（阻塞）
        res = nnr_wait_for_new_card(pnd);
        if(quit_flag) break;
        printf("\n\n========================================\n");
        print_log(log_info, "Get new card: \n");

        res = nnr_card_connect(pnd);
        print_log(log_info, "Connect\n");

        res = nnr_card_disconnect(pnd);
        print_log(log_info, "Disconnect\n");
    }
    nnr_close(pnd);
    return 0;
}
