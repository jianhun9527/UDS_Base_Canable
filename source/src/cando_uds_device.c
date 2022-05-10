/*
 * @Author       : jianhun
 * @Date         : 2021-12-01 21:38:49
 * @LastEditors  : your name
 * @LastEditTime : 2022-05-09 12:16:26
 * @FilePath     : \UDS_Base_CANable\source\src\cando_uds_device.c
 * @Description  : 
 */
#include "cando_uds_device.h"
#include <windows.h>
#include <time.h>

/******************************************************************************
 * UDS功能相关功能定义区
 *****************************************************************************/
/*********************************************************
 * 宏定义区间
 ********************************************************/
#define TP_MTYPE_SF 0U
#define TP_MTYPE_FF 1U
#define TP_MTYPE_CF 2U
#define TP_MTYPE_FC 3U
#define TP_MTYPE_MAX 3U

#define TP_FRM_DLC_MIN 2U
#define TP_FRM_DLC_MAX 8U

#define TP_SF_DLC_MIN 2U
#define TP_FF_DLC_MIN 3U
#define TP_CF_DLC_MIN 2U
#define TP_FC_DLC_MIN 3U

#define TP_SF_DL_MAX 7U
#define TP_FF_DL_MIN 8U
#define TP_FF_DL_EFF 6U
#define TP_FF_DL_MAX 4096U

#define TP_FC_FS_CTS 0U
#define TP_FC_FS_WAIT 1U
#define TP_FC_FS_OVFLW 2U

#define TP_CF_SN_INIT 0U

#define TP_RX_STAT_IDLE 0U
#define TP_RX_STAT_FC_TRANS 1U
#define TP_RX_STAT_FC_CONF 2U
#define TP_RX_STAT_CF_RECV 3U

#define TP_TX_STAT_IDLE 0U
#define TP_TX_STAT_SF_CONF 1U
#define TP_TX_STAT_FF_CONF 2U
#define TP_TX_STAT_FC_RECV 3U
#define TP_TX_STAT_CF_TRANS 4U
#define TP_TX_STAT_CF_CONF 5U

#define TP_ERR_TIMEOUT_A 1U
#define TP_ERR_TIMEOUT_BS 2U
#define TP_ERR_TIMEOUT_CR 3U
#define TP_ERR_WRONG_SN 4U
#define TP_ERR_INVALID_FS 5U
#define TP_ERR_UNEXP_PDU 6U
#define TP_ERR_WFT_OVRN 7U
#define TP_ERR_BUF_OVFLW 8U

#define TP_PHYSICAL_ID 0x73BU
#define TP_FUNCTIONAL_ID 0x7DFU
#define TP_RESPONSE_ID 0x7BBU

#define TP_RX_BUFF_SIZE 256U
#define TP_TX_BUFF_SIZE 256U

#define TP_PDU_PAD_DATA 0xFFU
#define TP_BLOCK_SIZE 0U
#define TP_WFT_MAX 8U

#define TP_TMG_STmin 20U
#define TP_TMG_N_AS 70U
#define TP_TMG_N_AR 70U
#define TP_TMG_N_BS 150U
#define TP_TMG_N_BR 10U /* < 70ms */
#define TP_TMG_N_CS 10U /* < 70ms */
#define TP_TMG_N_CR 150U

/*********************************************************
 * 类型声明
 ********************************************************/
typedef unsigned char tp_bool;
typedef unsigned char tp_uc8;
typedef unsigned short tp_us16;
typedef unsigned int tp_ui16;
typedef unsigned long tp_ul32;

typedef union
{
    tp_uc8 Byte[8];

    union
    {
        struct
        {
            struct
            {
                tp_uc8 SF_DL : 4;
                tp_uc8 Mtype : 4;
            } PCI;
            tp_uc8 Data[7];
        } SF;
        struct
        {
            struct
            {
                tp_us16 FF_DL : 12;
                tp_us16 Mtype : 4;
            } PCI;
            tp_uc8 Data[6];
        } FF;
        struct
        {
            struct
            {
                tp_uc8 SN : 4;
                tp_uc8 Mtype : 4;
            } PCI;
            tp_uc8 Data[7];
        } CF;
        struct
        {
            struct
            {
                tp_uc8 FS : 4;
                tp_uc8 Mtype : 4;
            } PCI;
            tp_uc8 BS;
            tp_uc8 STmin;
            tp_uc8 Data[5];
        } FC;
    } Type;
} TpPDU_T;

