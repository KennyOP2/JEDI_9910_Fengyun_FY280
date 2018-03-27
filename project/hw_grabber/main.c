#include "mmp_types.h"
#include "pal/thread.h"
#include "mmp_i2s.h"
#include "mmp_usbex.h"
#include "core_interface.h"
#include "pal/pal.h"
#include "hdmi_loop_through.h"
#include "mmp_capture.h"
#include "gpio_key_led_map.h"
#include "mmp_nor.h"
#include "mmp_timer.h"
#include "msg_route.h"
#include "config.h"
#include "grabber_control.h"
#include "mmp_isp.h"
#include "storage_mgr.h"

//#define INPUT_SWITCH_YPBPR_CVBS  //kenny geniatech
#define Geniatech_RTC

#define Change_Resolution

//#define CUSTOMER_YONGKE_FUNCTION
//#define REGIA_FILE_NAME

//#define YK_MINI_GRABBER   //one Key,one Led
#ifdef YK_MINI_GRABBER
    #undef INPUT_SWITCH_YPBPR_CVBS
#endif

//#define MPEG_AUDIO_FOR_FY
//disable #define ENABLE_AAC mencoder.c

#define SPLITER_FILE

//=============================================================================
//                              Constant Definition
//=============================================================================
#ifdef IT9913_128LQFP
    #define IIC_DELAY            0
#else
    #define IIC_DELAY            100
#endif

#define USB0_ENABLE              (0x1 << 0)
#define USB1_ENABLE              (0x1 << 1)
#define USB_ENABLE_MODE          (USB0_ENABLE | USB1_ENABLE)
#define USB_MIN_FREESPACE_MB     60                 // MB

#define LED_TIMER_NUM            5
#define LED_TIMER_TIMEOUT        100000             // us

#define WAIT_USB_WRITE_DONE_TIME 3 * 1000           //ms
#define CONFIG_SIGNATURE         (('I' << 24) | ('T' << 16) | ('E' << 8))

#define DEF_MICVOLGAINDB         10   // kenny0518 17
#define DEF_MICBOOSTGAIN         1
#define DEF_HDMIBOOSTGAIN        7       //5 kenny0910     //0db
#define DEF_ANALOGBOOSTGAIN      5            //0db

typedef enum SYSTEM_STATUS_TAG
{
    SYSTEM_USB_UNPLUG_ERROR       = (1 << 0),
    SYSTEM_USB_NO_FREESPACE_ERROR = (1 << 1),
    SYSTEM_USB_MOUNT_ERROR        = (1 << 2),
    SYSTEM_USB_NO_STORAGE_ERROR   = (1 << 3),
    SYSTEM_UNSTABLE_ERROR         = (1 << 4),
    SYSTEM_HDCP_PROTECTED         = (1 << 5),
    SYSTEM_IN_RECORD_STATUS       = (1 << 6),
    SYSTEM_IN_UPGRADE_STATUS      = (1 << 7),
    SYSTEM_USB_BUSY_STATUS        = (1 << 8),
    SYSTEM_OUT_RECORD_STATUS      = (1 << 9),
    SYSTEM_PC_CONNECTED_MODE      = (1 << 10),
    STSTEM_STAND_ALONE_MODE       = (1 << 11),
    SYSTEM_FULL_HD_RECORD         = (1 << 12),
    SYSTEM_DIGITAL_INPUT          = (1 << 13),
    SYSTEM_SWITCH_DIGITAL_ANALOG  = (1 << 14),
    SYSTEM_STARTUP_LED            = (1 << 15),
    SYSTEM_YPBPR_INPUT            = (1 << 16),
    SYSTEM_CVBS_INPUT             = (1 << 17),
} SYSTEM_STATUS;

typedef enum ENCODE_RESOLUTION_TAG
{
    ENCODE_RESOLUTION_720P,
    ENCODE_RESOLUTION_1080P,
    ENCODE_RESOLUTION_UNKNOWN
} ENCODE_RESOLUTION;

typedef enum INPUT_DEVICE_TYPE_TAG
{
    INPUT_DEV_UNKNOWN,
    INPUT_DEV_ANALOG,
    INPUT_DEV_DIGITAL
} INPUT_DEVICE_TYPE;

//=============================================================================
//                              Structure Definition
//=============================================================================
typedef struct AUDIO_CODEC_CONTROL_TAG
{
    MMP_BOOL   bPCConnectedMode;
    MMP_BOOL   bHDMIInput;
    MMP_BOOL   bDVIMode;
    MMP_BOOL   bEnRecord;
    MMP_BOOL   bInsertMic;

    MMP_UINT32 hdmiBoostGain;      //000 = Mute 001 = -12dB ...3dB steps up to 111 = +6dB
    MMP_UINT32 analogBoostGain;    //000 = Mute 001 = -12dB ...3dB steps up to 111 = +6dB
    MMP_INT32  micVolume;
    MMP_UINT32 micBoostGain;       //0:+0db, 1:+13db, 2:+20db, 3:+29db
} AUDIO_CODEC_CONTROL;

typedef struct PCMODE_LED_CTRL_TAG
{
    LED_STATUS BlueLed;
    LED_STATUS GreenLed;
    LED_STATUS RedLed;
    LED_STATUS BlingRingLed;
} PCMODE_LED_CTRL;

//=============================================================================
//                              Extern Reference
//=============================================================================

//=============================================================================
//                              Global Data Definition
//=============================================================================
#ifdef HAVE_FAT
static PAL_THREAD thread_Usb        = MMP_NULL;
    #ifdef ENABLE_USB_DEVICE
static PAL_THREAD thread_Usb_device = MMP_NULL;
    #endif
#endif

static PAL_THREAD                         thread_HDMI_Loop_Through = MMP_NULL;
static PAL_THREAD                         thread_KEY_MANAGER       = MMP_NULL;

/*static kenny patch 20140428 */ MMP_BOOL gbEnableRecord           = MMP_FALSE;
static MMP_BOOL                           gbRecording              = MMP_FALSE;

static SYSTEM_STATUS                      gtSystemStatus           = SYSTEM_UNSTABLE_ERROR;
static MMP_BOOL                           gbSrcChangeResolution    = MMP_FALSE;

MMP_UINT                                  gYear                    = 0, gMonth = 0, gDay = 0;
MMP_UINT8                                 gHour                    = 0, gMin = 0, gSec = 0;

static MMP_UINT32                         gTimerIsrCount           = 0;
MMP_BOOL                                  gbDeviceMode             = MMP_FALSE;
static MMP_BOOL                           gbChangeDevice           = MMP_FALSE;
static AUDIO_CODEC_CONTROL                gtAudioCodec             = {0};
static GPIO_LED_CONTROL                   gtLedCtrl = {0};
//static INPUT_ASPECT_RATIO   gtAspectRatioCtrl = INPUT_ASPECT_RATIO_AUTO;
static PCMODE_LED_CTRL                    gtPCModeLed;
MMP_BOOL                                  gbPC_MODE_ENALBE_RECORD = MMP_FALSE;

//kenny geniatech
static MMP_BOOL                           gbLockS3Key             = MMP_FALSE;
//kenny patch 20140428
#ifdef YK_MINI_GRABBER
MMP_INT                                   flaginput               = 0;
#endif
//#ifdef CUSTOMER_YONGKE_FUNCTION
MMP_BOOL                                  ykproject               = MMP_FALSE;
//#endif
//=============================================================================
//                              Private Function Declaration
//=============================================================================
static void
_FirmwareUpgrade(
    void);

static void *
_KeyManagerFunction(
    void *data);

static void
_UsbManagerFunction(
    void);

static void
_Led_SetEncodeResolution(
    ENCODE_RESOLUTION res);

static MMP_BOOL
_CheckUsbFreeSpace(
    MMP_UINT32 *remainspace);

static void
_ProcessMsg(
    MSG_OBJECT *ptMsg);

static void
_LedTimerIsr(
    void *data);

static void
_LedTimerInit(
    void);

static void
_LedTimerTerminate(
    void);

static MMP_BOOL
_IsContentProtection(
    void);

static void
_SetRtcFromFile(
    void);

static void
_AudioCodecControl(
    AUDIO_CODEC_CONTROL *pCodec);

static void
_AutoChangeDevice(
    void);

static void
_GetNextRecordFileName(
    char       *filename,
    MMP_UINT32 rec_resolution);

