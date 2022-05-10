#include "uds_fileProgram.h"

#define REQ_PHYS 0x73B
#define REQ_FUNC 0x7DF
#define RES_ID 0x7BB

#define UDS_TX_MAX_LEN 4095
#define UDS_RX_MAX_LEN 1024

#define FLASH_SECTOR_SIZE 0x200

#define XOR_TAB     { 0X12, 0X34, 0X56, 0X78 }
#define ZERO_TAB    { 0x00, 0x00, 0x00, 0x00 }

/******************************************************************************
 * 函数定义区
 *****************************************************************************/
static DWORD WINAPI programThread(LPVOID pM); // 编程线程函数，所有UDS操作在里面完成

static void calculateKey(bAccess_t *pCtrl);
static tU8 addressInSameSector(tU32 lastAddr, tU32 curAddr);

static void func_defaultSession(B_DIR dir);
static void func_externSession(B_DIR dir);
static void func_requestSeed(B_DIR dir);
static void func_submitKey(B_DIR dir);
static void func_RoutCtrlSer(B_DIR dir);
static void func_extSessionECU(B_DIR dir);
static void func_disDTCRecord(B_DIR dir);
static void func_closeNetWMess(B_DIR dir);
static void func_programSession(B_DIR dir);
static void func_wFingerInfo(B_DIR dir);
static void func_reqDrvDownld(B_DIR dir);
static void func_reqDrvTrans(B_DIR dir);
static void func_reqDrvExit(B_DIR dir);
static void func_checkDriver(B_DIR dir);
static void func_eraseAppFlash(B_DIR dir);
static void func_reqAppDownld(B_DIR dir);
static void func_reqAppTrans(B_DIR dir);
static void func_reqAppExit(B_DIR dir);
static void func_checkApp(B_DIR dir);
static void func_openNetWMess(B_DIR dir);
static void func_ecuReset(B_DIR dir);

/******************************************************************************
 * 全局变量定义区
 *****************************************************************************/
HANDLE tComEndSem; // 信号量句柄，可控制进程的关闭

static bSeg_t seg;              // 读取到的传输文件变量
static bMsg_t txMsg, rxMsg;     // UDS发送变量和接受变量
static tU16 P2_server_max = 50; // UDS最大延时时间
static tU16 P21_server_max = 5000;
static bAccess_t access = {XOR_TAB, ZERO_TAB, ZERO_TAB, ZERO_TAB, LEVEL_1}; // 密匙控制变量
static tU8 netwManagment;                                                   // 网络管理控制变量

/***************************************************************
 * 状态机变量控制区
 **************************************************************/
static B_STEPNAME step;
static B_RESULT result;

