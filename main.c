/*************************************************************************
    > File Name: main.c
    > Author: nian
    > Blog: https://whoisnian.com
    > Mail: zhuchangbao1998@gmail.com
    > Created Time: 2019年06月29日 星期六 21时42分21秒
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <nfc/nfc.h>
#include <nfc/nfc-types.h>
#include <winscard.h>
#include <reader.h>
#define IOCTL_CCID_ESCAPE_SCARD_CTL_CODE SCARD_CTL_CODE(1)
#define DEVICE_NAME_LENGTH  256
#define ACR122_PCSC_RESPONSE_LEN 268

#include <time.h>
#include <stdarg.h>

typedef enum LogType {
    log_info,
    log_warn,
    log_error,
}LogType;

// log 输出
void print_log(LogType type, const char *format, ...)
{
    time_t timer;
    time(&timer);
    char formatStr[20];
    strftime(formatStr, 20, "%Y-%m-%d %H:%M:%S", localtime(&timer));

    if(type == log_info)
        printf("%s \033[1;32;40m[I]\033[0m ", formatStr);
    else if(type == log_warn)
        printf("%s \033[1;33;40m[W]\033[0m ", formatStr);
    else if(type == log_error)
        printf("%s \033[1;31;40m[E]\033[0m ", formatStr);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// 十六进制输出
void print_hex(const uint8_t *data, const size_t size)
{
    size_t  pos;
    for(pos = 0;pos < size;pos++)
        printf("%02x ", data[pos]);
    printf("\n");
}

// 发送 APDU 命令
int transceive(nfc_device *pnd, uint8_t *txData, size_t txLen, uint8_t *rxData, size_t *rxLen)
{
    int res;

    print_log(log_info, "=> ");
    print_hex(txData, txLen);

    res = nfc_initiator_transceive_bytes(pnd, txData, txLen, rxData, *rxLen, 500);
    if(res < 0) return -1;

    *rxLen = (size_t)res;
    print_log(log_info, "<= ");
    print_hex(rxData, *rxLen);
    return 0;
}

struct nfc_device {
  const nfc_context *context;
  const struct nfc_driver *driver;
  void *driver_data;
  void *chip_data;
  char    name[DEVICE_NAME_LENGTH];
  nfc_connstring connstring;
  bool    bCrc;
  bool    bPar;
  bool    bEasyFraming;
  bool    bInfiniteSelect;
  bool    bAutoIso14443_4;
  uint8_t  btSupportByte;
  int     last_error;
};
struct acr122_pcsc_data {
  SCARDHANDLE hCard;
  SCARD_IO_REQUEST ioCard;
  uint8_t  abtRx[ACR122_PCSC_RESPONSE_LEN];
  size_t  szRx;
};
// 发送原始 APDU 命令
int transceive_raw(nfc_device *pnd, uint8_t *txData, size_t txLen, uint8_t *rxData, size_t *rxLen)
{
    if(((struct acr122_pcsc_data*)(pnd->driver_data))->ioCard.dwProtocol == SCARD_PROTOCOL_UNDEFINED)
        return (SCardControl(((struct acr122_pcsc_data*)(pnd->driver_data))->hCard, IOCTL_CCID_ESCAPE_SCARD_CTL_CODE, txData, txLen, rxData, *rxLen, rxLen) == SCARD_S_SUCCESS);
    else
        return (SCardTransmit(((struct acr122_pcsc_data*)(pnd->driver_data))->hCard, &((struct acr122_pcsc_data*)(pnd->driver_data))->ioCard, txData, txLen, NULL, rxData, rxLen) == SCARD_S_SUCCESS);
}

void led_to_green(nfc_device *pnd)
{
    // FF004000040A0A0101
    uint8_t txData[264], rxData[264];
    size_t txLen, rxLen;
    txLen=9;
    rxLen=sizeof(rxData);
    memcpy(txData, "\xFF\x00\x40\x22\x04\x02\x02\x01\x00", txLen);
    //memcpy(txData, "\xFF\x00\x40\x00\x04\x01\x01\x01\x00", txLen);
    transceive_raw(pnd, txData, txLen, rxData, &rxLen);
}

nfc_device *pnd;
nfc_target nt;
nfc_context *context;
bool quit_flag = false;

void stop_read(int sig)
{ 
    (void) sig;
    print_log(log_info, "Stop read... (sig:%d)\n", sig);
    if (pnd != NULL) {
        nfc_abort_command(pnd);
        quit_flag = true;
    } else {
        nfc_exit(context);
    }
}

int main(void)
{
    signal(SIGINT, stop_read);
    signal(SIGTERM, stop_read);

    // 初始化 libnfc
    nfc_init(&context);
    if(context == NULL)
    {
        print_log(log_error, "Unable to init libnfc (malloc).\n");
        return -1;
    }
    print_log(log_info, "Init libnfc %s successful.\n", nfc_version());

    // 打开 NFC 设备
    pnd = nfc_open(context, NULL);
    if(pnd == NULL)
    {
        print_log(log_error, "Unable to open NFC device.\n");
        return -1;
    }

    // 初始化 NFC 设备
    if(nfc_initiator_init(pnd) < 0)
    {
        nfc_perror(pnd, "nfc_initiator_init");
        return -1;
    }
    print_log(log_info, "NFC reader %s opened.\n", nfc_device_get_name(pnd));

    uint8_t txData[264], rxData[264];
    size_t txLen, rxLen;
    uint8_t oldUID[10];
    unsigned int sleepTime = 5000;

    // 指定卡型号（使用安卓应用 NFC TagInfo by NXP 读取校园卡查询到的型号）
    const nfc_modulation nmNEUCard = {
        .nmt = NMT_ISO14443A,
        .nbr = NBR_106,
    };

    while(!quit_flag)
    {
        if(nfc_initiator_select_passive_target(pnd, nmNEUCard, NULL, 0, &nt) > 0)
        {
            if(!memcmp(oldUID, nt.nti.nai.abtUid, sizeof(oldUID)))
            {
                nfc_initiator_deselect_target(pnd);
                usleep(sleepTime);
                printf("same card\n");
                continue;
            }
            //led_to_green(pnd);

            memcpy(oldUID, nt.nti.nai.abtUid, sizeof(oldUID));
            print_log(log_info, "New card: ");
            print_hex(nt.nti.nai.abtUid, nt.nti.nai.szUidLen);

            // 选择应用
            // 00A404000E4E432E65436172642E4444463031 19
            txLen=19;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xA4\x04\x00\x0E\x4E\x43\x2E\x65\x43\x61\x72\x64\x2E\x44\x44\x46\x30\x31", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0){ nfc_perror(pnd, "send:");
                return -1;}

            // 00A40000023F00 7
            txLen=7;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xA4\x00\x00\x02\x3F\x00", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;

            // 00A4000002EC01 7
            txLen=7;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xA4\x00\x00\x02\xEC\x01", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;

            // 获取学号
            // 00B0968414 5
            txLen=5;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xB0\x96\x84\x14", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;
            print_log(log_info, "学号：");
            print_hex(rxData, rxLen);

            // 获取姓名
            // 00B0960214 5
            txLen=5;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xB0\x96\x02\x14", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;
            print_log(log_info, "姓名：");
            print_hex(rxData, rxLen);

            // 获取身份证号
            // 00B0961612 5
            txLen=5;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xB0\x96\x16\x12", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;
            print_log(log_info, "身份证：");
            print_hex(rxData, rxLen);

            // 获取余额
            // 00A4000002EC11 7
            // 805C000204 5
            txLen=7;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x00\xA4\x00\x00\x02\xEC\x11", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;
            txLen=5;
            rxLen=sizeof(rxData);
            memcpy(txData, "\x80\x5C\x00\x02\x04", txLen);
            if(transceive(pnd, txData, txLen, rxData, &rxLen) < 0)
                return -1;
            print_log(log_info, "余额：");
            print_hex(rxData, rxLen);
            nfc_initiator_deselect_target(pnd);
        }
        else
        {
            nfc_initiator_deselect_target(pnd);
            printf("no card\n");
            usleep(sleepTime);
        }
    }
    return 0;
}