//=============================================================================
//                              Public Function Definition
//=============================================================================
static MMP_INT
Initialize(
    void)
{
    MMP_INT              result           = 0;
    TS_MUXER_PARAMETER   tMuxerParam      = { 0 };
    MMP_UINT             year             = 0, month = 0, day = 0;
    MMP_UINT32           micCurrVol;
    MMP_UINT8            pServiceName[32] = { 0 };
    CAPTURE_VIDEO_SOURCE capdev;
    MMP_UINT32           lineboost;
    MMP_BOOL             flag;

    _coreInitVideoEnPara = projectInitVideoEnPara;
    _coreSetVideoEnPara  = projectSetVideoEnPara;
    _coreGetVideoEnPara  = projectGetVideoEnPara;

    // chip warm up
    if (1)
    {
        volatile MMP_UINT16 value;
        MMP_UINT32          cnt  = 10;
        MMP_UINT32          cnt1 = 0;
        MMP_UINT32          i;

        // fire ISP
        HOST_WriteRegister(0x0500, 0x0019);

        for (i = 0; i < 55000 * 80; i++)
            asm ("");

        while (cnt-- != 0)
        {
            HOST_ReadRegister(0x34a, &value);
            dbg_msg(DBG_MSG_TYPE_INFO, "Rd %x\n", value);
            if ((value & 0xF) == 0xE)
                cnt1++;
        }

        // stop capture & isp
        HOST_WriteRegister(0x2018, 0x0000);
        HOST_WriteRegister(0x060a, 0x0000);

        // wait isp idle
        HOST_ReadRegister(0x6fc, &value);

        while ((value & 0x1) != 0)
        {
            HOST_ReadRegister(0x6fc, &value);
        }

        // wait capture idle
        HOST_ReadRegister(0x1f22, &value);

        while ((value & 0x80c1) != 0x80c1)
        {
            HOST_ReadRegister(0x1f22, &value);
        }

        // read dram status
        HOST_ReadRegister(0x34a, &value);

        if (((value & 0xF) == 0xC && cnt1 == 0) || (value & 0xF) == 0x8)
        {
            HOST_WriteRegister(0x340, 0x2a54);
            for (i = 0; i < 30000; i++)
                asm ("");
            dbg_msg(DBG_MSG_TYPE_INFO, "Update 0x340\n");
        }

        HOST_ReadRegister(0x3a6, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x3a6) %x\n", value);

        HOST_ReadRegister(0x340, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x340) %x\n", value);

        HOST_ReadRegister(0x342, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x342) %x\n", value);

        HOST_ReadRegister(0x344, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x344) %x\n", value);

        HOST_ReadRegister(0x346, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x346) %x\n", value);

        HOST_ReadRegister(0x348, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x348) %x\n", value);

        HOST_ReadRegister(0x34a, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x34a) %x\n", value);

        HOST_ReadRegister(0x34e, &value);
        dbg_msg(DBG_MSG_TYPE_INFO, "Mem Addr (0x34e) %x\n", value);
    }
    // enable in script, reset
    mmpWatchDogDisable();
    // Set the watch dog timer to 5 second to ensure later copy process can be done in time.
    mmpWatchDogEnable(5);
    ithIntrInit();
    _LedTimerInit();

    printf("version: %d.%d.%d.%d.%d\n", CUSTOMER_CODE, PROJECT_CODE, SDK_MAJOR_VERSION, SDK_MINOR_VERSION, BUILD_NUMBER);
    // wait for creating tasks, may reduce or remove later
    PalSleep(100);
//kenny patch 20140428
#ifdef CUSTOMER_YONGKE_FUNCTION
    mmpIrInitialize(0);
#endif
    GpioLEDInitialize();

    gtSystemStatus |= STSTEM_STAND_ALONE_MODE;

    RegisterProcessMsgRoutine(_ProcessMsg);
    InitDelayedMessages();

    //ithIntrInit(); // move ahead
    mmpDmaInitialize();

    result = mmpIicInitialize(0, 0, 0, 0, 0, IIC_DELAY, MMP_TRUE);
    if (result)
    {
        PalAssert("IIC init error\n");
        goto end;
    }

    result = mmpIicSetClockRate(200 * 1000);
    if (result)
        dbg_msg(DBG_MSG_TYPE_ERROR, "iic clock %d !\n", result);

    load_default_encodepara();
    config_load();
    serial_number_load();
/*
    {
        MMP_UINT8 serialnum[128]= {0};
        MMP_RESULT result;
        coreGetUsbSerialNumber((MMP_CHAR*)&serialnum);
        dbg_msg(DBG_MSG_TYPE_INFO, "++++ serial num = %s +++++\n", serialnum);
        if (serialnum[0] == MMP_NULL)
        {
            dbg_msg(DBG_MSG_TYPE_INFO, " ++ Write usb serial number for test ++\n");
            {
                MMP_UINT32 serialnumaddr;
                MMP_UINT32 capacity;
                MMP_UINT8  temp[] = "1369021";
                USB_SERIAL_NUMBER usbserialnum ={0};
                usbserialnum.signature = CONFIG_SIGNATURE;
                usbserialnum.size = sizeof(usbserialnum);
                strcpy(usbserialnum.serial_number, temp);
                dbg_msg(DBG_MSG_TYPE_INFO, "string usb serial num = %s\n", usbserialnum.serial_number);
                capacity = NorCapacity();
                serialnumaddr = capacity - (128*1024)-(64*1024);
                result = NorWrite(&usbserialnum, serialnumaddr, sizeof(usbserialnum));
                NorRead(&usbserialnum, serialnumaddr, sizeof(usbserialnum));
                dbg_msg(DBG_MSG_TYPE_INFO, " -- Write usb serial number for test --\n");
            }
        }
    }
 */
#ifdef HAVE_FAT
    // USB/Device initial
    result = mmpUsbExInitialize(USB_ENABLE_MODE);
    if (result)
    {
        PalAssert(!"INIT USBEX FAIL");
        goto end;
    }

    // Create task for USB host/device driver
    /** This is HC driver task, and it will never be destroyed. */
    thread_Usb = PalCreateThread(PAL_THREAD_USBEX,
                                 USBEX_ThreadFunc,
                                 MMP_NULL,
                                 2000,
                                 PAL_THREAD_PRIORITY_NORMAL);
    if (!thread_Usb)
        PalAssert(!" Create USB host task fail~~");

    #ifdef ENABLE_USB_DEVICE
    thread_Usb_device = PalCreateThread(PAL_THREAD_USB_DEVICE,
                                        DEVICE_ThreadFunc,
                                        MMP_NULL,
                                        2000,
                                        PAL_THREAD_PRIORITY_NORMAL);
    if (!thread_Usb_device)
        PalAssert(!" Create USB device task fail~~");
    #endif

    // register usb function drivers to USB driver
    mmpMscDriverRegister();
    PalSleep(1);

    result = storageMgrInitialize(MMP_TRUE);
    if (result)
    {
        dbg_msg(DBG_MSG_TYPE_ERROR, "storageMgrInitialize() fail !! %s [#%d]\n", __FILE__, __LINE__);
    }
    PalSleep(0);

    result = PalFileInitialize();
    if (result)
    {
        dbg_msg(DBG_MSG_TYPE_ERROR, "PalFileInitialize() fail !! %s [#%d]\n", __FILE__, __LINE__);
        goto end;
    }
    //_LedTimerInit(); // move ahead
    PalSleep(3000);
    _FirmwareUpgrade();
#endif

#ifdef EXTERNAL_RTC
    mmpExRtcGetDate(&year, &month, &day);
    if (year != 2013)
    {
        dbg_msg(DBG_MSG_TYPE_INFO, "Setup External RTC\n");
        mmpExRtcSetDateTime(2013, 5, 30, 0, 0, 0);
    }
#endif
    mmpCapSetDeviceReboot(MMP_TRUE);
    coreInitialize(MMP_MP4_MUX);
    coreInitVideoEnPara();
    config_get_capturedev(&capdev);
    coreSetCaptureSource(capdev);
    coreEnableISPOnFly(MMP_FALSE);
    coreEnableAVEngine(MMP_FALSE);
    //coreEnableISPOnFly(MMP_TRUE);

    //kenny geniatech
#ifndef INPUT_SWITCH_YPBPR_CVBS
    if ((capdev == CAPTURE_VIDEO_SOURCE_HDMI) ||
        (capdev == CAPTURE_VIDEO_SOURCE_DVI))
        gtSystemStatus |= SYSTEM_DIGITAL_INPUT;
    else
        gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
#else
    if ((capdev == CAPTURE_VIDEO_SOURCE_HDMI) ||
        (capdev == CAPTURE_VIDEO_SOURCE_DVI))
    {
        gtSystemStatus |= SYSTEM_DIGITAL_INPUT;
        gtSystemStatus &= (~SYSTEM_YPBPR_INPUT);
        gtSystemStatus &= (~SYSTEM_CVBS_INPUT);
    }
    else if (capdev == CAPTURE_VIDEO_SOURCE_YPBPR)
    {
        gtSystemStatus &= (~SYSTEM_CVBS_INPUT);
        gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
        gtSystemStatus |= SYSTEM_YPBPR_INPUT;
    }
    else if (capdev == CAPTURE_VIDEO_SOURCE_CVBS)
    {
        gtSystemStatus &= (~SYSTEM_YPBPR_INPUT);
        gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
        gtSystemStatus |= SYSTEM_CVBS_INPUT;
    }
    dbg_msg(DBG_MSG_TYPE_INFO, "gtSystemStatus =%x,capdev=%d\r\n", gtSystemStatus, capdev);
#endif

#ifdef TSO_ENABLE
    tMuxerParam.bEnableTso    = MMP_FALSE;
    tMuxerParam.bEnableEagle  = MMP_FALSE;
    tMuxerParam.constellation = CONSTELATTION_64QAM;
    tMuxerParam.codeRate      = CODE_RATE_7_8;
    tMuxerParam.guardInterval = GUARD_INTERVAL_1_32;
    tMuxerParam.frequency     = 887000;
    tMuxerParam.bandwidth     = 6000;
#endif
    sprintf(pServiceName, "AIR_CH_%d_%dM", tMuxerParam.frequency / 1000, tMuxerParam.bandwidth / 1000);
#ifdef AV_SENDER_SECURITY_MODE
    tMuxerParam.bEnableSecuirtyMode = MMP_TRUE;
#else
    tMuxerParam.bEnableSecuirtyMode = MMP_FALSE;
#endif
    tMuxerParam.bAddStuffData       = MMP_FALSE;
#ifdef MPEG_AUDIO_FOR_FY
    tMuxerParam.audioEncoderType    = MPEG_AUDIO_ENCODER;
#else
    tMuxerParam.audioEncoderType    = AAC_AUDIO_ENCODER;
#endif
// Step 1: Removed all saved services
    coreTsRemoveServices();
    // Step 2: Specifiy the country code for NIT default setting (network_id, original_network_id, private_data_specifier_descriptor, LCN rule).
    coreTsUpdateCountryId(CORE_COUNTRY_TAIWAN);
    // Step 3: Update the modulation parameter for NIT.
    coreTsUpdateModulationParameter(tMuxerParam.frequency, tMuxerParam.bandwidth,
                                    tMuxerParam.constellation, tMuxerParam.codeRate, tMuxerParam.guardInterval);
    // Step 4: Insert desried service parameter for PMT, SDT, and NIT
    coreTsInsertService(0x100, 0x1000,
                        0x1011, H264_VIDEO_STREAM,
#ifdef MPEG_AUDIO_FOR_FY
                        0x1100, MPEG_AUDIO,
#else
                        0x1100, AAC,
#endif
                        "ITE", sizeof("ITE"),
                        pServiceName, strlen(pServiceName));
    // Step 5: Notify the table setup is done, then generate the SI/PSI table including PAT, PMT, SDT, and NIT
    coreTsUpdateTable();
    coreSetMuxerParameter((void *) &tMuxerParam);

    config_get_microphone_volume(&micCurrVol);

    //micCurrVol = 16; //30dB

    codec_initialize();
    config_get_line_boost(&lineboost);
    gtAudioCodec.bPCConnectedMode = MMP_FALSE;
    gtAudioCodec.bHDMIInput       = ((coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI) || (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI)) ? MMP_TRUE : MMP_FALSE;
    gtAudioCodec.bDVIMode         = (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI) ? MMP_TRUE : MMP_FALSE;
    gtAudioCodec.bEnRecord        = MMP_FALSE;
    gtAudioCodec.bInsertMic       = MMP_FALSE;
    gtAudioCodec.hdmiBoostGain    = DEF_HDMIBOOSTGAIN;
    gtAudioCodec.analogBoostGain  = DEF_ANALOGBOOSTGAIN;
    gtAudioCodec.micVolume        = DEF_MICVOLGAINDB;
    gtAudioCodec.micBoostGain     = DEF_MICBOOSTGAIN;

    _AudioCodecControl(&gtAudioCodec);

    corePlay();

#ifdef HDMI_LOOPTHROUGH
    thread_HDMI_Loop_Through = PalCreateThread(PAL_THREAD_HDMILooPThrough,
                                               HDMILoopThrough_ThreadFun,
                                               MMP_NULL,
                                               2000,
                                               PAL_THREAD_PRIORITY_NORMAL);

    HDMILoopThrough_CreateThread();
    if (!thread_HDMI_Loop_Through)
        PalAssert(!" Create HDMI Loop Through task fail~~");
#endif

    thread_KEY_MANAGER = PalCreateThread(PAL_THREAD_KEY_MANAGER,
                                         _KeyManagerFunction,
                                         MMP_NULL,
                                         2000,
                                         PAL_THREAD_PRIORITY_NORMAL);
    if (!thread_KEY_MANAGER)
        PalAssert(!" Create KEY MANAGER task fail~~");

    GpioKeyInitialize();
    GpioMicInitialize();
    //mmpWatchDogEnable(30); // move ahead

//kenny patch 20140428

#ifdef EXTERNAL_RTC
    _SetRtcFromFile();
#else
    gYear  = 2014;
    gMonth = 1;
    gDay   = 1;
    gHour  = 0;
    gMin   = 0;
    gSec   = 0;
    #ifdef Geniatech_RTC
    _SetRtcFromFile();
    #endif
    if (KEY_CODE_S1 == GpioKeyGetKey())
    {
        config_set_rec_index(0);
        printf("index set default\n");
    }
#endif

    //kenny geniatech
    gtSystemStatus |= SYSTEM_STARTUP_LED;
    gtSystemStatus |= SYSTEM_FULL_HD_RECORD;  //kenny 20140103  set default, full HD input funll HD record
//kenny patch 20140428
#ifdef YK_MINI_GRABBER
    gtSystemStatus &= (~SYSTEM_UNSTABLE_ERROR);
#endif
#ifdef CUSTOMER_YONGKE_FUNCTION
    ykproject       = MMP_TRUE;
#endif

#if 0 //def MPEG_AUDIO_FOR_FY
    {
        CORE_AUDIO_ENCODE_PARA aencoder_param;
        aencoder_param.audioEncoderType = AAC_AUDIO_ENCODER;
        aencoder_param.bitRate          = 192000;
        coreSetAudioEncodeParameter(&aencoder_param);
    }
#endif

    // turn off DPU clock
    HOST_WriteRegisterMask(0x16, (0x0 << 5), (0x1 << 5));

    //Hardware Trap
    AHB_WriteRegisterMask((GPIO_BASE + 0xD4), 0x00000, 0x00ff0);

#if 0
    vTaskGetRunTimeStats((char *)0);
    vTaskGetRunCodeStackStats((char *)0);
    malloc_stats();
#endif

end:
    return result;
}

static MMP_INT
Terminate(
    void)
{
    MMP_INT result = 0;

    codec_terminate();
    GpioLEDTerminate();
    GpioKeyTerminate();
    _LedTimerTerminate();
    coreTerminate();
    TerminateDelayedMessages();

    return result;
}