/*********************************************************
 * 变量声明
 ********************************************************/
static TpPDU_T tTpTxPdu, tTpRxPdu;
static cando_frame_t tTxFrame, tRxFrame;
static int STmin;

/*********************************************************
 * 函数定义
 ********************************************************/
static int can_sendframe(cando_handle DevHandle, cando_frame_t *frame);
static int can_readframe(cando_handle DevHandle, cando_frame_t *frame, uint32_t timeout_ms);

static int CAN_TP_SFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_uc8 _ucLen);
static int CAN_TP_FFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_us16 _usMsgLen);
static int CAN_TP_CFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_us16 _usMsgLen);
static int CAN_TP_FCTrans(cando_handle DevHandle, tp_us16 _usMsgLen);

static int CAN_TP_SFReceive(unsigned char *pResData, tp_us16 *_usMsgLen);
static int CAN_TP_FFReceive(tp_uc8 *pResData, tp_us16 *_usMsgLen);
static int CAN_TP_CFReceive(tp_uc8 *pResData, tp_us16 *_usMsgLen, tp_us16 *_usCurPos, tp_uc8 *state);
static int CAN_TP_FCReceive(cando_handle DevHandle, tp_ul32 resid, tp_ul32 timeout_ms);

/*********************************************************
 * 函数声明
 ********************************************************/
static int can_sendframe(cando_handle DevHandle, cando_frame_t *frame)
{
    cando_frame_t rece_frame;

    if (!cando_frame_send(DevHandle, frame))
        return CAN_UDS_SEND_FAIL;
    if (!cando_frame_read(DevHandle, &rece_frame, 10))
        return CAN_UDS_SEND_FAIL;
    // if (rece_frame.echo_id == 0xFFFFFFFF || rece_frame.can_id != frame->can_id) return CAN_UDS_SEND_FAIL;

    return CAN_UDS_OK;
}

static int can_readframe(cando_handle DevHandle, cando_frame_t *frame, uint32_t timeout_ms)
{
    uint8_t err_tx_cnt, err_rx_cnt;
    uint32_t err_code;

    if (!cando_frame_read(DevHandle, frame, timeout_ms))
        return CAN_UDS_TIMEOUT_A;
    // if(frame->echo_id != 0xFFFFFFFF) return CAN_UDS_READ_FAIL;
    if (frame->can_id & CANDO_ID_ERR)
    {
        if (!cando_parse_err_frame(frame, &err_code, &err_tx_cnt, &err_rx_cnt))
            return CAN_UDS_READ_ERR_FAIL;
        if (err_code == 0x00000000)
            return CAN_UDS_READ_ERR_FAIL;
        for (int i = 0; i < 10; ++i)
        {
            if ((err_code >> i) % 2 == 1)
                return (CAN_UDS_BUSOFF - i);
        }
    }

    return CAN_UDS_OK;
}

/******************************************************************************
 * Function: CAN_TP_SFTrans
 * Brief : This function is used to send a single frame.
 * Input : _ucpData - Data pointer
 *         _ucLen - Data length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_SFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_uc8 _ucLen)
{
    tp_uc8 ucIdx = _ucLen;

    /* Configure message type. */
    tTpTxPdu.Type.SF.PCI.Mtype = TP_MTYPE_SF;
    /* Configure SF_DL. */
    tTpTxPdu.Type.SF.PCI.SF_DL = _ucLen;
    /* Copy the data to the transmission PDU. */
    memcpy(tTpTxPdu.Type.SF.Data, _ucpData, _ucLen);
    /* Filling unused data. */
    while (ucIdx < 7U)
        tTpTxPdu.Type.SF.Data[ucIdx++] = TP_PDU_PAD_DATA;
    /* Copy the data to the transmission character. */
    memcpy(tTxFrame.data, tTpTxPdu.Byte, 8U);

    return can_sendframe(DevHandle, &tTxFrame);
}

