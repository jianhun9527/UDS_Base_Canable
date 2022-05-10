#include "uds_fileResolve.h"
#include "dictionary.h"
#include "iniparser.h"

#define LINE_MAX_LEN 400
#define APP_BUFFER_SIZE (120 * 1024)  // 120K
#define DERIVE_BUFFER_SIZE (8 * 1024) // 8K

static const tU32 crc32_table[256] = CRC32_Table_INIT;
static const dictionary *iniFileDir;
prgFile_t driverFile, appFile;

static RSL_LINERESULT resolveLineS19(char *, tU32 *, tU16 *, tU8 *);
static RSL_RESULT resolveFileS19(const char *, prgFile_t *);
static tU32 convertHexTotU32(char *, tU16, tU16);
static tU32 getFileCRC32(tU8* pBuffer, tU32 size);
static void getFileSegments(FILE *, prgFile_t *);
static void makeSegmentsOrder(prgFile_t *);

static RSL_RESULT resolveFileS19(const char *file_name, prgFile_t *file)
{
    FILE *fileHandle;

    fileHandle = fopen(file_name, "r");
    if (NULL == fileHandle)
    {
        LOG_ERR("Open file failed!");
        return RSL_ERROR;
    }
    getFileSegments(fileHandle, file);
    fclose(fileHandle);

    file->crc32 = getFileCRC32(file->buffer, file->length);
    makeSegmentsOrder(file);

    LOG_DOUT("====================================================================\r\n");
    for (tU8 i = 0; i < file->count; i++)
    {
        LOG_INF("Segment <%d> start address: \t0x%08lX", i, file->segment[i].startAddr);
        LOG_INF("Segment <%d> data length: \t0x%08lX", i, file->segment[i].dataLength);
        LOG_INF("Segment <%d> crc32 result: \t0x%08lX", i, file->segment[i].crc32);
        // LOG_INF("Start position in buffer: \t0x%X",file->segment[i].startAddrInBuff);
        LOG_DOUT("-------------------------------------------------------\r\n");
    }
    LOG_INF("File total data length: \t0x%08lX", file->length);
    LOG_INF("File crc32: \t\t\t0x%08lX", file->crc32);
    LOG_DOUT("====================================================================\r\n");

    return RSL_OK;
}

static tU32 convertHexTotU32(char *hexChar, tU16 begin, tU16 count)
{
    tU32 result = 0;
    tU8 temp;

    for (tU16 i = 0; i < count; i++)
    {
        result <<= 4;
        temp = hexChar[begin + i];

        if (temp >= 48 && temp <= 57)
            result += temp - 48;
        else if (temp >= 65 && temp <= 70)
            result += temp - 55;
        else if (temp >= 97 && temp <= 102)
            result += temp - 87;
    }

    return result;
}

static RSL_LINERESULT resolveLineS19(char *lineRecord, tU32 *recordAddr, tU16 *recordLength, tU8 *recordData)
{
    tU32 i;
    RSL_LINERESULT result;

    if (lineRecord[0] != 'S')
        return INVALID_LINE;

    switch (lineRecord[1])
    {
    case '0':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 3;
        *recordAddr = convertHexTotU32(lineRecord, 4, 4);
        for (i = 0; i < *recordLength; i++)
        {
            recordData[i] = (tU8)convertHexTotU32(lineRecord, 8 + 2 * i, 2);
        }
        result = HEADER_LINE;
        break;
    case '1':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 3;
        *recordAddr = convertHexTotU32(lineRecord, 4, 4);
        for (i = 0; i < *recordLength; i++)
        {
            recordData[i] = (tU8)(convertHexTotU32(lineRecord, 8 + 2 * i, 2));
        }
        result = DATA_LINE;
        break;
    case '2':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 4;
        *recordAddr = convertHexTotU32(lineRecord, 4, 6);
        for (i = 0; i < *recordLength; i++)
        {
            recordData[i] = (tU8)(convertHexTotU32(lineRecord, 10 + 2 * i, 2));
        }
        result = DATA_LINE;
        break;
    case '3':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 5;
        *recordAddr = convertHexTotU32(lineRecord, 4, 8);
        for (i = 0; i < *recordLength; i++)
        {
            recordData[i] = (tU8)(convertHexTotU32(lineRecord, 12 + 2 * i, 2));
        }
        result = DATA_LINE;
        break;
    case '5':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 3;
        *recordAddr = convertHexTotU32(lineRecord, 4, 4);
        result = COUNT_LINE;
        break;
    case '6':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 4;
        *recordAddr = convertHexTotU32(lineRecord, 4, 6);
        result = COUNT_LINE;
        break;
    case '7':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 5;
        *recordAddr = convertHexTotU32(lineRecord, 4, 8);
        result = END_LINE;
        break;
    case '8':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 4;
        *recordAddr = convertHexTotU32(lineRecord, 4, 6);
        result = END_LINE;
        break;
    case '9':
        *recordLength = convertHexTotU32(lineRecord, 2, 2) - 3;
        *recordAddr = convertHexTotU32(lineRecord, 4, 4);
        result = END_LINE;
        break;
    default:
        result = INVALID_LINE;
        break;
    }
    return result;
}

