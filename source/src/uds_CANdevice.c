/*
 * @Author       : jianhun
 * @Date         : 2022-05-10 20:56:12
 * @LastEditors  : jianhun
 * @LastEditTime : 2022-05-10 22:16:37
 * @FilePath     : \UDS_Base_CANable\source\src\uds_CANdevice.c
 * @Description  : 
 */
/******************************************************************************
 * 头文件区
 *****************************************************************************/
#include "uds_CANdevice.h"
#include "uds_typedef.h"

/******************************************************************************
 * 宏定义区
 *****************************************************************************/
#define CANDO_DEVICE_DEFAULT_NUM 0
#define CANDO_RUN_MODE CANDO_MODE_NORMAL

// baseClk, bitrate, samplePoint, brp, phase_seg1, phase_seg2
// sample point: 50.0%
// << CandoTiming(48000000,    5000, 500, 960, 3, 5)
// << CandoTiming(48000000,   10000, 500, 300, 6, 8)
// << CandoTiming(48000000,   20000, 500, 150, 6, 8)
// << CandoTiming(48000000,   50000, 500,  60, 6, 8)
// << CandoTiming(48000000,   83333, 500,  36, 6, 8)
// << CandoTiming(48000000,  100000, 500,  30, 6, 8)
// << CandoTiming(48000000,  125000, 500,  24, 6, 8)
// << CandoTiming(48000000,  250000, 500,  12, 6, 8)
// << CandoTiming(48000000,  500000, 500,   6, 6, 8)
// << CandoTiming(48000000,  800000, 500,   3, 8, 9)
// << CandoTiming(48000000, 1000000, 500,   3, 6, 8)
// sample point: 62.5%
// << CandoTiming(48000000,    5000, 625, 738, 6, 5)
// << CandoTiming(48000000,   10000, 625, 300, 8, 6)
// << CandoTiming(48000000,   20000, 625, 150, 8, 6)
// << CandoTiming(48000000,   50000, 625,  60, 8, 6)
// << CandoTiming(48000000,   83333, 625,  36, 8, 6)
// << CandoTiming(48000000,  100000, 625,  30, 8, 6)
// << CandoTiming(48000000,  125000, 625,  24, 8, 6)
// << CandoTiming(48000000,  250000, 625,  12, 8, 6)
// << CandoTiming(48000000,  500000, 625,   6, 8, 6)
// << CandoTiming(48000000,  800000, 600,   4, 7, 6)
// << CandoTiming(48000000, 1000000, 625,   3, 8, 6)
// sample point: 75.0%
// << CandoTiming(48000000,    5000, 750, 800, 7, 3)
// << CandoTiming(48000000,   10000, 750, 300, 10, 4)
// << CandoTiming(48000000,   20000, 750, 150, 10, 4)
// << CandoTiming(48000000,   50000, 750,  60, 10, 4)
// << CandoTiming(48000000,   83333, 750,  36, 10, 4)
// << CandoTiming(48000000,  100000, 750,  30, 10, 4)
// << CandoTiming(48000000,  125000, 750,  24, 10, 4)
// << CandoTiming(48000000,  250000, 750,  12, 10, 4)
// << CandoTiming(48000000,  500000, 750,   6, 10, 4)
// << CandoTiming(48000000,  800000, 750,   3, 13, 5)
// << CandoTiming(48000000, 1000000, 750,   3, 10, 4)
// sample point: 87.5%
// << CandoTiming(48000000,    5000, 875, 600, 12, 2)
// << CandoTiming(48000000,   10000, 875, 300, 12, 2)
// << CandoTiming(48000000,   20000, 875, 150, 12, 2)
// << CandoTiming(48000000,   50000, 875,  60, 12, 2)
// << CandoTiming(48000000,   83333, 875,  36, 12, 2)
// << CandoTiming(48000000,  100000, 875,  30, 12, 2)
// << CandoTiming(48000000,  125000, 875,  24, 12, 2)
// << CandoTiming(48000000,  250000, 875,  12, 12, 2)
// << CandoTiming(48000000,  500000, 875,   6, 12, 2)
// << CandoTiming(48000000,  800000, 867,   4, 11, 2)
// << CandoTiming(48000000, 1000000, 875,   3, 12, 2)

