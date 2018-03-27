/*
 * Copyright (c) 2009 SMedia Technology Corp. All Rights Reserved.
 */

#include "mmp_types.h"
#include "pal/pal.h"
#include "host/ahb.h"
#include "host/host.h"
#include "host/gpio.h"
#include "gpio_key_led_map.h"

#define Fengyun_Setting //kenny 20140103

//=============================================================================
//                              Structure Definition
//=============================================================================
typedef struct LED_GPIO_TAG
{
    MMP_UINT32 gpioLed_Analog;
    MMP_UINT32 gpioLed_HDMI;
    MMP_UINT32 gpioLed_720P;
    MMP_UINT32 gpioLed_1080P;
    MMP_UINT32 gpioLed_Signal;
    MMP_UINT32 gpioLed_Record;
} LED_GPIO;

//=============================================================================
//                              Constant Definition
//=============================================================================

#define BUTTON_CTRL_0 27       //GPIO27
#define BUTTON_CTRL_1 28       //GPIO28

/* For Customer Dexatek  +*/
#define MIC_IN        42       //GPIO42
/* For Customer Dexatek  -*/
//kenny 20140617
//#define MIC_IIS_SPDIF_IN       43
#define MIC_IIS_SPDIF_IN_2     43 //fongyun

//=============================================================================
//                              Extern Reference
//=============================================================================

//=============================================================================
//                              Global Data Definition
//=============================================================================
LED_GPIO gtLEDCtrl = {0};

//=============================================================================
//                              Private Function Declaration
//=============================================================================

//=============================================================================
//                              Public Function Definition
//=============================================================================
//=============================================================================
/**
 * GPIO led Process
 */
//=============================================================================
MMP_RESULT
GpioLEDInitialize(
    void)
{
    gtLEDCtrl.gpioLed_Analog = 31;
    gtLEDCtrl.gpioLed_HDMI   = 32;
    gtLEDCtrl.gpioLed_720P   = 29;
    gtLEDCtrl.gpioLed_1080P  = 23;    //red
#ifdef Fengyun_Setting
    gtLEDCtrl.gpioLed_Signal = 21;    //kenny geniatech  cvbs led   FY
    gtLEDCtrl.gpioLed_Record = 22;    //kenny geniatech 22     FY
#else
    gtLEDCtrl.gpioLed_Signal = 22;    //kenny geniatech  cvbs led   FY
    gtLEDCtrl.gpioLed_Record = 21;    //kenny geniatech 22     FY
#endif

    //Set GPIO Initialize Value
    GPIO_SetState(gtLEDCtrl.gpioLed_Analog, GPIO_STATE_LO);
    GPIO_SetState(gtLEDCtrl.gpioLed_HDMI, GPIO_STATE_LO);
    GPIO_SetState(gtLEDCtrl.gpioLed_720P, GPIO_STATE_LO);
    GPIO_SetState(gtLEDCtrl.gpioLed_1080P, GPIO_STATE_LO);
    GPIO_SetState(gtLEDCtrl.gpioLed_Signal, GPIO_STATE_LO);
    GPIO_SetState(gtLEDCtrl.gpioLed_Record, GPIO_STATE_LO);

    //Set GPIO Output Mode
    GPIO_SetMode(gtLEDCtrl.gpioLed_Analog, GPIO_MODE_OUTPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_HDMI, GPIO_MODE_OUTPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_720P, GPIO_MODE_OUTPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_1080P, GPIO_MODE_OUTPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_Signal, GPIO_MODE_OUTPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_Record, GPIO_MODE_OUTPUT);

    //Set GPIO Mode0
    ithGpioSetMode(gtLEDCtrl.gpioLed_Analog, ITH_GPIO_MODE0);
    ithGpioSetMode(gtLEDCtrl.gpioLed_HDMI, ITH_GPIO_MODE0);
    ithGpioSetMode(gtLEDCtrl.gpioLed_720P, ITH_GPIO_MODE0);
    ithGpioSetMode(gtLEDCtrl.gpioLed_1080P, ITH_GPIO_MODE0);
    ithGpioSetMode(gtLEDCtrl.gpioLed_Signal, ITH_GPIO_MODE0);
    ithGpioSetMode(gtLEDCtrl.gpioLed_Record, ITH_GPIO_MODE0);

    return MMP_SUCCESS;
}

