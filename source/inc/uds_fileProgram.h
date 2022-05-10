/*
 * @Author       : jianhun
 * @Date         : 2021-10-29 20:08:30
 * @LastEditors  : your name
 * @LastEditTime : 2022-04-21 22:46:54
 * @FilePath     : \UDS_Base_CANable\source\inc\uds_fileProgram.h
 * @Description  : 
 */
#ifndef UDS_FILE_PROGRAM_H_
#define UDS_FILE_PROGRAM_H_

#include "cando_uds_device.h"
#include "uds_fileResolve.h"
#include "uds_CANdevice.h"
#include "uds_typedef.h"

typedef enum
{
    rSuccess,
    rFailed,
}B_RESULT;

typedef enum
{
    defSession,
    extSession,
    requestSeed_1,
    submitKey_1,
    RoutCtrlSer,
    extSessionECU,
    disDTCRecode,
    closeNetWMess,
    progSession,
    requestSeed_FBL,
    submitKey_FBL,
    wFingerInfo,
    reqDrvDownld,
    reqDrvTrans,
    reqDrvExit,
    checkDriver,
    eraseAppFlash,
    reqAppDownld,
    reqAppTrans,
    reqAppExit,
    checkApp,
    openNetWMess1,
    openNetWMess2,
    ecuReset,
    btComplete,
}B_STEPNAME;

typedef struct
{
    char*      name;
    B_STEPNAME curStep;
    B_STEPNAME passStep;
    B_STEPNAME failStep;
    tU8        allowRepeat;
    B_STEPNAME repeatStep;
    tU16       repeatCycle;
    tU16       waitTime;
}bStep_t;

typedef enum 
{
    dSend,
    dRecv,
}B_DIR;

typedef void (*pUdsFunc)(B_DIR);
typedef struct 
{
    B_STEPNAME    name;
    pUdsFunc      func;
}bFunc_t;

typedef enum
{
    LEVEL_1,
    LEVEL_FBL,
}securityAccessLevel;

typedef struct
{
    const tU8 xor[4];
    tU8 seed[4];
    tU8 cal[4];
    tU8 key[4];
    securityAccessLevel level;
}bAccess_t;

typedef struct
{
    tU8  *buff;
    tU16 len;
}bMsg_t;

typedef struct
{   
    tU8     downloaded;             //Downloaded segment
    // tU8     erased;                 //Erased segment
    tU32    dataAddress;            //segment data start address
    tU32    dataLength;             //segment date length
    tU32    dataOffset;             //segment data offset in data buffer
    tU32    dataCursor;             //segment data length has been downloaded
    tU32    maxDataLen;             //Maximum download length allowed by the client
    tU8     blockCount;             //current downloading block
    tU8     blockType;              //0-singular block   1-plural block
}bSeg_t;

extern HANDLE tComEndSem;
extern void startCommunicate(deviceCAN_t* device);

#endif 