static MMP_INT
MainLoop(
    void)
{
    MMP_INT           result             = 0;
    MMP_UINT32        CheckRecordClock   = PalGetClock();
    MMP_UINT32        CheckDeviceClock   = PalGetClock();
    //MMP_UINT32 clockLED = PalGetClock();
    MMP_UINT32        videoUnstableClock = 0;
    MMP_UINT32        CheckRtcClock      = PalGetClock();
    //MMP_UINT32 recFileIndex = 0;
    MMP_UINT32        remainspace        = 0;
    MMP_UINT8         filename[512];
    //MMP_BOOL   keyswitch = MMP_FALSE;
    PAL_FILE          *fp;
    //MMP_BOOL Trigger = MMP_FALSE;
    //static MMP_BOOL prevTrigger = MMP_FALSE;
    MMP_UINT32        durationTime  = 0;
    MMP_UINT32        adjustTime    = 0;
#ifndef EXTERNAL_RTC
    MMP_UINT32        recFileIndex  = 0;
    static MMP_UINT32 recresolution = 0; //kenny patch resolution flag
#endif
//kenny patch resolution flog
#ifdef Change_Resolution
    static MMP_INT    preres   = 0xFF;
    MMP_INT           resinput = 0xFF;
    extern MMP_UINT16 gtHDMIResolution;
    extern MMP_UINT16 gtYPBPRResolution;
    extern MMP_UINT16 gtCVBSResolution;
#endif
    for (;;)
    {
        PalSleep(33);

        SendDelayedMessages();

        _AutoChangeDevice();
        _UsbManagerFunction();
        //it_ts_file_UpgradeFirmware(); //PC config , upgrade Firmware
        mmpWatchDogRefreshTimer();

#if 1
        if ((durationTime = PalGetDuration(CheckRtcClock)) >= 1000)
        {
         //   printf("reg 59=%x\r\n",HDMITX_ReadI2C_Byte(0x59));
            adjustTime += (durationTime - 1000);
            if (adjustTime >= 1000)
            {
                gSec++;
                adjustTime -= 1000;
            }
            gSec++;
            if (gSec >= 60)
            {
                gSec -= 60;
                gMin++;
            }

            if (gMin == 60)
            {
                gMin -= 60;
                gHour++;
            }

            if (gHour == 24)
            {
                gHour -= 24;
                gDay++;
            }

            switch (gMonth)
            {
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 10:
            case 12:
                if (gDay > 31)
                {
                    gMonth++;
                    gDay -= 31;
                }
                break;

            case 4:
            case 6:
            case 9:
            case 11:
                if (gDay > 30)
                {
                    gMonth++;
                    gDay -= 30;
                }
                break;

            case 2:
                if (gYear % 4 == 0)
                {
                    if (gDay > 29)
                    {
                        gDay -= 29;
                        gMonth++;
                    }
                }
                else
                {
                    if (gDay > 28)
                    {
                        gDay -= 28;
                        gMonth++;
                    }
                }
                break;
            }
            if (gMonth > 13)
            {
                gMonth -= 12;
                gYear++;
            }
            CheckRtcClock = PalGetClock();
        }

        if (PalGetDuration(CheckRecordClock) > 300)
        {
            MMP_WCHAR       wfilename[255];
            static MMP_BOOL preEnableRecord = MMP_FALSE;

            if (gbEnableRecord && !preEnableRecord)
            {
                VIDEO_ENCODER_INPUT_INFO   srcIdx = coreGetInputSrcInfo();
                CAPTURE_VIDEO_SOURCE       capdev = coreGetCaptureSource();
                VIDEO_ENCODER_UPDATE_FLAGS flags  = 0;
                VIDEO_ENCODER_PARAMETER    tEnPara;

                //update Default
                flags         = VIDEO_ENCODER_FLAGS_UPDATE_DEFAULT;
                coreSetVideoEnPara(capdev, flags, srcIdx, &tEnPara);
                recresolution = 0;
                printf("record srcIdx =%d\r\n", srcIdx);
                if (RECORD_TABLE[srcIdx].isFullHDRes && srcIdx <= MMP_CAP_INPUT_INFO_ALL /*kenny patch add 20140415*/)
                {
                    coreGetVideoEnPara(capdev, srcIdx, &tEnPara);
                    recresolution = 1;
                    if ((gtSystemStatus & SYSTEM_FULL_HD_RECORD) == 0) //720p case
                    {
                        recresolution         = 2;
                        printf("Full HD record 720p \r\n");
                        tEnPara.EnWidth       = 1280;
                        tEnPara.EnHeight      = 720;
                        tEnPara.EnFrameRate   = tEnPara.EnFrameRate * 2;
                        tEnPara.EnGOPSize     = tEnPara.EnGOPSize * 2;
                        tEnPara.EnAspectRatio = AR_LETTER_BOX;

                        flags                 = VIDEO_ENCODER_FLAGS_UPDATE_WIDTH_HEIGHT |
                                                VIDEO_ENCODER_FLAGS_UPDATE_FRAME_RATE |
                                                VIDEO_ENCODER_FLAGS_UPDATE_GOP_SIZE |
                                                VIDEO_ENCODER_FLAGS_UPDATE_ASPECT_RATIO;

                        coreSetVideoEnPara(capdev, flags, srcIdx, &tEnPara);
                    }
                }

                dbg_msg(DBG_MSG_TYPE_INFO, "---- corestop Dev = %d ----\n", mmpCapGetCaptureDevice());
                mmpCapSetDeviceReboot(MMP_FALSE);
                coreStop();
                coreEnableAVEngine(MMP_TRUE);
                mmpWatchDogRefreshTimer();
                corePlay();
                dbg_msg(DBG_MSG_TYPE_INFO, "---- corePlay Dev = %d ----\n", mmpCapGetCaptureDevice());
            }
            else if (!mmpCapGetDeviceReboot() && avSyncIsVideoStable())
                mmpCapSetDeviceReboot(MMP_TRUE);

            preEnableRecord = gbEnableRecord;

            if (gbEnableRecord && !gbRecording && avSyncIsVideoStable())
            {
                if (_CheckUsbFreeSpace(&remainspace))
                {
                    MMP_INT     i;
                    RECORD_MODE recMode = {0};
//kenny patch
    #ifdef Change_Resolution
                    if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI)
                    {
                        preres = gtHDMIResolution;
                    }
                    else if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_YPBPR)
                    {
                        preres = gtYPBPRResolution;
                    }
                    else if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_CVBS)
                    {
                        preres = gtCVBSResolution;
                    }
    #endif

                    _GetNextRecordFileName(filename, recresolution);
                    for (i = strlen(filename) + 1; i >= 0; --i)
                        wfilename[i] = (MMP_WCHAR)filename[i];

                    fp = PalWFileOpen(wfilename, PAL_FILE_WB, MMP_NULL, MMP_NULL);
                    if (!fp)
                        PalWFileDelete(wfilename, MMP_NULL, MMP_NULL);
                    else
                        PalFileClose(fp, MMP_NULL, MMP_NULL);

    #ifdef  SPLITER_FILE
                    recMode.bDisableFileSplitter = MMP_FALSE;
    #else
                    recMode.bDisableFileSplitter =
                        (FAT_FILE_SYSTEM != storageMgrGetVolumeFileSystemFormat(storageMgrGetVolumeNumber(wfilename)));
    #endif
                    coreSetRecordMode(&recMode);
                    coreStartRecord(wfilename, MMP_NULL);
                    printf( "coreStartRecord (%s)\n", filename);
                    gbRecording = MMP_TRUE;
                }
                else
                {
                    printf( "---- USB No FreeSpace ----\n");
                    gtSystemStatus |= SYSTEM_USB_NO_FREESPACE_ERROR;
                    gbEnableRecord  = MMP_FALSE;
                }
            }

            if (gbRecording)
            {
                if (_IsContentProtection())
                {
                    dbg_msg(DBG_MSG_TYPE_INFO, "---- HDCP On , Can not Record ----\n");
                    gbEnableRecord = MMP_FALSE;
                    PalTFileDelete(wfilename, MMP_NULL, MMP_NULL);
                }

                //dbg_msg(DBG_MSG_TYPE_INFO, "remainspace(%d), WriteMB(%d), diff (%d)\n", remainspace, mencoder_GetWriteSize(), remainspace-mencoder_GetWriteSize());
                if (remainspace - mencoder_GetWriteSize() <= USB_MIN_FREESPACE_MB)
                {
                    dbg_msg(DBG_MSG_TYPE_INFO, "---- NO Free Space ----\n");
                    gtSystemStatus |= SYSTEM_USB_NO_FREESPACE_ERROR;
                    gbEnableRecord  = MMP_FALSE;
                }

                // If un-stable state lasts 2 more seconds, then stop recording.
                if (avSyncIsVideoStable())
                    videoUnstableClock = 0;
                else
                {
                    if (videoUnstableClock > 0)
                    {
                        if (PalGetDuration(videoUnstableClock) > 2000 && gbEnableRecord)
                        {
                            dbg_msg(DBG_MSG_TYPE_INFO, "---- Video Unstable , Can not Record ----\n");
                            gbEnableRecord = MMP_FALSE;
                        }
                    }
                    else
                        videoUnstableClock = PalGetClock();
                }
//kenny patch
    #ifdef Change_Resolution

                if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI)
                    resinput = gtHDMIResolution;
                else if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_YPBPR)
                    resinput = gtYPBPRResolution;
                else if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_CVBS)
                    resinput = gtCVBSResolution;

                if (preres != resinput)
                {
                    gbSrcChangeResolution = MMP_TRUE;
                    preres                = resinput;

                    recresolution         = 0;
        #ifdef YK_MINI_GRABBER
                    flaginput             = 0;
        #endif
                    VIDEO_ENCODER_INPUT_INFO srcIdx = coreGetInputSrcInfo();

                    if (RECORD_TABLE[srcIdx].isFullHDRes && srcIdx < MMP_CAP_INPUT_INFO_ALL)
                    {
                        recresolution = 1;
        #ifdef YK_MINI_GRABBER
                        flaginput     = 2;
        #endif
                    }
                }
    #endif

                if (gbSrcChangeResolution)
                {
                    MMP_INT     i;
                    RECORD_MODE recMode = {0};

                    gtSystemStatus |= SYSTEM_OUT_RECORD_STATUS;
                    coreStopRecord();
                    gtSystemStatus &= ~SYSTEM_OUT_RECORD_STATUS;

                    _GetNextRecordFileName(filename, recresolution);
                    for (i = strlen(filename) + 1; i >= 0; --i)
                        wfilename[i] = (MMP_WCHAR)filename[i];

    #ifdef SPLITER_FILE
                    recMode.bDisableFileSplitter = MMP_FALSE;
    #else
                    recMode.bDisableFileSplitter =
                        (FAT_FILE_SYSTEM != storageMgrGetVolumeFileSystemFormat(storageMgrGetVolumeNumber(wfilename)));
    #endif
                    coreSetRecordMode(&recMode);
                    coreStartRecord(wfilename, MMP_NULL);
                    dbg_msg(DBG_MSG_TYPE_INFO, "---- coreStartRecord (%s) ----\n", filename);
                    gbSrcChangeResolution = MMP_FALSE;
                }
            }

            if (gbEnableRecord == MMP_FALSE)
            {
                if (gbRecording)
                {
                    //PAL_CLOCK_T clock;

                    gbRecording     = MMP_FALSE;
                    gtSystemStatus |= SYSTEM_OUT_RECORD_STATUS;
                    coreStopRecord();
                    printf( "Write Test is Done (%d)\n", __LINE__);

                    {
                        MMP_UINT32 WriteDoneClock = PalGetClock();
                        while (PalGetDuration(WriteDoneClock) < WAIT_USB_WRITE_DONE_TIME)
                            PalSleep(1);
                    }

                    coreEnableAVEngine(MMP_FALSE);
                    gtSystemStatus &= ~SYSTEM_IN_RECORD_STATUS;
                    gtSystemStatus &= ~SYSTEM_OUT_RECORD_STATUS;
                }
            }
            CheckRecordClock = PalGetClock();
        }

    #if (defined(COMPONENT_DEV) || defined(COMPOSITE_DEV))
        //Auto Detect Device
        if (PalGetDuration(CheckDeviceClock) > 500)
        {
            CAPTURE_VIDEO_SOURCE CapDev = coreGetCaptureSource();
            CAPTURE_VIDEO_SOURCE newCapDev;

            if (avSyncIsVideoStable())
            {
                //static CAPTURE_VIDEO_SOURCE stableSource = MMP_CAP_VIDEO_SOURCE_UNKNOW;
                CAPTURE_VIDEO_SOURCE stableSource;
                CAPTURE_VIDEO_SOURCE newstableSource = coreGetCaptureSource();

                config_get_capturedev(&stableSource);
                if (stableSource != newstableSource)
                {
                    dbg_msg(DBG_MSG_TYPE_INFO, "---- stable source changed from %d to %d ----\n", stableSource, newstableSource);
                    config_set_capturedev(newstableSource);
                    stableSource = newstableSource;
                }

                if (_IsContentProtection())
                    gtSystemStatus |= SYSTEM_HDCP_PROTECTED;
                else
                    gtSystemStatus &= (~SYSTEM_HDCP_PROTECTED);

                gtSystemStatus &= (~SYSTEM_UNSTABLE_ERROR);
            }
            else
            {
                gtSystemStatus &= (~SYSTEM_HDCP_PROTECTED);
                gtSystemStatus |= SYSTEM_UNSTABLE_ERROR;
            }

            if (gbChangeDevice)
            {
                gtAudioCodec.bEnRecord = MMP_FALSE;
                _AudioCodecControl(&gtAudioCodec);
//kenny geniatech
        #ifndef INPUT_SWITCH_YPBPR_CVBS
                if (CapDev == CAPTURE_VIDEO_SOURCE_HDMI)
                    newCapDev = CAPTURE_VIDEO_SOURCE_YPBPR;
                else
                    newCapDev = CAPTURE_VIDEO_SOURCE_HDMI;
        #else
                dbg_msg(DBG_MSG_TYPE_INFO, "change device gtSystemStatus =%x\r\n", gtSystemStatus);
                if (gtSystemStatus & SYSTEM_DIGITAL_INPUT)
                    newCapDev = CAPTURE_VIDEO_SOURCE_HDMI;
                else
                {
                    if (gtSystemStatus & SYSTEM_YPBPR_INPUT)
                        newCapDev = CAPTURE_VIDEO_SOURCE_YPBPR;
                    else
                        newCapDev = CAPTURE_VIDEO_SOURCE_CVBS;
                }
        #endif
                printf("change device gtSystemStatus =%x\r\n", gtSystemStatus);
                gtSystemStatus |= SYSTEM_UNSTABLE_ERROR;
                dbg_msg(DBG_MSG_TYPE_INFO, "---- coreStop Dev = %d ----\n", newCapDev);
                mmpCapSetDeviceReboot(MMP_TRUE);
                coreStop();
                //codec_terminate();
                codec_initialize();
                gtAudioCodec.bPCConnectedMode = MMP_FALSE;
                gtAudioCodec.bHDMIInput       = ((newCapDev == CAPTURE_VIDEO_SOURCE_HDMI) || (newCapDev == CAPTURE_VIDEO_SOURCE_DVI)) ? MMP_TRUE : MMP_FALSE;
                gtAudioCodec.bDVIMode         = (newCapDev == CAPTURE_VIDEO_SOURCE_DVI) ? MMP_TRUE : MMP_FALSE;
                gtAudioCodec.bEnRecord        = MMP_FALSE;
                _AudioCodecControl(&gtAudioCodec);

                gbEnableRecord                = MMP_FALSE;
                mmpWatchDogRefreshTimer();
                coreSetCaptureSource(newCapDev);

                coreEnableAVEngine(MMP_FALSE);
                CapDev = newCapDev;
                corePlay();
                dbg_msg(DBG_MSG_TYPE_INFO, "---- corePlay Dev = %d ----\n", newCapDev);
                //config_set_capturedev(newCapDev);  //doesn`t saving config all the time.
                gbChangeDevice  = MMP_FALSE;
                //kenny geniatech
                gtSystemStatus &= (~SYSTEM_SWITCH_DIGITAL_ANALOG);
            }

            CheckDeviceClock = PalGetClock();
        }
    #endif

    #if defined(CONFIG_HAVE_USBD)
        if (gbDeviceMode)
        {
            extern int it_usbd_main();

            it_usbd_main();
        }
    #endif