/******************************************************************************
 * 函数定义区(外部)
 *****************************************************************************/
int scanDecive(deviceCAN_t *device);
int openDecive(deviceCAN_t *device);
int closeDevice(deviceCAN_t *device);
int showDeviceInfo(deviceCAN_t *device);

/******************************************************************************
 * 函数声明区(外部)
 *****************************************************************************/
int scanDecive(deviceCAN_t *device)
{
    uint8_t devicecnt;

    LOG_DOUT("====================================================================\r\n");
    // 创建Cando设备接受列表句柄
    if (cando_list_malloc(&device->mListHandle)) {
        // 扫描接入的Cando设备
        if (cando_list_scan(device->mListHandle)) {
            // 获取接入的Cando设备数量
            if (cando_list_num(device->mListHandle, &devicecnt)) {
                if (devicecnt > 0) {
                    LOG_INF("Computer connected to Cando device number: %d", devicecnt);
                    device->deviceCnt = devicecnt;
                    return 1;
                } else {
                    LOG_ERR("No Cando device connected!");
                }
            } else {
                LOG_ERR("Failed to read the number of connected Cando devices!");
            }
        } else {
            LOG_ERR("Failed to scan Cando device!");
        }
    } else {
        LOG_ERR("Failed to create Cando list handle!");
    }

    if (device->mListHandle != NULL) while (!cando_list_free(device->mListHandle));

    return 0;
}

int openDecive(deviceCAN_t *device)
{
    if (cando_malloc(device->mListHandle, CANDO_DEVICE_DEFAULT_NUM, &device->mHandle)) {
        cando_close(device->mHandle);
        if (cando_open(device->mHandle)) {
            LOG_INF("Open the Cando device successfully!");
            if (cando_set_timing(device->mHandle, &device->timing)) {
                if (cando_start(device->mHandle, CANDO_RUN_MODE)) {
                    LOG_INF("Start the Cando device successfully!");
                    LOG_DOUT("====================================================================\r\n");
                    return 1;
                } else {
                    LOG_ERR("Failed to start Cando device!");
                    if (cando_close(device->mHandle)) {
                        LOG_INF("Close the Cando device successfully!");
                    } else {
                        LOG_ERR("Failed to close the cando device!");
                    }
                }
            } else {
                cando_close(device->mHandle);
                LOG_ERR("Failed to configure can communication rate!");
            }
        } else {
            LOG_ERR("Failed to open Cando device!");
        }
    } else {
        LOG_ERR("Failed to read the first device handle of the Cando list handle!");
    }

    while (!cando_list_free(device->mListHandle));
    if (device->mHandle != NULL) while (!cando_free(device->mHandle));

    return 0;
}

int closeDevice(deviceCAN_t *device)
{
    LOG_DOUT("====================================================================\r\n");
    if (cando_stop(device->mHandle)) {
        LOG_INF("Stop the Cando device successfully!");
    } else {
        LOG_ERR("Failed to stop the Cando device!");
    }
    if (cando_close(device->mHandle)) {
        LOG_INF("Close the Cando device successfully!");
    } else {
        LOG_ERR("Failed to close the cando device!");
    }
    while (!cando_list_free(device->mListHandle));
    while (!cando_free(device->mHandle));
    LOG_DOUT("====================================================================\r\n");

    return 1;
}

int showDeviceInfo(deviceCAN_t *device)
{
    uint32_t fw_version, hw_version;
    wchar_t *serial_number;

    serial_number = cando_get_serial_number_str(device->mHandle);
    if (cando_get_dev_info(device->mHandle, &fw_version, &hw_version)) {
        LOG_INF("Firmware Info:");
        LOG_INF("Firmware serial number: %S",(serial_number));
        LOG_INF("Firmware Version: v%d.%d", (fw_version / 10), (fw_version % 10));
        LOG_INF("Hardware Version: v%d.%d", (hw_version / 10), (hw_version % 10));
        LOG_DOUT("====================================================================\r\n");

        return 1;
    } else {
        LOG_ERR("Failed to read Cando device information!");
    }

    return 0;
}