static bStep_t stepList[] =
    {
        //name           curStep         passStep        failStep   allowRepeat  repeatStep     repeatCycle   waitTime
        {"defSession", defSession, extSession, btComplete, 0, defSession, 0, 50},
        {"extSession", extSession, requestSeed_1, btComplete, 1, extSession, 0, 50},
        {"requestSeed_1", requestSeed_1, submitKey_1, btComplete, 0, extSession, 0, 50},
        {"submitKey_1", submitKey_1, RoutCtrlSer, btComplete, 0, extSession, 0, 50},
        {"RoutCtrlSer", RoutCtrlSer, extSessionECU, btComplete, 0, extSession, 0, 50},
        {"extSessionECU", extSessionECU, disDTCRecode, btComplete, 1, disDTCRecode, 0, 50},
        {"disDTCRecode", disDTCRecode, closeNetWMess, btComplete, 1, closeNetWMess, 0, 50},
        {"closeNetWMess", closeNetWMess, progSession, btComplete, 0, closeNetWMess, 0, 50},
        {"progSession", progSession, requestSeed_FBL, btComplete, 0, extSession, 0, 50},
        {"requestSeed_FBL", requestSeed_FBL, submitKey_FBL, btComplete, 0, extSession, 0, 50},
        {"submitKey_FBL", submitKey_FBL, wFingerInfo, btComplete, 0, extSession, 0, 50},
        {"wFingerInfo", wFingerInfo, reqDrvDownld, btComplete, 0, extSession, 0, 50},
        {"reqDrvDownld", reqDrvDownld, reqDrvTrans, btComplete, 0, extSession, 0, 50},
        {"reqDrvTrans", reqDrvTrans, reqDrvExit, btComplete, 0, reqDrvTrans, 0, 50},
        {"reqDrvExit", reqDrvExit, checkDriver, btComplete, 0, reqDrvDownld, 0, 50},
        {"checkDriver", checkDriver, eraseAppFlash, btComplete, 0, extSession, 0, 50},
        {"eraseAppFlash", eraseAppFlash, reqAppDownld, btComplete, 0, extSession, 0, 50},
        {"reqAppDownld", reqAppDownld, reqAppTrans, btComplete, 0, extSession, 0, 50},
        {"reqAppTrans", reqAppTrans, reqAppExit, btComplete, 0, reqAppTrans, 0, 50},
        {"reqAppExit", reqAppExit, checkApp, btComplete, 0, eraseAppFlash, 0, 50},
        {"checkApp", checkApp, openNetWMess1, btComplete, 0, extSession, 0, 50},
        {"openNetWMess1", openNetWMess1, openNetWMess2, btComplete, 1, openNetWMess2, 0, 50},
        {"openNetWMess2", openNetWMess2, ecuReset, btComplete, 1, ecuReset, 0, 50},
        {"ecuReset", ecuReset, btComplete, btComplete, 0, extSession, 0, 50},
};

static bFunc_t funcList[] =
    {
        //name              function
        {defSession, func_defaultSession},
        {extSession, func_externSession},
        {requestSeed_1, func_requestSeed},
        {submitKey_1, func_submitKey},
        {RoutCtrlSer, func_RoutCtrlSer},
        {extSessionECU, func_extSessionECU},
        {disDTCRecode, func_disDTCRecord},
        {closeNetWMess, func_closeNetWMess},
        {progSession, func_programSession},
        {requestSeed_FBL, func_requestSeed},
        {submitKey_FBL, func_submitKey},
        {wFingerInfo, func_wFingerInfo},
        {reqDrvDownld, func_reqDrvDownld},
        {reqDrvTrans, func_reqDrvTrans},
        {reqDrvExit, func_reqDrvExit},
        {checkDriver, func_checkDriver},
        {eraseAppFlash, func_eraseAppFlash},
        {reqAppDownld, func_reqAppDownld},
        {reqAppTrans, func_reqAppTrans},
        {reqAppExit, func_reqAppExit},
        {checkApp, func_checkApp},
        {openNetWMess1, func_openNetWMess},
        {openNetWMess2, func_openNetWMess},
        {ecuReset, func_ecuReset},
};

void startCommunicate(deviceCAN_t *device)
{
    HANDLE tProgHandle;
    DWORD tProgPID;

    tComEndSem = CreateSemaphore(NULL, 0, 1, NULL);
    tProgHandle = CreateThread(NULL, 0, programThread, (LPVOID)device, 0, &tProgPID);
    CloseHandle(tProgHandle);
}