MMP_RESULT
GpioLEDTerminate(
    void)
{
    //Set GPIO Input Mode
    GPIO_SetMode(gtLEDCtrl.gpioLed_Analog, GPIO_MODE_INPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_HDMI, GPIO_MODE_INPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_720P, GPIO_MODE_INPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_1080P, GPIO_MODE_INPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_Signal, GPIO_MODE_INPUT);
    GPIO_SetMode(gtLEDCtrl.gpioLed_Record, GPIO_MODE_INPUT);

    return MMP_SUCCESS;
}

void
GpioLedControl(
    GPIO_LED_CONTROL *pLedCtrl)
{
    if (pLedCtrl->EnAnalog)
        GPIO_SetState(gtLEDCtrl.gpioLed_Analog, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_Analog, GPIO_STATE_LO);

    if (pLedCtrl->EnHDMI)
        GPIO_SetState(gtLEDCtrl.gpioLed_HDMI, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_HDMI, GPIO_STATE_LO);

    if (pLedCtrl->En720P)
        GPIO_SetState(gtLEDCtrl.gpioLed_720P, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_720P, GPIO_STATE_LO);

    if (pLedCtrl->En1080P)
        GPIO_SetState(gtLEDCtrl.gpioLed_1080P, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_1080P, GPIO_STATE_LO);

    if (pLedCtrl->EnRecord)
        GPIO_SetState(gtLEDCtrl.gpioLed_Record, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_Record, GPIO_STATE_LO);

    if (pLedCtrl->EnSignal)
        GPIO_SetState(gtLEDCtrl.gpioLed_Signal, GPIO_STATE_HI);
    else
        GPIO_SetState(gtLEDCtrl.gpioLed_Signal, GPIO_STATE_LO);
}

//=============================================================================
/**
 * GPIO Key Process
 */
//=============================================================================
MMP_RESULT
GpioKeyInitialize(
    void)
{
    // Set GPIO as input
    GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_INPUT);
    GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);

    ithGpioSetMode(BUTTON_CTRL_0, ITH_GPIO_MODE0);
    ithGpioSetMode(BUTTON_CTRL_1, ITH_GPIO_MODE0);

    return MMP_SUCCESS;
}

MMP_RESULT
GpioKeyTerminate(
    void)
{
    // Set GPIO as input
    GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_INPUT);
    GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);

    return MMP_SUCCESS;
}