#endif
    }

    return result;
}

int appmain(
    void)
{
    MMP_INT result = 0;

    result = Initialize();
    if (result)
        goto end;

    result = MainLoop();
    if (result)
        goto end;

    result = Terminate();
    if (result)
        goto end;

end:
    return result;
}

void
GrabberControlSetParam(
    GRABBER_CTRL_PARAM *pCtrl)
{
    switch (pCtrl->flag)
    {
    case GRABBER_CTRL_MIC_VOL:
        if (pCtrl->micVol == MIC_VOL_MUTE)
        {
            //mute
        }
        else
        {
            gtAudioCodec.micVolume    = DEF_MICVOLGAINDB;
            gtAudioCodec.micBoostGain = DEF_MICBOOSTGAIN;
        }
        config_set_microphone_volume((MMP_UINT32)pCtrl->micVol);
        break;

    case GRABBER_CTRL_LINE_BOOST:
        gtAudioCodec.hdmiBoostGain   = pCtrl->lineBoost;
        gtAudioCodec.analogBoostGain = pCtrl->lineBoost;
        config_set_line_boost((MMP_UINT32)pCtrl->lineBoost);
        break;

    case GRABBER_CTRL_HDCP:
        break;

    case GRABBER_CTRL_AUDIO_BITRATE:
        config_set_audio_bitrate(pCtrl->audiobitrate);
        break;

    case GRABBER_CTRL_HDMI_AUDIO_MODE:
        break;

    case GRABBER_CTRL_BLUE_LED:
        gtPCModeLed.BlueLed = pCtrl->BlueLed;
        break;

    case GRABBER_CTRL_GREEN_LED:
        gtPCModeLed.GreenLed = pCtrl->GreenLed;
        break;

    case GRABBER_CTRL_RED_LED:
        gtPCModeLed.RedLed = pCtrl->RedLed;
        break;

    case GRABBER_CTRL_BLING_RING_LED:
        gtPCModeLed.BlingRingLed = pCtrl->BlingRingLed;
        break;

    case GRABBER_CTRL_VIDEO_COLOR:
        {
            MMP_ISP_COLOR_CTRL ispCtrl;
            ispCtrl.brightness = pCtrl->COLOR.brightness;
            ispCtrl.contrast   = pCtrl->COLOR.contrast;
            ispCtrl.hue        = pCtrl->COLOR.hue;
            ispCtrl.saturation = pCtrl->COLOR.saturation;
            mmpIspSetColorCtrl(&ispCtrl);
            config_set_video_color_param(
                pCtrl->COLOR.brightness,
                pCtrl->COLOR.contrast,
                pCtrl->COLOR.hue,
                pCtrl->COLOR.saturation);
        }
        break;

    case GRABBER_CTRL_DIGITAL_AUDIO:
        config_set_digital_audio_volume(pCtrl->digitalVolume);
        break;

    default:
        break;
    }
}

void
GrabberControlGetParam(
    GRABBER_CTRL_PARAM *pCtrl)
{
    MMP_UINT32           i;
    MMP_UINT32           aspectRatio;
    MMP_UINT32           volume;
    MMP_UINT32           lineboost;
    DIGITAL_AUDIO_VOLUME digitalvolume;

    switch (pCtrl->flag)
    {
    case GRABBER_CTRL_MIC_VOL:
        config_get_microphone_volume(&volume);
        pCtrl->micVol = volume;
        break;

    case GRABBER_CTRL_LINE_BOOST:
        config_get_line_boost(&lineboost);
        pCtrl->lineBoost = lineboost;
        break;

    case GRABBER_CTRL_HDCP:
        pCtrl->bIsHDCP = _IsContentProtection();
        break;

    case GRABBER_CTRL_AUDIO_BITRATE:
        config_get_audio_bitrate(&pCtrl->audiobitrate);
        break;

    case GRABBER_CTRL_HDMI_AUDIO_MODE:
        pCtrl->hdmiAudioMode = mmpHDMIRXGetProperty(HDMIRX_AUDIO_MODE);
        break;

    case GRABBER_CTRL_BLUE_LED:
        pCtrl->BlueLed = gtPCModeLed.BlueLed;
        break;

    case GRABBER_CTRL_GREEN_LED:
        pCtrl->GreenLed = gtPCModeLed.GreenLed;
        break;

    case GRABBER_CTRL_RED_LED:
        pCtrl->RedLed = gtPCModeLed.RedLed;
        break;

    case GRABBER_CTRL_BLING_RING_LED:
        pCtrl->BlingRingLed = gtPCModeLed.BlingRingLed;
        break;

    case GRABBER_CTRL_VIDEO_COLOR:
        config_get_video_color_param(
            &(pCtrl->COLOR.brightness),
            &(pCtrl->COLOR.contrast),
            &(pCtrl->COLOR.hue),
            &(pCtrl->COLOR.saturation));
        break;

    case GRABBER_CTRL_DIGITAL_AUDIO:
        config_get_digital_audio_volume(&digitalvolume);
        pCtrl->digitalVolume = digitalvolume;
        break;

    default:
        break;
    }
}

MMP_BOOL
IsRecordState(
    void)
{
    return gbEnableRecord ? MMP_TRUE : MMP_FALSE;
}

//=============================================================================
//                              Private Function Definition
//=============================================================================
static void
_FirmwareUpgrade(
    void)
{
    MMP_INT    volumeIndex      = 0;
    MMP_BOOL   bUpgradeFinish   = MMP_FALSE;
    MMP_WCHAR  filePath[16]     = {0};
    PAL_FILE   *pRomFile        = MMP_NULL;
    MMP_UINT8  *pReadFileBuffer = MMP_NULL;
    MMP_UINT8  *pReadNorBuffer  = MMP_NULL;
    MMP_UINT32 writeNorSize     = 0;
    MMP_UINT32 fileSize         = 0;
    MMP_UINT8  pFileIdentifier[16];

    if (storageMgrGetUSBEvent(PAL_DEVICE_TYPE_USB0))
    {
        volumeIndex = storageMgrGetCardVolume(PAL_DEVICE_TYPE_USB0, 0);
        if (volumeIndex < 0)
            return;
        filePath[0] = PAL_T('A') + volumeIndex;
        PalMemcpy(&filePath[1], &PAL_T(":/jedi.img"), sizeof(PAL_T(":/jedi.img")));
        pRomFile    = PalTFileOpen(filePath, PAL_FILE_RB, MMP_NULL, MMP_NULL);
        if (pRomFile == MMP_NULL)
        {
            dbg_msg(DBG_MSG_TYPE_ERROR, "no rom file exist\n");
            return;
        }

        fileSize  = PalTGetFileLength(filePath);
        PalTFileRead(pFileIdentifier, 1, 16, pRomFile, MMP_NULL, MMP_NULL);
        fileSize -= 16;
        if ((pFileIdentifier[0] == 0 && pFileIdentifier[1] == 0) ||
            (CUSTOMER_CODE == 0 && PROJECT_CODE == 0))
        {
            // always pass, no need to check version
        }
        else if ((pFileIdentifier[0] != CUSTOMER_CODE) ||
                 (pFileIdentifier[1] != PROJECT_CODE))
        {
            PalTFileClose(pRomFile, MMP_NULL, MMP_NULL);
            dbg_msg(DBG_MSG_TYPE_ERROR, "version invalid\n");
            return;
        }

        pReadFileBuffer = PalHeapAlloc(PAL_HEAP_DEFAULT, fileSize);
        pReadNorBuffer  = PalHeapAlloc(PAL_HEAP_DEFAULT, fileSize);

        while (1)
        {
            if (bUpgradeFinish == MMP_FALSE)
            {
                if (fileSize == writeNorSize)
                {
                    NorRead(pReadNorBuffer, 0, writeNorSize);
                    if (0 == memcmp(pReadFileBuffer, pReadNorBuffer, writeNorSize))
                    {
                        PalTFileClose(pRomFile, MMP_NULL, MMP_NULL);
                        PalTFileDelete(filePath, MMP_NULL, MMP_NULL);
                        dbg_msg(DBG_MSG_TYPE_ERROR, "upgrade success\n");
                    }
                    else
                    {
                        PalTFileClose(pRomFile, MMP_NULL, MMP_NULL);
                        dbg_msg(DBG_MSG_TYPE_ERROR, "upgrade fail\n");
                    }
                    PalHeapFree(PAL_HEAP_DEFAULT, pReadFileBuffer);
                    PalHeapFree(PAL_HEAP_DEFAULT, pReadNorBuffer);
                    bUpgradeFinish  = MMP_TRUE;
                    gtSystemStatus &= ~SYSTEM_IN_UPGRADE_STATUS;
                    dbg_msg(DBG_MSG_TYPE_ERROR, "upgrade finish\n");
                    mmpWatchDogEnable(1);   //1s
                    dbg_msg(DBG_MSG_TYPE_ERROR, "---- Reboot SYSTEM ----\n");
                    while (1)
                    {
                        mmpWatchDogRefreshTimer();
                        PalSleep(1000);
                    }
                }
                else
                {
                    PalTFileRead(pReadFileBuffer, 1, fileSize, pRomFile, MMP_NULL, MMP_NULL);
                    HOST_WriteRegister(0x7c90, 0x4000);
                    HOST_WriteRegister(0x7c92, 0x0054);
                    NorInitial();
                    dbg_msg(DBG_MSG_TYPE_ERROR, "upgrade start\n");
                    gtSystemStatus |= SYSTEM_IN_UPGRADE_STATUS;
                    NorWrite(pReadFileBuffer, 0, fileSize);
                    writeNorSize   += fileSize;
                }
            }
        }
    }
}

