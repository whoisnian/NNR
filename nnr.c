#include "nnr.h"

void print_log(LogType type, const char *format, ...)
{
    time_t timer;
    time(&timer);
    char formatStr[20];
    strftime(formatStr, 20, "%Y-%m-%d %H:%M:%S", localtime(&timer));

    if (type == log_info)
        printf("%s \033[32m[I]\033[0m ", formatStr);
    else if (type == log_warn)
        printf("%s \033[33m[W]\033[0m ", formatStr);
    else if (type == log_error)
        printf("%s \033[31m[E]\033[0m ", formatStr);
    else if (type == log_debug)
        printf("%s \033[35m[D]\033[0m ", formatStr);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

nnr_device *nnr_init(const char *readerName)
{
    nnr_device *pnd = (nnr_device *)malloc(sizeof(nnr_device));

    LONG rv;
    DWORD dwReaders, dwActiveProtocol;
    dwReaders = SCARD_AUTOALLOCATE;
    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &pnd->hContext);
    if (rv != SCARD_S_SUCCESS)
        return NULL;
    rv = SCardListReaders(pnd->hContext, NULL, (LPSTR)&pnd->mszReaders, &dwReaders);
    if (rv != SCARD_S_SUCCESS)
        return NULL;

    // 找到包含指定 readerName 的第一个读卡器
    pnd->szReader = pnd->mszReaders;
    while (*pnd->szReader && strstr(pnd->szReader, readerName) == NULL)
        pnd->szReader += strlen(pnd->szReader) + 1;
    if (!*pnd->szReader)
        return NULL;

    pnd->rgReaderState.szReader = pnd->szReader;
    pnd->rgReaderState.dwCurrentState = SCARD_STATE_UNAWARE;
    return pnd;
}

int nnr_close(nnr_device *pnd)
{
    LONG rv;
    rv = SCardFreeMemory(pnd->hContext, pnd->mszReaders);
    if (rv != SCARD_S_SUCCESS)
        return rv;
    rv = SCardReleaseContext(pnd->hContext);
    if (rv != SCARD_S_SUCCESS)
        return rv;
    free(pnd);
    return 0;
}

int nnr_wait_for_new_card(nnr_device *pnd)
{
    LONG rv = 0;
    while (1)
    {
        rv = SCardGetStatusChange(pnd->hContext, INFINITE, &pnd->rgReaderState, 1);
        if (rv == SCARD_E_CANCELLED)
            break;

        pnd->rgReaderState.dwCurrentState = pnd->rgReaderState.dwEventState;
        if (pnd->rgReaderState.dwEventState & SCARD_STATE_PRESENT)
            break;
    }
    return rv;
}

int nnr_cancel_wait(nnr_device *pnd)
{
    return SCardCancel(pnd->hContext);
}

int nnr_card_connect(nnr_device *pnd)
{
    LONG rv;
    DWORD dwActiveProtocol;
    rv = SCardConnect(pnd->hContext, pnd->szReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &pnd->hCard, &dwActiveProtocol);
    switch (dwActiveProtocol)
    {
    case SCARD_PROTOCOL_T0:
        pnd->pioSendPci = *SCARD_PCI_T0;
        break;
    case SCARD_PROTOCOL_T1:
        pnd->pioSendPci = *SCARD_PCI_T1;
        break;
    }
    return rv;
}

int nnr_card_disconnect(nnr_device *pnd)
{
    return SCardDisconnect(pnd->hCard, SCARD_LEAVE_CARD);
}

int nnr_transceive_bytes(nnr_device *pnd, const uint8_t *pbtTx, const size_t szTx, uint8_t *pbtRx, size_t *szRx)
{
    return SCardTransmit(pnd->hCard, &pnd->pioSendPci, pbtTx, szTx, NULL, pbtRx, szRx);
}