KEY_CODE
GpioKeyGetKey(
    void)
{
    //static PAL_CLOCK_T lastClock = 0;
    KEY_CODE   key = KEY_CODE_UNKNOW;
    GPIO_STATE button0, button1;
    MMP_UINT32 value, mask;
    //MMP_UINT32 flag;

    //if (PalGetDuration(lastClock) >= 300)
    {
        // BUTTON_CTRL_0 output low, BUTTON_CTRL_1 input
        GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_OUTPUT);
        GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);
        GPIO_SetState(BUTTON_CTRL_0, GPIO_STATE_LO);
        PalSleep(1);// have to delay

        AHB_ReadRegister(GPIO_BASE + GPIO_DATAIN_REG, &value);
        mask    = (1 << BUTTON_CTRL_1);
        button1 = ((value & mask) ? GPIO_STATE_HI : GPIO_STATE_LO);
        if (button1 == GPIO_STATE_LO)
        {
            // BUTTON_CTRL_0 input, BUTTON_CTRL_1 input
            GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_INPUT);
            GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);
            PalSleep(1); // have to delay

            AHB_ReadRegister(GPIO_BASE + GPIO_DATAIN_REG, &value);

            if (!(value & ((1 << BUTTON_CTRL_1)))) //BUTTON_CTRL_1 = LO
            {
                key = KEY_CODE_S2;
            }
            else
            {
                // BUTTON_CTRL_0 output low, BUTTON_CTRL_1 input
                GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_OUTPUT);
                GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);
                GPIO_SetState(BUTTON_CTRL_0, GPIO_STATE_LO);
                PalSleep(1);// have to delay

                AHB_ReadRegister(GPIO_BASE + GPIO_DATAIN_REG, &value);
                mask    = (1 << BUTTON_CTRL_1);
                button1 = ((value & mask) ? GPIO_STATE_HI : GPIO_STATE_LO);

                if (button1 == GPIO_STATE_LO)
                    key = KEY_CODE_S3;
            }
        }

        // BUTTON_CTRL_0 input, BUTTON_CTRL_1 input low
        GPIO_SetMode(BUTTON_CTRL_0, GPIO_MODE_INPUT);
        GPIO_SetMode(BUTTON_CTRL_1, GPIO_MODE_INPUT);
        PalSleep(1);// have to delay

        AHB_ReadRegister(GPIO_BASE + GPIO_DATAIN_REG, &value);
        mask    = (1 << BUTTON_CTRL_0);
        button0 = ((value & mask) ? GPIO_STATE_HI : GPIO_STATE_LO);
        mask    = (1 << BUTTON_CTRL_1);
        button1 = ((value & mask) ? GPIO_STATE_HI : GPIO_STATE_LO);

        if (button0 == GPIO_STATE_LO && button1 == GPIO_STATE_HI)
        {
            key = KEY_CODE_S1;
        }

        // no key press
        //lastClock = PalGetClock();
    }

    return key;
}

//=============================================================================
/**
 * GPIO Mic Detect Process
 */
//=============================================================================
MMP_RESULT
GpioMicInitialize(
    void)
{

    //kenny 20140617
  //  GPIO_SetState(MIC_IIS_SPDIF_IN, GPIO_STATE_LO);
    //Set GPIO Output Mode
 //   GPIO_SetMode(MIC_IIS_SPDIF_IN, GPIO_MODE_OUTPUT);
    //Set GPIO Mode0
  //  ithGpioSetMode(MIC_IIS_SPDIF_IN, ITH_GPIO_MODE0);

    GPIO_SetState(MIC_IIS_SPDIF_IN_2, GPIO_STATE_LO);
    //Set GPIO Output Mode
    GPIO_SetMode(MIC_IIS_SPDIF_IN_2, GPIO_MODE_OUTPUT);
    //Set GPIO Mode0
    ithGpioSetMode(MIC_IIS_SPDIF_IN_2, ITH_GPIO_MODE0);
	
    //Set GPIO Input Mode
    GPIO_SetMode(MIC_IN, GPIO_MODE_INPUT);
    ithGpioSetMode(MIC_IN, ITH_GPIO_MODE0);
    return MMP_SUCCESS;
}

MMP_BOOL
GpioMicIsInsert(
    void)
{
    return !GPIO_GetState(MIC_IN);
}

//kenny 20140617
MMP_BOOL
GpioInputselect(
    MMP_BOOL status)
{
    if (status)
    {
       // GPIO_SetState(MIC_IIS_SPDIF_IN, GPIO_STATE_LO);   //YPBPR
        GPIO_SetState(MIC_IIS_SPDIF_IN_2, GPIO_STATE_LO); //YPBPR
    }
    else
    {
      //  GPIO_SetState(MIC_IIS_SPDIF_IN, GPIO_STATE_HI);   //HDMI
        GPIO_SetState(MIC_IIS_SPDIF_IN_2, GPIO_STATE_HI); //HDMI
    }
}