/******************************************************************************
 * Function: CAN_TP_FCTrans
 * Brief : This function is used to send a first frame.
 * Input : _ucpData - Data pointer
 *         _ucLen - Data length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_FFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_us16 _usMsgLen)
{
    tp_uc8 data_tmp;

    /* Configure message type. */
    tTpTxPdu.Type.FF.PCI.Mtype = TP_MTYPE_FF;
    /* Configure FF_DL. */
    tTpTxPdu.Type.FF.PCI.FF_DL = _usMsgLen;
    /* Align and convert between large and small ends */
    data_tmp = tTpTxPdu.Byte[0];
    tTpTxPdu.Byte[0] = tTpTxPdu.Byte[1];
    tTpTxPdu.Byte[1] = data_tmp;
    /* Copy the data to the transmission PDU. */
    memcpy(tTpTxPdu.Type.FF.Data, _ucpData, TP_FF_DL_EFF);
    /* Copy the data to the transmission character. */
    memcpy(tTxFrame.data, tTpTxPdu.Byte, 8);

    return can_sendframe(DevHandle, &tTxFrame);
}

/******************************************************************************
 * Function: CAN_TP_CFTrans
 * Brief : This function is used to send a consecutive frame.
 * Input : _ucpData - Data pointer
 *         _ucLen - Data length
 *         _uc_Sqn - Sequence number
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_CFTrans(cando_handle DevHandle, tp_uc8 *_ucpData, tp_us16 _usMsgLen)
{
    tp_uc8 count, segcnt = 0;
    tp_us16 curpos = 0;
    int err_code;

    /* Configure message type. */
    tTpTxPdu.Type.CF.PCI.Mtype = TP_MTYPE_CF;

    while (1)
    {
        /* Configure SN. */
        tTpTxPdu.Type.CF.PCI.SN = ++segcnt;
        count = _usMsgLen - curpos;
        if (count >= TP_FF_DL_MIN)
        {
            /* Copy the data to the transmission PDU. */
            memcpy(tTpTxPdu.Type.CF.Data, (_ucpData + curpos), 7);
            curpos = curpos + 7;
        }
        else
        {
            /* Copy the data to the transmission PDU. */
            memcpy(tTpTxPdu.Type.CF.Data, (_ucpData + curpos), count);
            curpos = _usMsgLen;
            /* Filling unused data. */
            while (count < 7U)
                tTpTxPdu.Type.CF.Data[count++] = TP_PDU_PAD_DATA;
        }
        /* Copy the data to the transmission character. */
        memcpy(tTxFrame.data, tTpTxPdu.Byte, 8);
        err_code = can_sendframe(DevHandle, &tTxFrame);
        if (err_code != CAN_UDS_OK)
            return err_code;
        if (curpos == _usMsgLen)
            break;
        Sleep(STmin);
    }

    return CAN_UDS_OK;
}

/******************************************************************************
 * Function: CAN_TP_FCTrans
 * Brief : This function is used to send a flow control frame.
 * Input : _usMsgLen - Message length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_FCTrans(cando_handle DevHandle, tp_us16 _usMsgLen)
{
    /* Configure message type. */
    tTpTxPdu.Type.FC.PCI.Mtype = TP_MTYPE_FC;
    /* Configure flow status. */
    tTpTxPdu.Type.FC.PCI.FS = (_usMsgLen > TP_RX_BUFF_SIZE) ? TP_FC_FS_OVFLW : TP_FC_FS_CTS;
    /* Configure block size. */
    tTpTxPdu.Type.FC.BS = TP_BLOCK_SIZE;
    /* Configure stmin. */
    tTpTxPdu.Type.FC.STmin = TP_TMG_STmin;
    /* Unused bytes padding. */
    for (int index = 0; index < 5; index++)
    {
        tTpTxPdu.Type.FC.Data[index] = TP_PDU_PAD_DATA;
    }
    memcpy(tTxFrame.data, tTpTxPdu.Byte, 8);

    /* Flow control frame transmission. */
    return can_sendframe(DevHandle, &tTxFrame);
}