static DWORD WINAPI programThread(LPVOID pM)
{
    CAN_UDS_ADDR udsParm;
    deviceCAN_t *udsDev = (deviceCAN_t *)pM;
    int ret;

    udsParm.ExternFlag = 0;
    udsParm.AddrFormats = 0;
    udsParm.ReqID = REQ_PHYS;
    udsParm.ResID = RES_ID;
    step = defSession;
    result = rSuccess;
    txMsg.buff = malloc(UDS_TX_MAX_LEN);
    rxMsg.buff = malloc(UDS_RX_MAX_LEN);

    LOG_INF("Start CAN Bootloader...");
    LOG_INF("Waiting for connecting...");
    while (btComplete != step)
    {
        funcList[step].func(dSend);                                              //select which sevice to send and ready to send
        ret = CAN_UDS_Request(udsDev->mHandle, &udsParm, txMsg.buff, txMsg.len); //uds can send message
        if (ret != CAN_UDS_OK)
        {
            LOG_ERR("CAN uds Request Failed! %d", ret);
        }
#ifdef DBG_TX_STREAM
        else
        {
            printf("[log D] Request:");
            for (int i = 0; i < txMsg.len; i++)
            {
                printf(" %02X", txMsg.buff[i]);
            }
            printf("\r\n");
        }
#endif
    __continueWait:
        //uds can receive message successful
        ret = CAN_UDS_Response(udsDev->mHandle, &udsParm, rxMsg.buff, stepList[step].waitTime);
        if (ret > CAN_UDS_OK)
        {
#ifdef DBG_RX_STREAM
            printf("[LOG D] Response:");
            for (int i = 0; i < ret; i++)
            {
                printf(" %02X", rxMsg.buff[i]);
            }
            printf("\r\n");
#endif
            if (0x7F == rxMsg.buff[0])
            {
                //nagetive response received
                if (0x78 == rxMsg.buff[2])
                {
                    // stepList[step].waitTime = P21_server_max;
                    stepList[step].waitTime = 5000;
                    goto __continueWait;
                }
                else
                {
                    LOG_ERR("Service: <%02X> NRC [%02X] Received!", rxMsg.buff[1], rxMsg.buff[2]);
                    step = stepList[step].failStep;
                    result = rFailed;
                }
            }
            else
            {
                //positive response received
                funcList[step].func(dRecv);
            }
        }
        else
        {
            //uds can receive message failed
            switch (ret)
            {
            case CAN_UDS_TIMEOUT_A:
            case CAN_UDS_TIMEOUT_Bs:
                if (stepList[step].allowRepeat)
                {
                    if (0 == strcmp("extSessionECU",stepList[step].name) || \
                        0 == strcmp("disDTCRecode",stepList[step].name) || \
                        0 == strcmp("openNetWMess1",stepList[step].name) || \
                        0 == strcmp("openNetWMess2",stepList[step].name)) {
                        stepList[step].waitTime = P2_server_max;
                    }
                    step = stepList[step].repeatStep;
                }
                else
                {
                    LOG_ERR("Step: <%s> Timeout!Error code: %d", stepList[step].name, ret);
                    step = stepList[step].failStep;
                    result = rFailed;
                }
                break;
            default:
                LOG_ERR("Step: <%s> Error! Code: %d", stepList[step].name, ret);
                step = stepList[step].failStep;
                result = rFailed;
                break;
            }
        }
    }

    if (rSuccess == result)
        LOG_DBG("Download flash file successfully!");
    else
        LOG_ERR("Flash file is downloaded failed!");

    free(txMsg.buff);
    txMsg.buff = NULL;
    free(rxMsg.buff);
    txMsg.buff = NULL;

    ReleaseSemaphore(tComEndSem, 1, NULL);

    return 0;
}

static void func_defaultSession(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x10;
        txMsg.buff[1] = 0x01;
        txMsg.len = 2;
    }
    else
    {
        if (rxMsg.buff[0] == 0x50 && rxMsg.buff[1] == 0x01)
        {
            LOG_INF("Step:Enter default session...");
            step = stepList[step].passStep;
        }
        else
        {
            step = stepList[step].failStep;
        }
        result = rSuccess;
    }
}

