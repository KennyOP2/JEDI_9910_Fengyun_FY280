#include "usbd/inc/it_usbd.h"
#include "core_interface.h"
#include "config.h"

unsigned int
it_hw_grabber_set_datetime(
    unsigned int    year,
    unsigned int    month,
    unsigned int    day,
    unsigned int    hour,
    unsigned int    minute,
    unsigned int    second)
{
    coreRtcSetDateTime(year, month, day, hour, minute, second);
    return 0;
}

unsigned int
it_hw_grabber_set_microphone_volume(
    unsigned int    current_value)
{
    //coreSetMicInVolStep((MMP_UINT32)current_value);
    config_set_microphone_volume(current_value);
    return 0;
}

unsigned int
it_hw_grabber_get_microphone_volume(
    unsigned int*   p_current_value,
    unsigned int*   p_default_value,
    unsigned int*   p_min_value,
    unsigned int*   p_max_value)
{
    config_get_microphone_volume((MMP_UINT32*)p_current_value);
    *p_default_value = 44;
    *p_min_value = 0;
    *p_max_value = 63;
    return 0;
}

unsigned int
it_hw_grabber_set_encode_bitrate(
    unsigned int    resolution,
    unsigned int    bit_rate)
{
    config_set_encode_bitrate((MMP_UINT32)resolution, (MMP_UINT32)bit_rate);
    return 0;
}

unsigned int
it_hw_grabber_get_encode_bitrate(
    unsigned int    resolution,
    unsigned int*   p_bit_rate)
{
    config_get_encode_bitrate((MMP_UINT32)resolution, (MMP_UINT32*)p_bit_rate);
    return 0;
}

unsigned int
it_hw_grabber_set_pc_mode(
    unsigned int    enable)
{
    //printf("enable=%d\n", enable);
    coreSetHWGrabberPcMode(enable);
    return 0;
}

unsigned int
it_hw_grabber_capsense_read_i2c(
    unsigned char *pData,
    unsigned int NByte)
{
    return coreCapsenseReadI2C(pData, NByte);
}

unsigned int
it_hw_grabber_capsense_write_i2c(
    unsigned char *pData,
    unsigned int NByte)
{
    return coreCapsenseWriteI2C(pData, NByte);
}

unsigned int
it_hw_grabber_capsense_InterruptState(
    void)
{
    return coreCapsenseInterruptState();
}

unsigned int
it_hw_grabber_capsense_readFW(
    unsigned char *pData,
    unsigned int NByte)
{
    return coreCapsenseReadFW(pData, NByte);
}

unsigned int
it_hw_grabber_capsense_writeFW(
    unsigned char SubAddr,
    unsigned char *pData,
    unsigned int NByte)
{
    return coreCapsenseWriteFW(SubAddr, pData, NByte);
}

const void
it_hw_grabber_get_usb_serial_number(
    char* serialnum)
{
    coreGetUsbSerialNumber(serialnum);
}

const void
it_hw_grabber_set_usb_serial_number(
    char* serialnum)
{
    coreSetUsbSerialNumber(serialnum);
}

unsigned int
it_hw_grabber_set_led_blue(
    unsigned int    enable)
{
    coreSetBlueLed((LED_STATUS)!enable);
    return 0;
}

unsigned int
it_hw_grabber_set_led_green(
    unsigned int    enable)
{
    coreSetGreenLed((LED_STATUS)!enable);
    return 0;
}

unsigned int
it_hw_grabber_set_led_red(
    unsigned int    enable)
{
    coreSetRedLed((LED_STATUS)!enable);
    return 0;
}

unsigned int
it_hw_grabber_set_led_bling_bing(
    unsigned int    enable)
{
    coreSetBlingRingLed((LED_STATUS)!enable);
    return 0;
}