static void getFileSegments(FILE *fHandle, prgFile_t *file)
{
    char line_buffer[LINE_MAX_LEN]; //Used to record the ASCII code of a line in the S19 file
    tU8 prog_buffer[LINE_MAX_LEN];  //Used to record the programming byte array contained in the ASCII code of one line in the S19 file
    tU32 temp_address = 0;          //The programming start address used to record the single-line programming data of the S19 file
    tU16 temp_length = 0;           //Used to record the programming byte length of the single-line programming data of the S19 file
    tU32 except_address = 0;

    file->count = 0;
    file->length = 0;
    do
    {
        if (NULL == fgets(line_buffer, LINE_MAX_LEN, fHandle))
            break;
        if (DATA_LINE == resolveLineS19(line_buffer, &temp_address, &temp_length, prog_buffer))
        {
            if ((0 == file->length) || (except_address != temp_address))
            {
                file->segment[file->count].startAddrInBuff = file->length;
                file->segment[file->count].startAddr = temp_address;
                file->segment[file->count].dataLength = 0;
                except_address = temp_address;
                file->count++;
            }

            memcpy(file->buffer + file->length, prog_buffer, temp_length);
            file->length += temp_length;
            file->segment[file->count - 1].dataLength += temp_length;
            except_address += temp_length;
        }
    } while ('S' == line_buffer[0]);

    for (int i = 0; i < file->count; i++)
    {
        file->segment[i].crc32 = 
            getFileCRC32(file->buffer + file->segment[i].startAddrInBuff, 
                         file->segment[i].dataLength);
    }
}

static tU32 getFileCRC32(tU8* pBuffer, tU32 size)
{
    tU32 crcRes;

    crcRes = 0xFFFFFFFF;
    for (tU32 i = 0; i < size; i++)
    {
        crcRes = (crcRes >> 8) ^ crc32_table[(crcRes & 0xFF) ^ pBuffer[i]];
    }
    crcRes = ~crcRes;

    return crcRes;
}

static void makeSegmentsOrder(prgFile_t *file)
{
    for (tU8 i = 0; i < file->count; i++)
    {
        for (tU8 j = i + 1; j < file->count; j++)
        {
            if (file->segment[i].startAddr > file->segment[j].startAddr)
            {
                segInfo_t temp = file->segment[i];
                file->segment[i] = file->segment[j];
                file->segment[j] = temp;
            }
        }
    }
}

int readConfigFile(const char *ininame)
{
    iniFileDir = iniparser_load(ininame);
    if (iniFileDir != NULL)
    {
        return 1;
    }
    LOG_ERR("Cannot parse file: %s", ininame);

    return 0;
}

int readDriverFile(const char *drivername)
{
    const char *driverFileName;
    driverFileName = iniparser_getstring(iniFileDir, drivername, NULL);
    if (NULL != driverFileName)
    {
        LOG_INF("[FILE] %s", driverFileName);
        driverFile.buffer = malloc(sizeof(tU8) * DERIVE_BUFFER_SIZE);
        if (resolveFileS19(driverFileName, &driverFile) != RSL_OK)
            return 0;
        return 1;
    }
    else
    {
        LOG_ERR("Undefine driver file path!");
        return 0;
    }
}

int readAppFile(const char *appname)
{
    const char *appFileName;

    appFileName = iniparser_getstring(iniFileDir, appname, NULL);
    if (NULL != appFileName)
    {
        LOG_INF("[FILE] %s", appFileName);
        appFile.buffer = malloc(sizeof(tU8) * APP_BUFFER_SIZE);
        if (resolveFileS19(appFileName, &appFile) != RSL_OK)
            return 0;
        return 1;
    }
    else
    {
        LOG_ERR("Undefine app file path!");
        free(driverFile.buffer);
        return 0;
    }
}

int exitFileReading(void)
{
    if (appFile.buffer != NULL)
        free(appFile.buffer);
    if (driverFile.buffer != NULL)
        free(driverFile.buffer);

    return 1;
}