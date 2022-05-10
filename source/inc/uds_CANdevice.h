/*
 * @Author       : jianhun
 * @Date         : 2021-11-18 21:58:11
 * @LastEditors  : jianhun
 * @LastEditTime : 2021-12-01 21:42:14
 * @FilePath     : \UDS_Base_CANable\source\inc\uds_CANdevice.h
 * @Description  : 
 */
#ifndef UDS_CAN_DEVICE_H_
#define UDS_CAN_DEVICE_H_

#include "cando.h"

#define CANDO_TIMING_500K     {1,12,2,1,6}

//Cando 设备句柄结构定义
typedef struct
{
    cando_list_handle mListHandle;
    cando_handle      mHandle;
    cando_bittiming_t timing;
    int               deviceCnt;
}deviceCAN_t;


extern int scanDecive(deviceCAN_t* device);
extern int openDecive(deviceCAN_t* device);
extern int closeDevice(deviceCAN_t* device);
extern int showDeviceInfo(deviceCAN_t* device);

#endif 