static void *
_KeyManagerFunction(
    void *data)
{
    KEY_CODE   key;
    MMP_UINT32 pollingTime       = 50;
    MMP_UINT32 buttonPollingTime = 50;  // polling time of key pressed, extend to avoid repeat
    MMP_UINT32 index             = 0;
    MMP_UINT32 lockS3KeyClock    = 0;

    for (;;)
    {
        //kenny geniatech
        if (gbLockS3Key && PalGetDuration(lockS3KeyClock) > 2000) //kenny geniatech 4000
        {
            gbLockS3Key = MMP_FALSE;
        }

        if (avSyncIsAudioInitFinished())
        {
            static MMP_BOOL temp = MMP_FALSE;

            if (GpioMicIsInsert())
            {
                gtAudioCodec.bInsertMic = MMP_TRUE;
                if (temp == MMP_TRUE)
                {
                    printf("mic insert\r\n");
                    temp = MMP_FALSE;
                }
            }
            else
            {
                gtAudioCodec.bInsertMic = MMP_FALSE;
                temp                    = MMP_TRUE;
            }
        }

        buttonPollingTime = buttonPollingTime - pollingTime;
        if (buttonPollingTime == 0)
        {
            // polling time of key without pressing, reduce for sensitivity
            buttonPollingTime = 2000;

            key               = GpioKeyGetKey();
//kenny patch
#ifdef YK_MINI_GRABBER
            {
                static MMP_UINT32 lockKeyClock = 0;
                static MMP_UINT32 keyflag      = 0;
                if (key == KEY_CODE_S1 && keyflag == 0)
                {
                    lockKeyClock = PalGetClock();
                    keyflag      = 1;
                }

                if (keyflag == 1 && key == KEY_CODE_S1)
                {
                    if (PalGetDuration(lockKeyClock) > 3000)
                    {
                        key     = KEY_CODE_S2;
                        keyflag = 0;
                    }
                    else
                    {
                        key = KEY_CODE_UNKNOW;
                    }
                }
                else
                {
                    if (keyflag == 1)
                    {
                        key     = KEY_CODE_S1;
                        keyflag = 0;
                    }
                }
            }
#endif

#ifdef CUSTOMER_YONGKE_FUNCTION
            {
                MMP_UINT32 tempkey = 0;

                tempkey = mmpIrGetKey();

//IR 1
                if ((tempkey & 0xFFFF) == 0xFF00)
                {
                    tempkey = (tempkey >> 16) & 0xFFFF;
                    switch (tempkey)
                    {
    #if (defined(COMPONENT_DEV) || defined(COMPOSITE_DEV))
                    case  0xf609:
                        tempkey = 0xf708; //AV
                        break;

                    case 0xf807:
                        tempkey = 0xf609; //YPbPr
                        break;

                    case 0xf708:
                        tempkey = 0xf30c; //HDMI
                        break;

    #else
                    case 0xf30c:
                        tempkey = 0xfd02; //start record
                        break;

                    case  0xf609:
                        tempkey = 0xfc03; //stop record
                        break;

                    case 0xf50a:
                        tempkey = 0xfb04; //720p
                        break;

                    case 0xea15:
                        tempkey = 0xfa05; //1080p
                        break;
    #endif

    #if (defined(COMPONENT_DEV) || defined(COMPOSITE_DEV))
                    case 0xf20d:
                        tempkey = 0xf708; //AV
                        break;

                    case 0xe916:
                        tempkey = 0xf609; //YPbPr
                        break;

                    case 0xf10e:
                        tempkey = 0xf30c; //HDMI
                        break;

    #endif
                    default:
                        break;
                    }

                    tempkey = (tempkey << 16) | 0xfd02; //change IR
                    printf("IR 1  Key =%x\n", tempkey);
                }

                if ((tempkey & 0xFFFF) == 0xFd02)
                {
                    tempkey = (tempkey >> 16) & 0xFFFF;
                    printf("IR Key =%x,status =%x\n", tempkey, gtSystemStatus);
                    if ((tempkey == 0xfc03) && (gtSystemStatus & SYSTEM_IN_RECORD_STATUS))
                        key = KEY_CODE_S1;                                              //stop record
                    if (( tempkey == 0xfd02 ) && ( (gtSystemStatus & SYSTEM_IN_RECORD_STATUS) == 0))
                        key = KEY_CODE_S1;                                              //start record

                    if ( (tempkey == 0xfb04) && (gtSystemStatus & SYSTEM_FULL_HD_RECORD))
                        key = KEY_CODE_S2;                                              //720p
                    if ((tempkey == 0xfa05) && ( (gtSystemStatus & SYSTEM_FULL_HD_RECORD) == 0))
                        key = KEY_CODE_S2;                                              //1080p

    #if (defined(COMPONENT_DEV) || defined(COMPOSITE_DEV))

                    if ((gtSystemStatus & STSTEM_STAND_ALONE_MODE) &&
                        (gtSystemStatus & SYSTEM_IN_RECORD_STATUS) == 0x0 &&
                        (gtSystemStatus & SYSTEM_SWITCH_DIGITAL_ANALOG) == 0x0 &&
                        !gbLockS3Key)
                    {
                        if (( tempkey == 0xf708) && ((gtSystemStatus & SYSTEM_CVBS_INPUT) == 0))
                        {
                            gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
                            gtSystemStatus |= SYSTEM_YPBPR_INPUT;
                            key             = KEY_CODE_S3; //AV
                        }
                        if ((tempkey == 0xf609) && (( gtSystemStatus & SYSTEM_YPBPR_INPUT) == 0))
                        {
                            gtSystemStatus &= (~SYSTEM_CVBS_INPUT);
                            gtSystemStatus |= SYSTEM_DIGITAL_INPUT;
                            key             = KEY_CODE_S3; //YPBPR
                        }
                        if ((tempkey == 0xf30c ) && ( (gtSystemStatus & SYSTEM_DIGITAL_INPUT) == 0))
                        {
                            gtSystemStatus &= (~SYSTEM_YPBPR_INPUT);
                            gtSystemStatus |= SYSTEM_CVBS_INPUT;
                            key             = KEY_CODE_S3; //HDMI
                        }
                    }
    #endif
                }
            }
#endif
            switch (key)
            {
            case KEY_CODE_S1:
                if (avSyncIsVideoStable())
                {
                    MMP_UINT32 errorMask;

                    errorMask = SYSTEM_USB_UNPLUG_ERROR |
                                SYSTEM_USB_NO_FREESPACE_ERROR |
                                SYSTEM_USB_MOUNT_ERROR |
                                SYSTEM_USB_NO_STORAGE_ERROR |
                                SYSTEM_UNSTABLE_ERROR |
                                SYSTEM_HDCP_PROTECTED |
                                SYSTEM_OUT_RECORD_STATUS |
                                SYSTEM_USB_BUSY_STATUS;

                    if ((gtSystemStatus & errorMask))
                    {
                        dbg_msg(DBG_MSG_TYPE_INFO, "---- KEY_CODE_S1 NOT WORK ----\n");
                    }
                    else
                    {
                        if (!_IsContentProtection())
                        {
                            gbEnableRecord = !gbEnableRecord;

                            if (gbEnableRecord)
                                gtSystemStatus |= SYSTEM_IN_RECORD_STATUS;

                            dbg_msg(DBG_MSG_TYPE_INFO, "---- KEY_CODE_S1 ----\n");
                        }
                        else
                        {
                            dbg_msg(DBG_MSG_TYPE_INFO, "---- HDCP PROTECT ----\n");
                        }
                    }
                }
                break;
                //kenny geniatech
#if 1

            case KEY_CODE_S2:
                if (avSyncIsVideoStable() && (gtSystemStatus & STSTEM_STAND_ALONE_MODE))
                {
                    if ((gtSystemStatus & SYSTEM_IN_RECORD_STATUS) == 0x0)
                    {
                        MMP_BOOL isFULLHDSource;

                        if (RECORD_TABLE[mmpCapGetInputSrcInfo()].isFullHDRes)
                            isFULLHDSource = MMP_TRUE;
                        else
                            isFULLHDSource = MMP_FALSE;

                        if (isFULLHDSource)
                        {
                            if (gtSystemStatus & SYSTEM_FULL_HD_RECORD)
                                gtSystemStatus &= (~SYSTEM_FULL_HD_RECORD);
                            else
                                gtSystemStatus |= SYSTEM_FULL_HD_RECORD;
                        }
                        else
                            gtSystemStatus &= (~SYSTEM_FULL_HD_RECORD);

                        dbg_msg(DBG_MSG_TYPE_INFO, "---- KEY_CODE_S2 ----\n");
                    }
                }
                break;

            case KEY_CODE_S3:
                if ((gtSystemStatus & STSTEM_STAND_ALONE_MODE))
                {
                    dbg_msg(DBG_MSG_TYPE_INFO, "---- KEY_CODE_S3 ----\n");
                    if ((gtSystemStatus & SYSTEM_IN_RECORD_STATUS) == 0x0 &&
                        (gtSystemStatus & SYSTEM_SWITCH_DIGITAL_ANALOG) == 0x0 &&
                        !gbLockS3Key)
                    {
                        //kenny geniatech
    #ifndef INPUT_SWITCH_YPBPR_CVBS
                        if (gtSystemStatus & SYSTEM_DIGITAL_INPUT)
                            gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
                        else
                            gtSystemStatus |= SYSTEM_DIGITAL_INPUT;
    #else
                        dbg_msg(DBG_MSG_TYPE_INFO, "gtSystemStatus =%x\r\n", gtSystemStatus);
                        if (gtSystemStatus & SYSTEM_DIGITAL_INPUT)
                        {
                            gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
                            gtSystemStatus &= (~SYSTEM_CVBS_INPUT);
                            gtSystemStatus |= SYSTEM_YPBPR_INPUT;
                            dbg_msg(DBG_MSG_TYPE_INFO, "input: YPBPR\r\n");
                        }
                        else if (gtSystemStatus & SYSTEM_YPBPR_INPUT)
                        {
                            gtSystemStatus &= (~SYSTEM_DIGITAL_INPUT);
                            gtSystemStatus &= (~SYSTEM_YPBPR_INPUT);
                            gtSystemStatus |= SYSTEM_CVBS_INPUT;
                            dbg_msg(DBG_MSG_TYPE_INFO, "input: CVBS\r\n");
                        }
                        else if (gtSystemStatus & SYSTEM_CVBS_INPUT)
                        {
                            gtSystemStatus &= (~SYSTEM_CVBS_INPUT);
                            gtSystemStatus &= (~SYSTEM_YPBPR_INPUT);
                            gtSystemStatus |= SYSTEM_DIGITAL_INPUT;
                            dbg_msg(DBG_MSG_TYPE_INFO, "input: HDMI\r\n");
                        }
    #endif
                        gtSystemStatus |= SYSTEM_SWITCH_DIGITAL_ANALOG;
                        gbLockS3Key     = MMP_TRUE;
                        lockS3KeyClock  = PalGetClock();
                    }
                    else
                        dbg_msg(DBG_MSG_TYPE_INFO, "---- KEY_CODE_S3 NOT WORK ----\n");
                }
                break;

#endif
            default:
                // polling time of key without pressing, reduce for sensitivity
                buttonPollingTime = 50;
                break;
            }
        }

        gtAudioCodec.bHDMIInput = ((coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI) || (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI)) ? MMP_TRUE : MMP_FALSE;
        gtAudioCodec.bDVIMode   = (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI) ? MMP_TRUE : MMP_FALSE;
        gtAudioCodec.bEnRecord  = gbEnableRecord;
        _AudioCodecControl(&gtAudioCodec);

content_switch:
        PalSleep(pollingTime);
    }
}

static void
_UsbManagerFunction(
    void)
{
    static MMP_BOOL   bUsbInsert = MMP_FALSE;
    MMP_UINT32        remainspace;
    MMP_UINT8         filename[128];
    static MMP_WCHAR  wfilename[64];
    static MMP_CHAR   *buf         = MMP_NULL;
    MMP_INT           i;
    static PAL_FILE   *fp          = MMP_NULL;
    static MMP_UINT32 writeMBCount = 0;

    /* Check USB Plug/Unplug */
    if (!storageMgrGetUSBEvent(PAL_DEVICE_TYPE_USB0))
    {
        if (gbEnableRecord)
        {
            gbEnableRecord  = MMP_FALSE;
            //dbg_msg(DBG_MSG_TYPE_INFO, " USB unplug , Stop Record!!!!!\n");
            gtSystemStatus |= SYSTEM_USB_UNPLUG_ERROR;
            gtSystemStatus &= ~SYSTEM_IN_RECORD_STATUS;
        }
        gtSystemStatus |= SYSTEM_USB_NO_STORAGE_ERROR;
        bUsbInsert      = MMP_FALSE;
    }
    else //USB HotPlug Detect
    {
        if (bUsbInsert == MMP_FALSE)
        {
            gtSystemStatus |= SYSTEM_USB_BUSY_STATUS;

            /* Check USB Mount */
            if (!storageMgrGetCardVolume(PAL_DEVICE_TYPE_USB0, 0))
                gtSystemStatus |= SYSTEM_USB_MOUNT_ERROR;
            else
                gtSystemStatus &= ~SYSTEM_USB_MOUNT_ERROR;

            /*Check USB FreeSpace*/
            bUsbInsert = MMP_TRUE;
            if (!_CheckUsbFreeSpace(&remainspace))
            {
                gtSystemStatus |= SYSTEM_USB_NO_FREESPACE_ERROR;
                writeMBCount    = 0;
                return;
            }
            else
                gtSystemStatus &= ~SYSTEM_USB_NO_FREESPACE_ERROR;

            // Write Test
            (void)strcpy(filename, "C:/rec.tmp");
            i = strlen(filename) + 1;
            for (; i >= 0; --i)
                wfilename[i] = (MMP_WCHAR)filename[i];

            fp = PalWFileOpen(wfilename, PAL_FILE_WB, MMP_NULL, MMP_NULL);
            if (fp)
            {
                if (buf)
                    PalHeapFree(PAL_HEAP_DEFAULT, buf);
                buf          = PalHeapAlloc(PAL_HEAP_DEFAULT, 1048576);
                writeMBCount = 30;
                dbg_msg(DBG_MSG_TYPE_INFO, "write test start\n");
            }
            else
            {
                writeMBCount = 0;
            }

            /* Clear Error Flag */
            gtSystemStatus &= ~SYSTEM_USB_NO_STORAGE_ERROR;
            gtSystemStatus &= ~SYSTEM_USB_UNPLUG_ERROR;
            dbg_msg(DBG_MSG_TYPE_INFO, "USB hotplug Detect gtSystemStatus = 0x%x\n", gtSystemStatus);
        }
        else
        {
            if (gtSystemStatus & SYSTEM_USB_BUSY_STATUS)
            {
                if (writeMBCount)
                {
                    if (buf && fp)
                        PalFileWrite(buf, 1, 1048576, fp, MMP_NULL, MMP_NULL);
                    writeMBCount--;
                }
                else
                {
                    if (buf)
                    {
                        PalHeapFree(PAL_HEAP_DEFAULT, buf);
                        buf = MMP_NULL;
                    }
                    if (fp)
                    {
                        PalTFileClose(fp, MMP_NULL, MMP_NULL);
                        fp = MMP_NULL;
                    }
                    PalTFileDelete(wfilename, MMP_NULL, MMP_NULL);
                    gtSystemStatus &= ~SYSTEM_USB_BUSY_STATUS;
                    dbg_msg(DBG_MSG_TYPE_INFO, "write test end\n");
                }
            }
        }
    }
}

