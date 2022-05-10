/*
 * @Author       : jianhun
 * @Date         : 2021-11-22 21:34:46
 * @LastEditors  : jianhun
 * @LastEditTime : 2022-05-10 22:15:51
 * @FilePath     : \UDS_Base_CANable\source\main.c
 * @Description  : 
 */
#include "uds_fileProgram.h"

#define CONFIG_NAME     "Config.ini"
#define DERIVE_INDEX    "FILE:DRIVER_PATH"
#define APP_INDEX       "FILE:APP_PATH"

static const char SoftwareVer[] = {"SL_IHR_CANBooterload_V0.5"};
static deviceCAN_t hsCando = {NULL, NULL, CANDO_TIMING_500K, 0};

int main(int argc, const char* argv[])
{
    LOG_INF("Software version: [%s]",SoftwareVer);
    
    if(!scanDecive(&hsCando)) goto __systemEnd;
    if(!openDecive(&hsCando)) goto __systemEnd;
    if (!showDeviceInfo(&hsCando)) goto __CandoExit;
    
    if(!readConfigFile(CONFIG_NAME)) goto __CandoExit;
    if(!readDriverFile(DERIVE_INDEX)) goto __CandoFileExit;
    if(!readAppFile(APP_INDEX)) goto __CandoFileExit;
    
    // printf("[log C] Start APP download (Y/N): ");
    // char input = getche();
    // printf("\r\n");
    // if (input == 'Y' || input == 'y')
    {
        CAN_UDS_ClearCacha(hsCando.mHandle);
        startCommunicate(&hsCando);
        WaitForSingleObject(tComEndSem, INFINITE);
    }
    // else
    // {
    //     LOG_INF("Cancel APP download!");
    // }

__CandoFileExit:
    exitFileReading();
    
__CandoExit:
    closeDevice(&hsCando);
    
__systemEnd:
    system("pause");

    return 0;
}