static void func_externSession(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x10;
        txMsg.buff[1] = 0x03;
        txMsg.len = 2;
    }
    else
    {
        if (rxMsg.buff[0] == 0x50 && rxMsg.buff[1] == 0x03)
        {
            LOG_INF("Step:Enter external session...");
            P2_server_max = rxMsg.buff[2] * 256 + rxMsg.buff[3];
            P21_server_max = rxMsg.buff[4] * 256 + rxMsg.buff[5];
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            access.level = LEVEL_1;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x10);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_requestSeed(B_DIR dir)
{
    tU8 SubFun;

    if (access.level == LEVEL_1)
    {
        LOG_INF("Step:Request Seed - level 1...");
        SubFun = 0x01;
    }
    else
    {
        LOG_INF("Step:Request Seed - level 2...");
        SubFun = 0x09;
    }
    if (dSend == dir)
    {

        txMsg.buff[0] = 0x27;
        txMsg.buff[1] = SubFun;
        txMsg.len = 2;
    }
    else
    {
        if (rxMsg.buff[0] == 0x67 && rxMsg.buff[1] == SubFun)
        {
            if (access.level == LEVEL_1)
            {
                LOG_INF("Step:Accept Seed - level 1...");
            }
            else
            {
                LOG_INF("Step:Accept Seed - level 2...");
            }
            access.seed[0] = rxMsg.buff[2];
            access.seed[1] = rxMsg.buff[3];
            access.seed[2] = rxMsg.buff[4];
            access.seed[3] = rxMsg.buff[5];
            calculateKey(&access);
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x27);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_submitKey(B_DIR dir)
{
    tU8 SubFun;

    if (access.level == LEVEL_1)
    {
        LOG_INF("Step:Submit Key - level 1...");
        SubFun = 0x02;
    }
    else
    {
        LOG_INF("Step:Submit Key - level 2...");
        SubFun = 0x0a;
    }
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x27;
        txMsg.buff[1] = SubFun;
        txMsg.buff[2] = access.key[0];
        txMsg.buff[3] = access.key[1];
        txMsg.buff[4] = access.key[2];
        txMsg.buff[5] = access.key[3];
        txMsg.len = 6;
    }
    else
    {
        if (rxMsg.buff[0] == 0x67 && rxMsg.buff[1] == SubFun)
        {
            if (access.level == LEVEL_1)
            {
                LOG_INF("Step:Unlock Security Level 1 Successfully...");
            }
            else
            {
                LOG_INF("Step:Unlock Security Level 2 Successfully...");
            }

            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x27);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_RoutCtrlSer(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x31;
        txMsg.buff[1] = 0x01;
        txMsg.buff[2] = 0x02;
        txMsg.buff[3] = 0x03;
        txMsg.len = 4;
    }
    else
    {
        if (rxMsg.buff[0] == 0x71 && rxMsg.buff[1] == 0x01 && rxMsg.buff[2] == 0x02 && rxMsg.buff[3] == 0x03)
        {
            if (rxMsg.buff[4] == 0x00)
            {
                LOG_INF("Step:Check Flash Environment: Passed...");
                step = stepList[step].passStep;
                stepList[step].waitTime = P2_server_max;
                result = rSuccess;
            }
            else
            {
                LOG_ERR("Step:Check Flash Environment: Failed...");
                step = stepList[step].failStep;
                result = rFailed;
            }
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x31);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_extSessionECU(B_DIR dir)
{
    if (dSend == dir)
    {
        LOG_INF("Step:Enter Whole Vehicle External Session...");
        txMsg.buff[0] = 0x10;
        txMsg.buff[1] = 0x83;
        txMsg.len = 2;
    }
    else
    {
        if (rxMsg.buff[0] == 0x50 && rxMsg.buff[1] == 0x83)
        {
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x10);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_disDTCRecord(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x85;
        txMsg.buff[1] = 0x82;
        txMsg.len = 2;
    }
    else
    {
        if (rxMsg.buff[0] == 0xC5 && rxMsg.buff[1] == 0x82)
        {
            LOG_INF("Step:Prohibit Recording DTC...");
            netwManagment = 0;
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x85);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_closeNetWMess(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x28;
        txMsg.buff[1] = (netwManagment == 0) ? 0x01 : 0x03;
        txMsg.buff[2] = 0x03;
        txMsg.len = 3;
    }
    else
    {
        if (rxMsg.buff[0] == 0x68 && rxMsg.buff[1] == txMsg.buff[1])
        {
            if (netwManagment == 0)
            {
                LOG_INF("Step:Close Network Communication...");
                step = stepList[step].repeatStep;
                netwManagment = 1;
            }
            else
            {
                LOG_INF("Step:Close Network Management Communication...");
                step = stepList[step].passStep;
            }
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x28);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_programSession(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x10;
        txMsg.buff[1] = 0x02;
        txMsg.len = 2;
        LOG_INF("Step:Enter Program Session...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x50 && rxMsg.buff[1] == 0x02)
        {
            P2_server_max = rxMsg.buff[2] * 256 + rxMsg.buff[3];
            P21_server_max = rxMsg.buff[4] * 256 + rxMsg.buff[5];
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            access.level = LEVEL_FBL;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x10);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_wFingerInfo(B_DIR dir)
{
    if (dSend == dir)
    {
        time_t rawtime;
        struct tm *timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        txMsg.buff[0] = 0x2E;
        txMsg.buff[1] = 0xF1;
        txMsg.buff[2] = 0x84;
        txMsg.buff[3] = 0x01;
        txMsg.buff[4] = (tU8)(timeinfo->tm_year - 100);
        txMsg.buff[5] = (tU8)(1 + timeinfo->tm_mon);
        txMsg.buff[6] = (tU8)timeinfo->tm_mday;
        txMsg.buff[7] = 0x11;
        txMsg.buff[8] = 0x22;
        txMsg.buff[9] = 0x33;
        txMsg.buff[10] = 0x44;
        txMsg.buff[11] = 0x55;
        txMsg.buff[12] = 0x66;
        txMsg.len = 13;
        LOG_INF("Step:Write Finger information...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x6E && rxMsg.buff[1] == 0xF1 && rxMsg.buff[2] == 0x84)
        {
            seg.downloaded = 0;
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x2E);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqDrvDownld(B_DIR dir)
{
    if (dSend == dir)
    {
        seg.dataAddress = driverFile.segment[seg.downloaded].startAddr;
        seg.dataLength = driverFile.segment[seg.downloaded].dataLength;
        seg.dataOffset = driverFile.segment[seg.downloaded].startAddrInBuff;
        seg.dataCursor = 0;
        seg.blockCount = 0;

        txMsg.buff[0] = 0x34;
        txMsg.buff[1] = 0x00;
        txMsg.buff[2] = 0x44;
        txMsg.buff[3] = (tU8)(seg.dataAddress >> 24);
        txMsg.buff[4] = (tU8)(seg.dataAddress >> 16);
        txMsg.buff[5] = (tU8)(seg.dataAddress >> 8);
        txMsg.buff[6] = (tU8)seg.dataAddress;
        txMsg.buff[7] = (tU8)(seg.dataLength >> 24);
        txMsg.buff[8] = (tU8)(seg.dataLength >> 16);
        txMsg.buff[9] = (tU8)(seg.dataLength >> 8);
        txMsg.buff[10] = (tU8)seg.dataLength;
        txMsg.len = 11;
        LOG_INF("Step:Request Driver Download - Address:0x%08lX,Length:0x%08lX...", seg.dataAddress, seg.dataLength);
    }
    else
    {
        if (rxMsg.buff[0] == 0x74 && rxMsg.buff[1] == 0x20)
        {
            seg.maxDataLen = ((tU32)rxMsg.buff[2] << 8) + (tU32)rxMsg.buff[3];
            seg.maxDataLen = ((seg.maxDataLen > UDS_TX_MAX_LEN) || (seg.maxDataLen == 0)) ? UDS_TX_MAX_LEN : seg.maxDataLen;
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x34);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqDrvTrans(B_DIR dir)
{
    if (dSend == dir)
    {
        tU32 dataPosition;
        tU16 dataSize;

        if ((seg.dataCursor + (seg.maxDataLen - 2)) >= seg.dataLength)
        {
            dataSize = seg.dataLength - seg.dataCursor;
            seg.blockType = 0;
        }
        else
        {
            dataSize = seg.maxDataLen - 2;
            seg.blockType = 1;
        }
        dataPosition = seg.dataOffset + seg.dataCursor;
        txMsg.buff[0] = 0x36;
        txMsg.buff[1] = ++seg.blockCount;
        memcpy(txMsg.buff + 2, driverFile.buffer + dataPosition, dataSize);
        txMsg.len = dataSize + 2;
        LOG_INF("Step:Request Driver Data Transfer - block:[%03d],length[%03d]...", seg.blockCount, dataSize);
    }
    else
    {
        if (rxMsg.buff[0] == 0x76 && rxMsg.buff[1] == seg.blockCount)
        {
            if (seg.blockType)
            {
                seg.dataCursor += seg.maxDataLen - 2;
                step = stepList[step].repeatStep;
            }
            else
            {
                seg.dataCursor = seg.dataLength;
                step = stepList[step].passStep;
            }
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x36);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqDrvExit(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x37;
        txMsg.buff[1] = (tU8)(driverFile.segment[seg.downloaded].crc32 >> 24);
        txMsg.buff[2] = (tU8)(driverFile.segment[seg.downloaded].crc32 >> 16);
        txMsg.buff[3] = (tU8)(driverFile.segment[seg.downloaded].crc32 >> 8);
        txMsg.buff[4] = (tU8)(driverFile.segment[seg.downloaded].crc32);
        txMsg.len = 5;
        LOG_INF("Step:Request Driver Download Exit...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x77 && rxMsg.buff[1] == txMsg.buff[1] && rxMsg.buff[2] == txMsg.buff[2] && rxMsg.buff[3] == txMsg.buff[3] && rxMsg.buff[4] == txMsg.buff[4])
        {
            LOG_INF("Step:Drive Download Exit Successfully...");
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x37);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_checkDriver(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x31;
        txMsg.buff[1] = 0x01;
        txMsg.buff[2] = 0x02;
        txMsg.buff[3] = 0x02;
        txMsg.len = 4;
        LOG_INF("Step:Check Driver Download Completeness...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x71 && rxMsg.buff[1] == 0x01 && rxMsg.buff[2] == 0x02 && rxMsg.buff[3] == 0x02)
        {
            if (rxMsg.buff[4] == 0x00)
            {
                seg.downloaded = 0;
                step = stepList[step].passStep;
                stepList[step].waitTime = P2_server_max;
                result = rSuccess;
            }
            else
            {
                LOG_ERR("Flash Driver Test Failed!");
                step = stepList[step].failStep;
                result = rFailed;
            }
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x31);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_eraseAppFlash(B_DIR dir)
{
    tU32 startAddr, dataLength;
    if (dSend == dir)
    {
        seg.dataAddress = appFile.segment[seg.downloaded].startAddr;
        seg.dataLength = appFile.segment[seg.downloaded].dataLength;
        seg.dataOffset = appFile.segment[seg.downloaded].startAddrInBuff;
        seg.dataCursor = 0;
        seg.blockCount = 0;

        if (0 == seg.downloaded)
        {
            startAddr = seg.dataAddress;
            dataLength = seg.dataLength;
        }
        else
        {
            if (addressInSameSector((appFile.segment[seg.downloaded - 1].startAddr + appFile.segment[seg.downloaded - 1].dataLength), seg.dataAddress))
            {
                startAddr = ((seg.dataAddress / FLASH_SECTOR_SIZE) + 1) * FLASH_SECTOR_SIZE;
                dataLength = seg.dataLength + seg.dataAddress - startAddr + 1;
            }
            else
            {
                startAddr = seg.dataAddress;
                dataLength = seg.dataLength;
            }
        }

        txMsg.buff[0] = 0x31;
        txMsg.buff[1] = 0x01;
        txMsg.buff[2] = 0xFF;
        txMsg.buff[3] = 0x00;
        // txMsg.buff[4] = 0x44;
        // txMsg.buff[5] = (tU8)((startAddr) >> 24);
        // txMsg.buff[6] = (tU8)((startAddr) >> 16);
        // txMsg.buff[7] = (tU8)((startAddr) >> 8);
        // txMsg.buff[8] = (tU8)(startAddr);
        // txMsg.buff[9] = (tU8)((dataLength) >> 24);
        // txMsg.buff[10] = (tU8)((dataLength) >> 16);
        // txMsg.buff[11] = (tU8)((dataLength) >> 8);
        // txMsg.buff[12] = (tU8)(dataLength);
        // txMsg.len = 13;
        txMsg.buff[4] = (tU8)((startAddr)>>24);
        txMsg.buff[5] = (tU8)((startAddr)>>16);
        txMsg.buff[6] = (tU8)((startAddr)>>8);
        txMsg.buff[7] = (tU8)(startAddr);
        txMsg.buff[8] = (tU8)((dataLength)>>24);
        txMsg.buff[9] = (tU8)((dataLength)>>16);
        txMsg.buff[10] = (tU8)((dataLength)>>8);
        txMsg.buff[11] = (tU8)(dataLength);
        txMsg.len = 12;
        LOG_INF("Step:Erase App Flash - Address:0x%08lX,Length:0x%08lX...", startAddr, dataLength);
    }
    else
    {
        if (rxMsg.buff[0] == 0x71 && rxMsg.buff[1] == 0x01 && rxMsg.buff[2] == 0xFF && rxMsg.buff[3] == 0x00)
        {
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x31);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqAppDownld(B_DIR dir)
{
    if (dSend == dir)
    {
        {
            txMsg.buff[0] = 0x34;
            txMsg.buff[1] = 0x00;
            txMsg.buff[2] = 0x44;
            txMsg.buff[3] = (tU8)((seg.dataAddress) >> 24);
            txMsg.buff[4] = (tU8)((seg.dataAddress) >> 16);
            txMsg.buff[5] = (tU8)((seg.dataAddress) >> 8);
            txMsg.buff[6] = (tU8)(seg.dataAddress);
            txMsg.buff[7] = (tU8)((seg.dataLength) >> 24);
            txMsg.buff[8] = (tU8)((seg.dataLength) >> 16);
            txMsg.buff[9] = (tU8)((seg.dataLength) >> 8);
            txMsg.buff[10] = (tU8)(seg.dataLength);
            txMsg.len = 11;
            LOG_INF("Step:Request App Download - Address:0x%08lX,Length:0x%08lX...", seg.dataAddress, seg.dataLength);
        }
    }
    else
    {
        if (rxMsg.buff[0] == 0x74 && rxMsg.buff[1] == 0x20)
        {
            seg.maxDataLen = ((tU32)rxMsg.buff[2] << 8) + (tU32)rxMsg.buff[3];
            seg.maxDataLen = ((seg.maxDataLen > UDS_TX_MAX_LEN) || (seg.maxDataLen == 0)) ? UDS_TX_MAX_LEN : seg.maxDataLen;
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x34);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqAppTrans(B_DIR dir)
{
    if (dSend == dir)
    {
        tU32 dataPosition;
        tU16 dataSize;

        if (seg.dataCursor + (seg.maxDataLen - 2) >= seg.dataLength)
        {
            dataSize = seg.dataLength - seg.dataCursor;
            seg.blockType = 0;
        }
        else
        {
            dataSize = seg.maxDataLen - 2;
            seg.blockType = 1;
        }
        txMsg.buff[0] = 0x36;
        txMsg.buff[1] = ++seg.blockCount;
        dataPosition = seg.dataOffset + seg.dataCursor;
        memcpy(txMsg.buff + 2, appFile.buffer + dataPosition, dataSize);
        txMsg.len = dataSize + 2;
        LOG_INF("Step:Request App Data Transfer - block:[%03d],length:[%03d]...", seg.blockCount, dataSize);
    }
    else
    {
        if (rxMsg.buff[0] == 0x76 && rxMsg.buff[1] == seg.blockCount)
        {
            if (seg.blockType)
            {
                seg.dataCursor += seg.maxDataLen - 2;
                step = stepList[step].repeatStep;
            }
            else
            {
                seg.dataCursor = seg.dataLength;
                step = stepList[step].passStep;
            }
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x36);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_reqAppExit(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x37;
        txMsg.buff[1] = (tU8)(appFile.segment[seg.downloaded].crc32 >> 24);
        txMsg.buff[2] = (tU8)(appFile.segment[seg.downloaded].crc32 >> 16);
        txMsg.buff[3] = (tU8)(appFile.segment[seg.downloaded].crc32 >> 8);
        txMsg.buff[4] = (tU8)(appFile.segment[seg.downloaded].crc32);
        txMsg.len = 5;
        LOG_INF("Step:Request App Download Exit...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x77 && rxMsg.buff[1] == txMsg.buff[1] && rxMsg.buff[2] == txMsg.buff[2] && rxMsg.buff[3] == txMsg.buff[3] && rxMsg.buff[4] == txMsg.buff[4])
        {
            if (++seg.downloaded < appFile.count)
            {
                step = stepList[step].repeatStep;
            }
            else
            {
                step = stepList[step].passStep;
            }
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x37);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_checkApp(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x31;
        txMsg.buff[1] = 0x01;
        txMsg.buff[2] = 0xFF;
        txMsg.buff[3] = 0x01;
        txMsg.len = 4;
        LOG_INF("Step:Check App Download Completeness...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x71 && rxMsg.buff[1] == 0x01 && rxMsg.buff[2] == 0xFF && rxMsg.buff[3] == 0x01)
        {
            if (!rxMsg.buff[4])
            {
                netwManagment = 0;
                step = stepList[step].passStep;
                stepList[step].waitTime = P2_server_max;
                result = rSuccess;
            }
            else
            {
                LOG_ERR("Flash App Test Failed!");
                step = stepList[step].failStep;
                result = rFailed;
            }
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x31);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_openNetWMess(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x28;
        txMsg.buff[1] = 0x80;
        txMsg.buff[2] = (netwManagment == 0) ? 0x01 : 0x03;
        txMsg.len = 3;
        if (netwManagment == 0)
        {
            LOG_INF("Step:Open Network Communication...");
            netwManagment = 1;
        }
        else
        {
            LOG_INF("Step:Open Network Management Communication...");
        }
    }
    else
    {
        if (rxMsg.buff[0] == 0x28 && rxMsg.buff[1] == 0x80 && rxMsg.buff[2] == txMsg.buff[2])
        {
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x11);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void func_ecuReset(B_DIR dir)
{
    if (dSend == dir)
    {
        txMsg.buff[0] = 0x11;
        txMsg.buff[1] = 0x01;
        txMsg.len = 2;
        LOG_INF("Step:Request ECU restart...");
    }
    else
    {
        if (rxMsg.buff[0] == 0x51 && rxMsg.buff[1] == 0x01)
        {
            step = stepList[step].passStep;
            stepList[step].waitTime = P2_server_max;
            result = rSuccess;
        }
        else
        {
            LOG_ERR("Service: <%02X> Unexpected Message Received!", 0x11);
            step = stepList[step].failStep;
            result = rFailed;
        }
    }
}

static void calculateKey(bAccess_t *pCtrl)
{
    for (int i = 0; i < 4; i++)
    {
        pCtrl->cal[i] = pCtrl->seed[i] ^ pCtrl->xor [i];
    }

    if (pCtrl->level == LEVEL_1)
    {
        pCtrl->key[0] = ((pCtrl->cal[0] & 0x0f) << 4) | (pCtrl->cal[1] & 0xf0);
        pCtrl->key[1] = ((pCtrl->cal[1] & 0x0f) << 4) | (pCtrl->cal[2] & 0xf0);
        pCtrl->key[2] = ((pCtrl->cal[2] & 0xf0) << 4) | (pCtrl->cal[3] & 0xf0);
        pCtrl->key[3] = ((pCtrl->cal[3] & 0x0f) << 4) | (pCtrl->cal[0] & 0x0f);
    }
    else
    {
        pCtrl->key[0] = ((pCtrl->cal[0] & 0xf0) >> 4) | (pCtrl->cal[1] & 0xf0);
        pCtrl->key[1] = ((pCtrl->cal[1] & 0xf0) >> 4) | (pCtrl->cal[2] & 0xf0);
        pCtrl->key[2] = ((pCtrl->cal[2] & 0xf0) >> 4) | (pCtrl->cal[3] & 0xf0);
        pCtrl->key[3] = ((pCtrl->cal[3] & 0x0f) >> 4) | (pCtrl->cal[0] & 0xf0);
    }
}

static tU8 addressInSameSector(tU32 lastAddr, tU32 curAddr)
{
    return (((lastAddr - 1) / FLASH_SECTOR_SIZE) == (curAddr / FLASH_SECTOR_SIZE)) ? 1 : 0;
}