static MMP_BOOL
_CheckUsbFreeSpace(
    MMP_UINT32 *remainspace)
{
    MMP_UINT32 availableSize_h;
    MMP_UINT32 availableSize_l;
    //MMP_FLOAT   availableSize;
    MMP_UINT32 freespace = 0;

    /* Check USB FreeSpace */
    PalDiskGetFreeSpace(storageMgrGetCardVolume(PAL_DEVICE_TYPE_USB0, 0), &availableSize_h, &availableSize_l, MMP_NULL, MMP_NULL);

    if (availableSize_l >= (1ul << 20) * 1000 || availableSize_h > 0)
        *remainspace = freespace = (availableSize_l >> 20) + (availableSize_h * 4 * 1024);
    else if (availableSize_l < (1ul << 20) * 1000 && availableSize_l >= (1ul << 10) * 1000)
        *remainspace = freespace = ((availableSize_l >> 10) / 1024);
    else if (availableSize_l < (1ul << 10) * 1000)
        *remainspace = freespace = 0;
    dbg_msg(DBG_MSG_TYPE_INFO, "freespace = %d MB\n", freespace);

    if (freespace <= USB_MIN_FREESPACE_MB)
    {
        dbg_msg(DBG_MSG_TYPE_INFO, "!!!! USB FULL MB !!!!\n");
        return MMP_FALSE;
    }
    else
        return MMP_TRUE;
}

static void
_LedTimerInit(
    void)
{
    mmpTimerResetTimer(LED_TIMER_NUM);

    // Initialize Timer IRQ
    ithIntrDisableIrq(LED_TIMER_NUM);
    ithIntrClearIrq(LED_TIMER_NUM);

    // register Timer Handler to IRQ
    ithIntrRegisterHandlerIrq(ITH_INTR_TIMER6, _LedTimerIsr, (void *)ITH_INTR_TIMER6);

    // set Timer IRQ to edge trigger
    ithIntrSetTriggerModeIrq(ITH_INTR_TIMER6, ITH_INTR_EDGE);

    // set Timer IRQ to detect rising edge
    ithIntrSetTriggerLevelIrq(ITH_INTR_TIMER6, ITH_INTR_HIGH_RISING);

    // Enable Timer IRQ
    ithIntrEnableIrq(ITH_INTR_TIMER6);

    mmpTimerSetTimeOut(LED_TIMER_NUM, LED_TIMER_TIMEOUT);
    mmpTimerCtrlEnable(LED_TIMER_NUM, MMP_TIMER_EN);

    dbg_msg(DBG_MSG_TYPE_INFO, "Led Timer Init\n");
}

static void
_LedTimerTerminate(
    void)
{
    mmpTimerCtrlDisable(LED_TIMER_NUM, MMP_TIMER_EN);
    mmpTimerResetTimer(LED_TIMER_NUM);
    ithIntrDisableIrq(ITH_INTR_TIMER6);
    ithIntrClearIrq(ITH_INTR_TIMER6);
}

static void
_LedTimerIsr(
    void *data)
{
    uint32_t timer       = (uint32_t)data;
    MMP_BOOL isRecordLed = MMP_FALSE;
    //kenny geniatech
    MMP_BOOL isAnalogLedOn = MMP_FALSE;
    MMP_BOOL isHDMILedOn   = MMP_FALSE;

    if (gTimerIsrCount % 12 == 0)
        mmpWatchDogRefreshTimer();

    if (gTimerIsrCount % 3 == 0) // interval is LED_TIMER_TIMEOUT *3
    {
        if ((gtSystemStatus & STSTEM_STAND_ALONE_MODE))
        {
//kenny patch 20140428
#ifdef YK_MINI_GRABBER
            //Record Led Control
            static MMP_INT flag2 = 0;
            static MMP_INT flag1 = 0;

            {
                MMP_UINT32 statusMaskOn;
                MMP_UINT32 statusMaskBlink;

                statusMaskOn    = SYSTEM_IN_RECORD_STATUS;
                statusMaskBlink = SYSTEM_OUT_RECORD_STATUS |
                                  //    SYSTEM_USB_NO_STORAGE_ERROR|
                                  // SYSTEM_USB_NO_FREESPACE_ERROR|
                                  // SYSTEM_USB_BUSY_STATUS;
                                  SYSTEM_IN_UPGRADE_STATUS;

                if (gtSystemStatus & statusMaskBlink)  //unstable
                {
                    flag1 = 0;
                    if (flag2 == 0)
                    {
                        gtLedCtrl.EnRecord = MMP_FALSE;
                        gtLedCtrl.EnSignal = MMP_TRUE;
                        flag2              = 1;
                    }
                    isRecordLed = MMP_TRUE;
                    if (gTimerIsrCount % 6 == 0)
                    {
                        gtLedCtrl.EnRecord = (gtLedCtrl.EnRecord) ? MMP_FALSE : MMP_TRUE;
                        gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                    }
                    // printf("%d\n %d-%d\n",__LINE__, gtLedCtrl.EnRecord,gtLedCtrl.EnSignal);
                }
                else if (gtSystemStatus & statusMaskOn)  //recording
                {
                    isRecordLed = MMP_TRUE;
                    flag1       = 0;
                    flag2       = 0;
                    //kenny 20140103  enable 720p/1080p LED, when input in full HD
                    if (RECORD_TABLE[mmpCapGetInputSrcInfo()].isFullHDRes && mmpCapGetInputSrcInfo() < MMP_CAP_INPUT_INFO_ALL)
                    {
                        if (gtSystemStatus & SYSTEM_FULL_HD_RECORD)    //1080p
                        {
                            gtLedCtrl.EnSignal = MMP_FALSE;
                            gtLedCtrl.EnRecord = (gtLedCtrl.EnRecord) ? MMP_FALSE : MMP_TRUE;
                        }
                        else //1080p to 720p
                        {
                            gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                            gtLedCtrl.EnRecord = MMP_FALSE;
                        }
                    }
                    else   //normal
                    {
                        if (flaginput != 2)
                        {
                            gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                            gtLedCtrl.EnRecord = MMP_FALSE;
                        }
                    }
                    // printf("%d\n %d-%d\n",__LINE__, gtLedCtrl.EnRecord,gtLedCtrl.EnSignal);
                }
                else
                {
                    isRecordLed = MMP_FALSE;
                    flag2       = 0;
                    if (avSyncIsVideoStable())       //signal ok
                    {
                        flag1 = 0;
                        if (RECORD_TABLE[mmpCapGetInputSrcInfo()].isFullHDRes
                            && mmpCapGetInputSrcInfo() < MMP_CAP_INPUT_INFO_ALL && gtSystemStatus & SYSTEM_FULL_HD_RECORD)                               //1080p
                        {
                            gtLedCtrl.EnRecord = MMP_TRUE;
                            gtLedCtrl.EnSignal = MMP_FALSE;
                            flaginput          = 2;
                        }
                        else   //720p
                        {
                            gtLedCtrl.EnRecord = MMP_FALSE;
                            gtLedCtrl.EnSignal = MMP_TRUE;
                            flaginput          = 1;
                        }
                    }
                    else
                    {
                        if (flag1 == 0)
                        {
                            gtLedCtrl.EnRecord = MMP_FALSE;
                            gtLedCtrl.EnSignal = MMP_TRUE;
                            flag1              = 1;
                        }
                        flaginput          = 0;
                        gtLedCtrl.EnRecord = (gtLedCtrl.EnRecord) ? MMP_FALSE : MMP_TRUE;
                        gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                    }
                    // printf("%d\n %d-%d\n",__LINE__, gtLedCtrl.EnRecord,gtLedCtrl.EnSignal);
                }
            }

            GpioLedControl(&gtLedCtrl);
            gTimerIsrCount++;
            AHB_WriteRegisterMask(TIMER_BASE + 0x7C, 0x7 << (timer * 4), 0x7 << (timer * 4));
            return;
#else
            //Record Led Control
            {
                MMP_UINT32 statusMaskOn;
                MMP_UINT32 statusMaskBlink;

                statusMaskOn    = SYSTEM_IN_RECORD_STATUS;
                statusMaskBlink = SYSTEM_OUT_RECORD_STATUS |
                                  SYSTEM_IN_UPGRADE_STATUS;

                if (gtSystemStatus & statusMaskBlink)
                {
                    isRecordLed        = MMP_TRUE;
                    gtLedCtrl.EnRecord = (gtLedCtrl.EnRecord) ? MMP_FALSE : MMP_TRUE;
                }
                else if (gtSystemStatus & statusMaskOn)
                {
                    isRecordLed        = MMP_TRUE;
                    gtLedCtrl.EnRecord = MMP_TRUE;
                }
                else
                {
                    isRecordLed        = MMP_FALSE;
                    gtLedCtrl.EnRecord = MMP_FALSE;
                }
            }
#endif
            //kenny geniatech
            //Signal Led Control
#ifndef INPUT_SWITCH_YPBPR_CVBS
            if (!isRecordLed)
            {
                MMP_UINT32 statusMaskOn;
                MMP_UINT32 statusMaskBlink;

                statusMaskOn    = SYSTEM_UNSTABLE_ERROR;
                statusMaskBlink = SYSTEM_UNSTABLE_ERROR |
                                  SYSTEM_USB_NO_STORAGE_ERROR |
                                  SYSTEM_USB_NO_FREESPACE_ERROR |
                                  SYSTEM_USB_BUSY_STATUS;

                if (gtSystemStatus & statusMaskBlink)
                    gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                else if ((gtSystemStatus & statusMaskOn) == 0)
                    gtLedCtrl.EnSignal = MMP_TRUE;
                else
                    gtLedCtrl.EnSignal = MMP_FALSE;
            }
            else
                gtLedCtrl.EnSignal = MMP_FALSE;
#endif
            //720P & 1080P Control
            if (gtSystemStatus & SYSTEM_STARTUP_LED)
            {
                //kenny 20140103  enable 720p/1080p LED, when input in full HD
                if (RECORD_TABLE[mmpCapGetInputSrcInfo()].isFullHDRes && mmpCapGetInputSrcInfo() < MMP_CAP_INPUT_INFO_ALL)
                {
                    if (gtSystemStatus & SYSTEM_FULL_HD_RECORD)
                    {
                        gtLedCtrl.En720P  = MMP_FALSE;
                        gtLedCtrl.En1080P = MMP_TRUE;
                    }
                    else
                    {
                        gtLedCtrl.En720P  = MMP_TRUE;
                        gtLedCtrl.En1080P = MMP_FALSE;
                    }
                }
                else
                {
                    gtLedCtrl.En720P  = MMP_FALSE;
                    gtLedCtrl.En1080P = MMP_FALSE;
                }
            }

            //Analog & Digital Control
            if (gtSystemStatus & SYSTEM_STARTUP_LED)
            {
                MMP_UINT32 statusMaskOn;
                MMP_UINT32 statusMaskBlink;

                statusMaskOn    = SYSTEM_IN_RECORD_STATUS;
                statusMaskBlink = SYSTEM_UNSTABLE_ERROR |
                                  SYSTEM_SWITCH_DIGITAL_ANALOG;

                if (gtSystemStatus & SYSTEM_DIGITAL_INPUT)
                {
                    if (gtSystemStatus & statusMaskOn)
                        gtLedCtrl.EnHDMI = MMP_TRUE;
                    else if (gtSystemStatus & statusMaskBlink)
                        gtLedCtrl.EnHDMI = (gtLedCtrl.EnHDMI) ? MMP_FALSE : MMP_TRUE;
                    else
                        gtLedCtrl.EnHDMI = MMP_TRUE;
#ifdef INPUT_SWITCH_YPBPR_CVBS   //kenny 20140103 , as CVBS LED
                    gtLedCtrl.EnSignal = MMP_FALSE;
#endif
                    gtLedCtrl.EnAnalog = MMP_FALSE;
                }
                else
                {
                    //kenny geniatech
#ifndef INPUT_SWITCH_YPBPR_CVBS
                    if (gtSystemStatus & statusMaskOn)
                        gtLedCtrl.EnAnalog = MMP_TRUE;
                    else if (gtSystemStatus & statusMaskBlink)
                        gtLedCtrl.EnAnalog = (gtLedCtrl.EnAnalog) ? MMP_FALSE : MMP_TRUE;
                    else
                        gtLedCtrl.EnAnalog = MMP_TRUE;

                    gtLedCtrl.EnHDMI = MMP_FALSE;
#else
                    if (gtSystemStatus & SYSTEM_YPBPR_INPUT)
                    {
                        if (gtSystemStatus & statusMaskOn)
                            gtLedCtrl.EnAnalog = MMP_TRUE;
                        else if (gtSystemStatus & statusMaskBlink)
                            gtLedCtrl.EnAnalog = (gtLedCtrl.EnAnalog) ? MMP_FALSE : MMP_TRUE;
                        else
                            gtLedCtrl.EnAnalog = MMP_TRUE;

                        gtLedCtrl.EnSignal = MMP_FALSE;
                        gtLedCtrl.EnHDMI   = MMP_FALSE;
                    }
                    else if (gtSystemStatus & SYSTEM_CVBS_INPUT)
                    {
                        if (gtSystemStatus & statusMaskOn)
                            gtLedCtrl.EnSignal = MMP_TRUE;
                        else if (gtSystemStatus & statusMaskBlink)
                        {
                            gtLedCtrl.EnSignal = (gtLedCtrl.EnSignal) ? MMP_FALSE : MMP_TRUE;
                        }
                        else
                        {
                            gtLedCtrl.EnSignal = MMP_TRUE;
                        }
                        gtLedCtrl.EnAnalog = MMP_FALSE;
                        gtLedCtrl.EnHDMI   = MMP_FALSE;
                    }
#endif
                }
            }
        }
    }

    GpioLedControl(&gtLedCtrl);
    gTimerIsrCount++;
    AHB_WriteRegisterMask(TIMER_BASE + 0x7C, 0x7 << (timer * 4), 0x7 << (timer * 4));
}