/******************************************************************************
 * Function: CAN_TP_SFReceive
 * Brief : This function is used to receive a single frame.
 * Input :_usId - Frame ID
 *         _ucpData - Data pointer
 *         _ucLen - Data length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_SFReceive(tp_uc8 *pResData, tp_us16 *_usMsgLen)
{
    *_usMsgLen = tTpRxPdu.Type.SF.PCI.SF_DL;
    if (*_usMsgLen > TP_SF_DL_MAX)
        return CAN_UDS_ERROR;
    memcpy(pResData, tTpRxPdu.Type.SF.Data, *_usMsgLen);

    return CAN_UDS_OK;
}

/******************************************************************************
 * Function: CAN_TP_FFReceive
 * Brief : This function is used to receive a first frame.
 * Input :_usId - Frame ID
 *         _ucpData - Data pointer
 *         _usMsgLen - FF_DL 
 *         _ucLen - FF Data length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_FFReceive(tp_uc8 *pResData, tp_us16 *_usMsgLen)
{
    *_usMsgLen = ((tp_us16)(tTpRxPdu.Byte[0] & 0x0f) << 8) + (tp_us16)tTpRxPdu.Byte[1];
    if (*_usMsgLen > TP_RX_BUFF_SIZE)
        return CAN_UDS_BUFFER_OVFLW;
    memcpy(pResData, tTpRxPdu.Type.FF.Data, 6);

    return CAN_UDS_OK;
}

/******************************************************************************
 * Function: CAN_TP_CFReceive
 * Brief : This function is used to receive a consecutive frame.
 * Input :_usId - Frame ID
 *         _ucpData - Data pointer
 *         _usMsgLen - FF_DL 
 *         _ucLen - FF Data length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_CFReceive(tp_uc8 *pResData, tp_us16 *_usMsgLen, tp_us16 *_usCurPos, tp_uc8 *state)
{
    static tp_uc8 bolck_sn = 0;

    if (++bolck_sn != tTpRxPdu.Type.CF.PCI.SN)
        return CAN_UDS_WRONG_SN;
    if (*_usCurPos >= *_usMsgLen)
        return CAN_UDS_UNEXP_PDU;
    if (*_usMsgLen - *_usCurPos > 7U)
    {
        memcpy((pResData + *_usCurPos), tTpRxPdu.Type.CF.Data, 7U);
        *_usCurPos += 7;
        *state = 0;
    }
    else
    {
        memcpy((pResData + *_usCurPos), tTpRxPdu.Type.CF.Data, (*_usMsgLen - *_usCurPos));
        *_usCurPos = *_usMsgLen;
        *state = 1;
    }

    return CAN_UDS_OK;
}

/******************************************************************************
 * Function: CAN_TP_FCTrans
 * Brief : This function is used to send a flow control frame.
 * Input : _usMsgLen - Message length
 * Output : void
 * Warning : none
 ******************************************************************************/
static int CAN_TP_FCReceive(cando_handle DevHandle, tp_ul32 resid, tp_ul32 timeout_ms)
{
    DWORD now_time, last_time;
    tp_ul32 interval;
    int err_code;

    last_time = GetTickCount();
__rev_again:
    err_code = can_readframe(DevHandle, &tRxFrame, timeout_ms);
    now_time = GetTickCount();
    if (err_code != CAN_UDS_OK)
        return err_code;
    if (tRxFrame.can_dlc != 8)
        return CAN_UDS_TP_FORMAT;
    interval = (tp_ul32)(now_time - last_time);
    if (interval > timeout_ms || interval > 1000)
        return CAN_UDS_TIMEOUT_Bs;
    if (tRxFrame.can_id != resid)
        goto __rev_again;

    /* Copy the data to the transmission character. */
    memcpy(tTpRxPdu.Byte, tRxFrame.data, 8);
    if (tTpRxPdu.Type.FC.PCI.Mtype != TP_MTYPE_FC)
        return CAN_UDS_INVALID_FS;
    switch (tTpRxPdu.Type.FC.PCI.FS)
    {
    case TP_FC_FS_CTS:
        STmin = tTpRxPdu.Type.FC.STmin;
        break;
    case TP_FC_FS_WAIT:
        timeout_ms <<= 1;
        goto __rev_again;
        break;
    case TP_FC_FS_OVFLW:
        return CAN_UDS_FS_OVFLW;
    default:
        return CAN_UDS_ERROR;
    }

    return CAN_UDS_OK;
}

