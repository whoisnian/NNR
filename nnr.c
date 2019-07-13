#include "nnr.h"

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

nnr_device *nnr_init()
{
    nnr_device *pnd = (nnr_device *)malloc(sizeof(nnr_device));

    LONG rv;
    DWORD dwReaders, dwActiveProtocol;

    dwReaders = SCARD_AUTOALLOCATE;
    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &pnd->hContext);
    rv = SCardListReaders(pnd->hContext, NULL, pnd->readerName, &dwReaders);

    return pnd;
}

void nnr_close(nnr_device *pnd)
{
    LONG rv;
    if(pnd != NULL)
    {
        rv = SCardCancel(pnd->hContext);
        rv = SCardFreeMemory(pnd->hContext, pnd->readerName);
        rv = SCardReleaseContext(pnd->hContext);
        free(pnd);
    }
}

int nnr_wait_for_new_card(nnr_device *pnd)
{
    SCARD_READERSTATE rgReaderStates[2];
    rgReaderStates[0].szReader = pnd->readerName;
    //rgReaderStates[0].szReader = "ACS ACR122U PICC Interface 00 00";
    rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;
    rgReaderStates[1].szReader = "\\\\?PnP?\\Notification";
    rgReaderStates[1].dwCurrentState = SCARD_STATE_UNAWARE;

    LONG rv;
    while(1)
    {
        rv = SCardGetStatusChange(pnd->hContext, INFINITE, rgReaderStates, 2);
        if(rgReaderStates[0].dwEventState&SCARD_STATE_PRESENT)
        {
            return 0;
        }
        else
        {
            rgReaderStates[0].dwCurrentState = rgReaderStates[0].dwEventState;
        }
    }
}

int nnr_card_connect(nnr_device *pnd)
{
    LONG rv;
    DWORD dwActiveProtocol;

    rv = SCardConnect(pnd->hContext, pnd->readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1, &pnd->hCard, &dwActiveProtocol);
    switch(dwActiveProtocol)
    {
        case SCARD_PROTOCOL_T0:
            pnd->pioSendPci = *SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pnd->pioSendPci = *SCARD_PCI_T1;
        break;
    }
    return 0;
}

int nnr_card_disconnect(nnr_device *pnd)
{
    LONG rv;

    rv = SCardDisconnect(pnd->hCard, SCARD_LEAVE_CARD);
    return 0;
}

int nnr_transceive_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx)
{
    return -1;
}

int nnr_transceive_raw_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx)
{
    LONG rv;
    rv = SCardTransmit(pnd->hCard, &pnd->pioSendPci, pbtTx, szTx, NULL, pbtRx, szRx);
    return rv;
}