static MMP_BOOL
_IsContentProtection(
    void)
{
    if ((mmpCapGetCaptureDevice() != CAPTURE_DEV_HDMIRX)
        || (!mmpHDMIRXIsHDCPOn()))
        return MMP_FALSE;
    else
        return MMP_TRUE;
}

// This is a special function from customer's request.
// Read file created by user to set RTC current time.
static void
_SetRtcFromFile(
    void)
{
#ifdef HAVE_FAT
    MMP_INT    volumeIndex      = 0;
    MMP_WCHAR  filePath[16]     = {0};
    PAL_FILE   *pRtcFile        = MMP_NULL;
    MMP_UINT8  *pReadFileBuffer = MMP_NULL;
    MMP_UINT32 fileSize         = 0;
    MMP_UINT8  *pDateStr        = MMP_NULL;
    MMP_UINT   i                = 0;
    MMP_UINT8  *str[6];
    MMP_UINT8  buf[10];

    if (storageMgrIsCardInsert(PAL_DEVICE_TYPE_SD) || storageMgrIsCardInsert(PAL_DEVICE_TYPE_USB0))
    {
        if (storageMgrIsCardInsert(PAL_DEVICE_TYPE_SD))
        {
            volumeIndex = storageMgrGetCardVolume(PAL_DEVICE_TYPE_SD, 0);
        }
        else
        {
            volumeIndex = storageMgrGetCardVolume(PAL_DEVICE_TYPE_USB0, 0);
        }
        if (volumeIndex < 0)
            return;
        filePath[0] = PAL_T('A') + volumeIndex;
        PalMemcpy(&filePath[1], &PAL_T(":/rtc_setup.txt"), sizeof(PAL_T(":/rtc_setup.txt")));
        pRtcFile    = PalTFileOpen(filePath, PAL_FILE_RB, MMP_NULL, MMP_NULL);

        if (pRtcFile)
        {
            fileSize        = PalTGetFileLength(filePath);
            dbg_msg(DBG_MSG_TYPE_INFO, "found JEDI rtc_setup file, size: %u\n", fileSize);
            pReadFileBuffer = (MMP_UINT8 *) PalHeapAlloc(PAL_HEAP_DEFAULT, fileSize);
            if (pReadFileBuffer)
            {
                PalTFileRead(pReadFileBuffer, 1, fileSize, pRtcFile, MMP_NULL, MMP_NULL);
            }

            pDateStr = strtok(pReadFileBuffer, "/ :");
            while (pDateStr != MMP_NULL)
            {
                PalMemset(buf, 0, 10);
                //dbg_msg(DBG_MSG_TYPE_INFO, "pDateStr= %s\n", pDateStr);
                str[i]   = pDateStr;
                //dbg_msg(DBG_MSG_TYPE_INFO, "str[%d] = %s\n", i , str[i]);

                pDateStr = strtok(MMP_NULL, "/ :");
                switch (i)
                {
                case 0:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gYear = atoi(buf);
                    break;

                case 1:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gMonth = atoi(buf);
                    break;

                case 2:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gDay = atoi(buf);
                    break;

                case 3:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gHour = atoi(buf);
                    break;

                case 4:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gMin = atoi(buf);
                    break;

                case 5:
                    PalMemcpy(buf, str[i], strlen(str[i]));
                    gSec = atoi(buf);
                    break;

                default:
                    break;
                }
                i++;
            }
            dbg_msg(DBG_MSG_TYPE_INFO, "y = %d, month= %d, day =%d, hour =%d, min = %d ,sec = %d\n",
                   gYear, gMonth, gDay, gHour, gMin, gSec);
            //kenny patch
    #ifdef EXTERNAL_RTC
            mmpExRtcSetDate(gYear, gMonth, gDay);
            mmpExRtcSetTime(gHour, gMin, gDay);
    #endif
            PalHeapFree(PAL_HEAP_DEFAULT, pReadFileBuffer);
            PalFileClose(pRtcFile, MMP_NULL, MMP_NULL);
            //kenny 20140428 PalTFileDelete(filePath, MMP_NULL, MMP_NULL);
        }
    }
#endif
}

void
_DeviceModeProcess(
    void)
{
    TS_MUXER_PARAMETER   tMuxerParam      = { 0 };
    MMP_UINT8            pServiceName[32] = { 0 };
    MMP_UINT32           micCurrVol;
    CAPTURE_VIDEO_SOURCE capdev;
    MMP_UINT32           lineboost;

    if (gbEnableRecord && gbRecording)
    {
        gbRecording     = MMP_FALSE;
        gbEnableRecord  = MMP_FALSE;
        gtSystemStatus |= SYSTEM_OUT_RECORD_STATUS;
        coreStopRecord();
        dbg_msg(DBG_MSG_TYPE_INFO, "Write Test is Done (%d)\n", __LINE__);

        {
            MMP_UINT32 WriteDoneClock = PalGetClock();
            while (PalGetDuration(WriteDoneClock) < WAIT_USB_WRITE_DONE_TIME)
                PalSleep(1);
        }

        coreEnableAVEngine(MMP_FALSE);
        gtSystemStatus &= ~SYSTEM_IN_RECORD_STATUS;
        gtSystemStatus &= ~SYSTEM_OUT_RECORD_STATUS;
    }

    dbg_msg(DBG_MSG_TYPE_INFO, "---- Enter PC Connected Mode ----\n");
    //_SetDeviceMode(MMP_TRUE);

    gtSystemStatus          |= SYSTEM_PC_CONNECTED_MODE;
    gtSystemStatus          &= ~STSTEM_STAND_ALONE_MODE;

    gtPCModeLed.BlueLed      = LED_OFF;
    gtPCModeLed.GreenLed     = LED_OFF;
    gtPCModeLed.RedLed       = LED_OFF;
    gtPCModeLed.BlingRingLed = LED_OFF;

    gtLedCtrl.EnAnalog       = MMP_FALSE;
    gtLedCtrl.EnHDMI         = MMP_FALSE;
    gtLedCtrl.En720P         = MMP_FALSE;
    gtLedCtrl.En1080P        = MMP_FALSE;
    gtLedCtrl.EnRecord       = MMP_FALSE;
    gtLedCtrl.EnSignal       = MMP_FALSE;
    GpioLedControl(&gtLedCtrl);
    coreStop();
    //codec_terminate();
    coreTerminate();
    PalSleep(1);
    coreInitialize(MMP_TS_MUX);
    config_get_capturedev(&capdev);
    coreSetCaptureSource(capdev);
    coreEnableAVEngine(MMP_TRUE);
    config_get_microphone_volume(&micCurrVol);
    //micCurrVol = 16; //30dB
    codec_initialize();
    config_get_line_boost(&lineboost);
    gtAudioCodec.bPCConnectedMode = MMP_TRUE;
    gtAudioCodec.bHDMIInput       = !!((coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI) || (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI));
    gtAudioCodec.bDVIMode         = !!(coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI);
    gtAudioCodec.bEnRecord        = MMP_FALSE;
    gtAudioCodec.bInsertMic       = MMP_FALSE;
    gtAudioCodec.hdmiBoostGain    = DEF_HDMIBOOSTGAIN;   //+0db
    gtAudioCodec.analogBoostGain  = DEF_ANALOGBOOSTGAIN; //+0db
    gtAudioCodec.micVolume        = DEF_MICVOLGAINDB;
    gtAudioCodec.micBoostGain     = DEF_MICBOOSTGAIN;
    _AudioCodecControl(&gtAudioCodec);

#ifdef TSO_ENABLE
    tMuxerParam.bEnableTso    = MMP_FALSE;
    tMuxerParam.bEnableEagle  = MMP_FALSE;
    tMuxerParam.constellation = CONSTELATTION_64QAM;
    tMuxerParam.codeRate      = CODE_RATE_7_8;
    tMuxerParam.guardInterval = GUARD_INTERVAL_1_32;
    tMuxerParam.frequency     = 887000;
    tMuxerParam.bandwidth     = 6000;
#endif
    sprintf(pServiceName, "AIR_CH_%d_%dM", tMuxerParam.frequency / 1000, tMuxerParam.bandwidth / 1000);
#ifdef AV_SENDER_SECURITY_MODE
    tMuxerParam.bEnableSecuirtyMode = MMP_TRUE;
#else
    tMuxerParam.bEnableSecuirtyMode = MMP_FALSE;
#endif
    tMuxerParam.bAddStuffData       = MMP_TRUE;
#ifdef MPEG_AUDIO_FOR_FY
    tMuxerParam.audioEncoderType    = MPEG_AUDIO_ENCODER;
#else
    tMuxerParam.audioEncoderType    = AAC_AUDIO_ENCODER;
#endif
    // Step 1: Removed all saved services
    coreTsRemoveServices();
    // Step 2: Specifiy the country code for NIT default setting (network_id, original_network_id, private_data_specifier_descriptor, LCN rule).
    coreTsUpdateCountryId(CORE_COUNTRY_TAIWAN);
    // Step 3: Update the modulation parameter for NIT.
    coreTsUpdateModulationParameter(tMuxerParam.frequency, tMuxerParam.bandwidth,
                                    tMuxerParam.constellation, tMuxerParam.codeRate, tMuxerParam.guardInterval);
    // Step 4: Insert desried service parameter for PMT, SDT, and NIT
    coreTsInsertService(0x100, 0x1000,
                        0x7D1, H264_VIDEO_STREAM,
#ifdef MPEG_AUDIO_FOR_FY
                        0x7D2, MPEG_AUDIO,
#else
                        0x7D2, AAC,
#endif
                        "ITE", sizeof("ITE"),
                        pServiceName, strlen(pServiceName));
    // Step 5: Notify the table setup is done, then generate the SI/PSI table including PAT, PMT, SDT, and NIT
    coreTsUpdateTable();
    coreSetMuxerParameter((void *) &tMuxerParam);
    //corePlay();
    gbDeviceMode = MMP_TRUE;
}

void
_HostModeProcess(
    void)
{
    TS_MUXER_PARAMETER   tMuxerParam      = { 0 };
    MMP_UINT8            pServiceName[32] = { 0 };
    MMP_INT32            micCurrVol;
    CAPTURE_VIDEO_SOURCE capdev;
    MMP_UINT32           lineboost;

    //_SetDeviceMode(MMP_FALSE);
    dbg_msg(DBG_MSG_TYPE_INFO, "---- Enter Stand Alone Mode ----\n");
    gtSystemStatus          &= ~SYSTEM_PC_CONNECTED_MODE;
    gtSystemStatus          |= STSTEM_STAND_ALONE_MODE;

    gtPCModeLed.BlueLed      = LED_OFF;
    gtPCModeLed.GreenLed     = LED_OFF;
    gtPCModeLed.RedLed       = LED_OFF;
    gtPCModeLed.BlingRingLed = LED_OFF;

    gtLedCtrl.EnAnalog       = MMP_FALSE;
    gtLedCtrl.EnHDMI         = MMP_FALSE;
    gtLedCtrl.En720P         = MMP_FALSE;
    gtLedCtrl.En1080P        = MMP_FALSE;
    gtLedCtrl.EnRecord       = MMP_FALSE;
    gtLedCtrl.EnSignal       = MMP_FALSE;
    GpioLedControl(&gtLedCtrl);
    coreSetRoot(MMP_FALSE);
    coreStop();
    //codec_terminate();
    coreTerminate();
    coreInitialize(MMP_MP4_MUX);
    config_get_capturedev(&capdev);
    coreSetCaptureSource(capdev);
    coreEnableAVEngine(MMP_TRUE);

    config_get_microphone_volume(&micCurrVol);
    //micCurrVol = 16; //30dB
    codec_initialize();
    config_get_line_boost(&lineboost);
    gtAudioCodec.bPCConnectedMode = MMP_FALSE;
    gtAudioCodec.bHDMIInput       = ((coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_HDMI) || (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI)) ? MMP_TRUE : MMP_FALSE;
    gtAudioCodec.bDVIMode         = (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI) ? MMP_TRUE : MMP_FALSE;
    gtAudioCodec.bEnRecord        = MMP_FALSE;
    gtAudioCodec.bInsertMic       = MMP_FALSE;
    gtAudioCodec.hdmiBoostGain    = DEF_HDMIBOOSTGAIN;   //+0db
    gtAudioCodec.analogBoostGain  = DEF_HDMIBOOSTGAIN;   //+0db
    gtAudioCodec.micVolume        = DEF_MICVOLGAINDB;
    gtAudioCodec.micBoostGain     = DEF_MICBOOSTGAIN;
    _AudioCodecControl(&gtAudioCodec);

#ifdef TSO_ENABLE
    tMuxerParam.bEnableTso    = MMP_FALSE;
    tMuxerParam.bEnableEagle  = MMP_FALSE;
    tMuxerParam.constellation = CONSTELATTION_64QAM;
    tMuxerParam.codeRate      = CODE_RATE_7_8;
    tMuxerParam.guardInterval = GUARD_INTERVAL_1_32;
    tMuxerParam.frequency     = 887000;
    tMuxerParam.bandwidth     = 6000;
#endif
    sprintf(pServiceName, "AIR_CH_%d_%dM", tMuxerParam.frequency / 1000, tMuxerParam.bandwidth / 1000);
#ifdef AV_SENDER_SECURITY_MODE
    tMuxerParam.bEnableSecuirtyMode = MMP_TRUE;
#else
    tMuxerParam.bEnableSecuirtyMode = MMP_FALSE;
#endif
    tMuxerParam.bAddStuffData       = MMP_FALSE;
#ifdef MPEG_AUDIO_FOR_FY
    tMuxerParam.audioEncoderType    = MPEG_AUDIO_ENCODER;
#else
    tMuxerParam.audioEncoderType    = AAC_AUDIO_ENCODER;
#endif
    // Step 1: Removed all saved services
    coreTsRemoveServices();
    // Step 2: Specifiy the country code for NIT default setting (network_id, original_network_id, private_data_specifier_descriptor, LCN rule).
    coreTsUpdateCountryId(CORE_COUNTRY_TAIWAN);
    // Step 3: Update the modulation parameter for NIT.
    coreTsUpdateModulationParameter(tMuxerParam.frequency, tMuxerParam.bandwidth,
                                    tMuxerParam.constellation, tMuxerParam.codeRate, tMuxerParam.guardInterval);
    // Step 4: Insert desried service parameter for PMT, SDT, and NIT
    coreTsInsertService(0x100, 0x1000,
                        0x7D1, H264_VIDEO_STREAM,
#ifdef MPEG_AUDIO_FOR_FY
                        0x7D2, MPEG_AUDIO,
#else
                        0x7D2, AAC,
#endif
                        "ITE", sizeof("ITE"),
                        pServiceName, strlen(pServiceName));
    // Step 5: Notify the table setup is done, then generate the SI/PSI table including PAT, PMT, SDT, and NIT
    coreTsUpdateTable();
    coreSetMuxerParameter((void *) &tMuxerParam);
    corePlay();
    gbDeviceMode = MMP_FALSE;
}

