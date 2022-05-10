/*
 * @Author       : jianhun
 * @Date         : 2021-12-01 21:39:14
 * @LastEditors  : jianhun
 * @LastEditTime : 2022-05-10 22:17:39
 * @FilePath     : \UDS_Base_CANable\source\inc\cando_uds_device.h
 * @Description  : 
 */

#ifndef CANDO_UDS_DEVICE_H_
#define CANDO_UDS_DEVICE_H_

#include "cando.h"

//函数返回值错误定义
#define CAN_UDS_OK              0
#define CAN_UDS_SEND_FAIL       -31
#define CAN_UDS_READ_FAIL       -32
#define CAN_UDS_TIMEOUT_A       -33
#define CAN_UDS_TIMEOUT_Bs      -34
#define CAN_UDS_READ_ERR_FAIL   -35
#define CAN_UDS_BUFFER_OVFLW    -36
#define CAN_UDS_TP_FORMAT       -37
#define CAN_UDS_FS_OVFLW        -38
#define CAN_UDS_WRONG_SN        -39
#define CAN_UDS_UNEXP_PDU       -40
#define CAN_UDS_INVALID_FS      -41
#define CAN_UDS_ERROR           -60
#define CAN_UDS_BUSOFF          -64
#define CAN_UDS_RX_TX_WARNING   -65
#define CAN_UDS_RX_TX_PASSIVE   -66
#define CAN_UDS_BUS_OVERLOAD    -67
#define CAN_UDS_FORMAT          -68
#define CAN_UDS_STUFF           -69
#define CAN_UDS_ACK             -70
#define CAN_UDS_BIT_RECESSIVE   -71
#define CAN_UDS_BIT_DOMINANT    -72
#define CAN_UDS_CRC             -73

//CAN UDS地址定义
typedef  struct  _CAN_UDS_ADDR
{
    unsigned int    ReqID;        //请求报文ID。
    unsigned int    ResID;        //应答报文ID。
    unsigned char   ExternFlag;   //0-标准帧，1-扩展帧
    unsigned char   AddrFormats;  //0-normal, 1-extended ,2-mixed
    unsigned char   AddrExt;      //当AddrFormats不为normal时，该数据放到CAN数据域第1字节
    unsigned char   __Res;
}CAN_UDS_ADDR;


extern int CAN_UDS_Request(cando_handle DevHandle,CAN_UDS_ADDR *pUDSAddr,unsigned char *pReqData,int DataLen);
extern int CAN_UDS_Response(cando_handle DevHandle,CAN_UDS_ADDR *pUDSAddr,unsigned char *pResData,int TimeOutMs);
extern int CAN_UDS_ClearCacha(cando_handle DevHandle);

#endif 