/*********************************************************
 * 外部函数声明
 ********************************************************/
int CAN_UDS_Request(cando_handle DevHandle, CAN_UDS_ADDR *pUDSAddr, unsigned char *pReqData, int DataLen)
{
    tTxFrame.can_id = (uint32_t)pUDSAddr->ReqID;
    tTxFrame.can_dlc = 8;

    if (DataLen >= TP_FF_DL_MAX)
        return CAN_UDS_BUFFER_OVFLW;
    if (DataLen <= TP_SF_DL_MAX)
    {
        return CAN_TP_SFTrans(DevHandle, (tp_uc8 *)pReqData, (tp_uc8)DataLen);
    }
    else
    {
        int err_code;

        err_code = CAN_TP_FFTrans(DevHandle, (tp_uc8 *)pReqData, (tp_us16)DataLen);
        if (err_code != CAN_UDS_OK)
            return err_code;
        err_code = CAN_TP_FCReceive(DevHandle, (tp_ul32)pUDSAddr->ResID, TP_TMG_N_BS);
        if (err_code != CAN_UDS_OK)
            return err_code;
        err_code = CAN_TP_CFTrans(DevHandle, (tp_uc8 *)(pReqData + TP_FF_DL_EFF), (tp_us16)(DataLen - TP_FF_DL_EFF));
        if (err_code != CAN_UDS_OK)
            return err_code;
    }

    return CAN_UDS_OK;
}

int CAN_UDS_Response(cando_handle DevHandle, CAN_UDS_ADDR *pUDSAddr, unsigned char *pResData, int TimeOutMs)
{
    uint16_t datalen = 0U, curpos = 0U;
    DWORD now_time, last_time;
    uint8_t rece_state;
    int err_code;

__contuine_rev0:
    last_time = GetTickCount();
__contuine_rev1:
    err_code = can_readframe(DevHandle, &tRxFrame, TimeOutMs);
    now_time = GetTickCount();
    if (err_code != CAN_UDS_OK)
        return err_code;
    if (tRxFrame.can_dlc != 8U)
        return CAN_UDS_TP_FORMAT;
    if ((int)(now_time - last_time) > TimeOutMs)
        return CAN_UDS_TIMEOUT_A;
    if (tRxFrame.can_id != pUDSAddr->ResID)
        goto __contuine_rev1;

    /* Copy the data to the transmission character. */
    memcpy(tTpRxPdu.Byte, tRxFrame.data, 8U);
    switch (tTpRxPdu.Byte[0] >> 4U)
    {
    case TP_MTYPE_SF:
        err_code = (curpos == 0U) ? CAN_TP_SFReceive(pResData, &datalen) : CAN_UDS_ERROR;
        if (err_code != CAN_UDS_OK)
            return err_code;
        break;
    case TP_MTYPE_FF:
        err_code = (curpos == 0U) ? CAN_TP_FFReceive(pResData, &datalen) : CAN_UDS_ERROR;
        if (err_code != CAN_UDS_OK)
            return err_code;
        curpos += 6U;
        err_code = CAN_TP_FCTrans(DevHandle, datalen);
        if (err_code == CAN_UDS_OK)
        {
            goto __contuine_rev0;
        }
        else
        {
            return err_code;
        }
        break;
    case TP_MTYPE_CF:
        err_code = ((curpos >= 6U) && (curpos <= datalen)) ? CAN_TP_CFReceive(pResData, &datalen, &curpos, &rece_state) : CAN_UDS_ERROR;
        if (err_code != CAN_UDS_OK)
            return err_code;
        if (rece_state != 1)
            goto __contuine_rev0;
        break;
    case TP_MTYPE_FC:
        return CAN_UDS_ERROR;
        break;
    default:
        return CAN_UDS_ERROR;
        break;
    }

    return (int)datalen;
}

int CAN_UDS_ClearCacha(cando_handle DevHandle)
{
    cando_frame_t frame;
    
    frame.can_id = 0x7ff;
    frame.can_dlc = 8;
    for (int i = 0; i < 8; i++)
        frame.data[i] = 0;
    cando_frame_send(DevHandle, &frame);
    while (cando_frame_read(DevHandle, &frame, 0));
    
    return 1;
}