static void
_ProcessMsg(
    MSG_OBJECT *ptMsg)
{
    switch (ptMsg->name)
    {
    case MSG_CONFIG_STORE:
        config_store();
        break;

    case MSG_SERIAL_NUMBER_STORE:
        serial_number_store();
        break;

    case MSG_DEVICE_MODE:
        //_DEVICE_MODE_CTRL();
        _DeviceModeProcess();
        break;

    case MSG_HOST_MODE:
        //_SetDeviceMode(MMP_FALSE);
        _HostModeProcess();
        break;
    }
}

#ifdef SUPPORT_MIC_MIXED
static void
_AudioCodecControl(
    AUDIO_CODEC_CONTROL *pCodec)
{
    MMP_UINT32 micCurrVol = 0, micBoostGain = 0;

    if (pCodec->bEnRecord || pCodec->bPCConnectedMode)
    {
        if (pCodec->bInsertMic)
        {
            if (pCodec->bHDMIInput && !pCodec->bDVIMode)
            {
                codec_set_device(CODEC_INPUT_ONLY_MIC, 0);
            }
            else
            {
                codec_set_device(CODEC_INPUT_MIC_LINE, 1);
            }
        }
        else //mute case
        {
            if (pCodec->bHDMIInput && !pCodec->bDVIMode)
            {
                codec_set_device(CODEC_INPUT_NONE, 0);
            }
            else
            {
                codec_set_device(CODEC_INPUT_ONLY_LINE, 1);
            }
        }
    }
    else
    {
        if (pCodec->bInsertMic)
        {
            if (pCodec->bHDMIInput && !pCodec->bDVIMode)
            {
                codec_set_device(CODEC_INPUT_ONLY_MIC, 0);
            }
            else
            {
                codec_set_device(CODEC_INPUT_MIC_LINE, 1);
            }
        }
        else //mute case
        {
            if (pCodec->bHDMIInput && !pCodec->bDVIMode)
            {
                codec_set_device(CODEC_INPUT_NONE, 0);
            }
            else
            {
                codec_set_device(CODEC_INPUT_ONLY_LINE, 1);
            }
        }
    }

    micBoostGain = pCodec->micBoostGain;

    if (pCodec->micVolume < 0)
        micCurrVol = 23 - ((MMP_UINT32)((MMP_FLOAT)(pCodec->micVolume * -1.0) / 0.75));
    else
        micCurrVol = (MMP_UINT32)((MMP_FLOAT)((MMP_FLOAT)pCodec->micVolume) / 0.75) + 23;

    //dbg_msg(DBG_MSG_TYPE_INFO, "---(%d), %d, %d %d %d %d %d---\n", pCodec->micVolume, micBoostGain, micCurrVol, pCodec->hdmiBoostGain, pCodec->analogBoostGain,
    //                                        pCodec->bInsertMic, pCodec->bEnRecord);
    //dbg_msg(DBG_MSG_TYPE_INFO, "---- %d %d  %d----\n", pCodec->bPCConnectedMode, pCodec->bDVIMode,pCodec->bHDMIInput);
    codec_set_mic_boost(micBoostGain);
    codec_set_mic_vol(micCurrVol);
    codec_set_line_boost(1, pCodec->hdmiBoostGain);
    codec_set_line_boost(2, pCodec->analogBoostGain);
}
#else
static void
_AudioCodecControl(
    AUDIO_CODEC_CONTROL *pCodec)
{
    MMP_UINT32 micCurrVol = 0, micBoostGain = 0;

    if (pCodec->bHDMIInput && !pCodec->bDVIMode)
    {
        codec_set_device(CODEC_INPUT_NONE, 0);
    }
    else
    {
        codec_set_device(CODEC_INPUT_ONLY_LINE, 1);
    }

    micBoostGain = pCodec->micBoostGain;

    if (pCodec->micVolume < 0)
        micCurrVol = 23 - ((MMP_UINT32)((MMP_FLOAT)(pCodec->micVolume * -1.0) / 0.75));
    else
        micCurrVol = (MMP_UINT32)((MMP_FLOAT)((MMP_FLOAT)pCodec->micVolume) / 0.75) + 23;

    //dbg_msg(DBG_MSG_TYPE_INFO, "---(%d), %d, %d %d %d %d %d---\n", pCodec->micVolume, micBoostGain, micCurrVol, pCodec->hdmiBoostGain, pCodec->analogBoostGain,
    //                                        pCodec->bInsertMic, pCodec->bEnRecord);
    //dbg_msg(DBG_MSG_TYPE_INFO, "---- %d %d  %d----\n", pCodec->bPCConnectedMode, pCodec->bDVIMode,pCodec->bHDMIInput);
    codec_set_mic_boost(micBoostGain);
    codec_set_mic_vol(micCurrVol);
    codec_set_line_boost(1, pCodec->hdmiBoostGain);
    codec_set_line_boost(2, pCodec->analogBoostGain);
}
#endif
static void
_AutoChangeDevice(
    void)
{
    MMP_BOOL          bDevSignalStable;
    static MMP_BOOL   bDevNoSignalTrigger = MMP_FALSE;
    static MMP_UINT32 keepDevNoSignalTime = 0;
    MMP_BOOL          detectChange        = MMP_FALSE;

    if (coreGetCaptureSource() == CAPTURE_VIDEO_SOURCE_DVI)
        detectChange = MMP_FALSE;
    else if (!avSyncGetDeviceInitFinished())
        detectChange = MMP_FALSE;
    else if (gbChangeDevice)
        detectChange = MMP_FALSE;
    else if (gbEnableRecord)
        detectChange = MMP_FALSE;
    else if (gtSystemStatus & (SYSTEM_IN_RECORD_STATUS | SYSTEM_OUT_RECORD_STATUS))
        detectChange = MMP_FALSE;
    else if (!mmpHDMIRXGetProperty(HDMIRX_CHECK_ENGINE_IDLE) && mmpCapGetCaptureDevice() == MMP_CAP_DEV_HDMIRX)
        detectChange = MMP_FALSE;
    else
        detectChange = MMP_TRUE;

    if (detectChange)
    {
        bDevSignalStable = mmpCapDeviceIsSignalStable();
        //  dbg_msg(DBG_MSG_TYPE_INFO, "---%d, (%d), %d----\n", bDevSignalStable, PalGetDuration(keepDevNoSignalTime), bDevNoSignalTrigger);

        // signal stable
        if (bDevSignalStable || gbChangeDevice)
        {
            keepDevNoSignalTime = PalGetClock();
            bDevNoSignalTrigger = MMP_FALSE;
        }
        else if (!bDevSignalStable && (!bDevNoSignalTrigger))
        {
            keepDevNoSignalTime = PalGetClock();
            bDevNoSignalTrigger = MMP_TRUE;
        }

        if (PalGetDuration(keepDevNoSignalTime) >= 5000 && bDevNoSignalTrigger)
        {
            gbChangeDevice      = MMP_TRUE;
            bDevNoSignalTrigger = MMP_FALSE; //kenny
        }
    }
    else
    {
        keepDevNoSignalTime = PalGetClock();
        bDevNoSignalTrigger = MMP_FALSE;
    }

#ifdef INPUT_SWITCH_YPBPR_CVBS
    if ((gtSystemStatus & SYSTEM_DIGITAL_INPUT) &&
        (gtSystemStatus & SYSTEM_SWITCH_DIGITAL_ANALOG) == 0)
    {
        // dbg_msg(DBG_MSG_TYPE_INFO, "kenny test1\r\n");
        gbChangeDevice      = MMP_FALSE;
        keepDevNoSignalTime = PalGetClock();
        bDevNoSignalTrigger = MMP_FALSE;
    }
    else if (gtSystemStatus & SYSTEM_SWITCH_DIGITAL_ANALOG)
    {
        dbg_msg(DBG_MSG_TYPE_INFO, "kenny test2\r\n");
        gbChangeDevice = MMP_TRUE;
    }
#endif
}

static void
_GetNextRecordFileName(
    char       *filename,
    MMP_UINT32 rec_resolution)
{
#if  0//kenny 20150825 def EXTERNAL_RTC
    mmpExRtcGetDate(&gYear, &gMonth, &gDay);
    mmpExRtcGetTime(&gHour, &gMin, &gSec);
    sprintf(filename, "C:/%4d%02d%02d%02d%02d_%02d.mp4", gYear, gMonth, gDay, gHour, gMin, gSec);
#else
    #define MAX_REC_INDEX 10000
    MMP_UINT32 recFileIndex = 0;
    MMP_INT    i;
	
    config_get_rec_index(&recFileIndex);
#if 1

        if (recFileIndex >= MAX_REC_INDEX)
            recFileIndex = 0;
    #ifdef REGIA_FILE_NAME
        if (recFileIndex < 1)
            recFileIndex = 1;
        switch (rec_resolution)
        {
        case 0:
        default:
            sprintf(filename, "C:/REGIA_%04d.mp4", recFileIndex);
            break;

        case 1:
            sprintf(filename, "C:/REGIA_1080P_%04d.mp4", recFileIndex);
            break;

        case 2:
            sprintf(filename, "C:/REGIA_720P_%04d.mp4", recFileIndex);
            break;
        }
    #else
        switch (rec_resolution)
        {
        case 0:
        default:
            sprintf(filename, "C:/Encode_%d.mp4", recFileIndex);
            break;

        case 1:
            sprintf(filename, "C:/Encode_1080P_%d.mp4", recFileIndex);
            break;

        case 2:
            sprintf(filename, "C:/Encode_720P_%d.mp4", recFileIndex);
            break;
        }
    #endif

        recFileIndex++;
#else
    for (i = 0; i < MAX_REC_INDEX; ++i)
    {
           //move up
        if (recFileIndex >= MAX_REC_INDEX)
            recFileIndex = 0;

    #ifdef REGIA_FILE_NAME
        if (recFileIndex < 1)
            recFileIndex = 1;
        switch (rec_resolution)
        {
        case 0:
        default:
            sprintf(filename, "C:/REGIA_%04d.mp4", recFileIndex);
            break;

        case 1:
            sprintf(filename, "C:/REGIA_1080P_%04d.mp4", recFileIndex);
            break;

        case 2:
            sprintf(filename, "C:/REGIA_720P_%04d.mp4", recFileIndex);
            break;
        }
    #else
        switch (rec_resolution)
        {
        case 0:
        default:
            sprintf(filename, "C:/Encode_%d.mp4", recFileIndex);
            break;

        case 1:
            sprintf(filename, "C:/Encode_1080P_%d.mp4", recFileIndex);
            break;

        case 2:
            sprintf(filename, "C:/Encode_720P_%d.mp4", recFileIndex);
            break;
        }
    #endif

        recFileIndex++;

        {
           MMP_WCHAR       tmpfilename[255];
	     for (i = strlen(filename) + 1; i >= 0; --i)
                        tmpfilename[i] = (MMP_WCHAR)filename[i];
		 
            PAL_FILE *fp;
            printf("filename(%s)\n", filename);
            fp = PalWFileOpen(tmpfilename, PAL_FILE_RB, MMP_NULL, MMP_NULL);//kenny 20150815
            if (!fp)
            {
                break;
            }
            else
            {
                PalFileClose(fp, MMP_NULL, MMP_NULL);
            }
        }

    }
#endif
    config_set_rec_index(recFileIndex);
#endif
}