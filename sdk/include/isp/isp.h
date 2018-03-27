#ifndef __ISP_COMMON_H_Y9UQ8G8X_1Z7Z_EBF6_OXRW_U6T9196VF1RS__
#define __ISP_COMMON_H_Y9UQ8G8X_1Z7Z_EBF6_OXRW_U6T9196VF1RS__

#ifdef __cplusplus
extern "C" {
#endif


#include "isp/isp_types.h"
//=============================================================================
//                Constant Definition
//=============================================================================


//=============================================================================
//                Macro Definition
//=============================================================================


//=============================================================================
//                Structure Definition
//=============================================================================


//=============================================================================
//                Global Data Definition
//=============================================================================


//=============================================================================
//                Private Function Definition
//=============================================================================


//=============================================================================
//                Public Function Definition
//=============================================================================

//=============================================================================
/**
 * ISP default value initialization.
 */
//=============================================================================
MMP_RESULT 
ISP_ContextInitialize(
    MMP_BOOL initialMatrix);

//=============================================================================
/**
* Set Motion Detection Parameter.
**/
//=============================================================================
void 
ISP_EnableMotionDetectionParameter(
    void);

//=============================================================================
/**
* Clear Motion Detection Parameter.
**/
//=============================================================================
void 
ISP_DisableMotionDetectionParameter(
    void);
        
//=============================================================================
/**
* Set isp input format.
**/
//=============================================================================
MMP_RESULT 
ISP_SetInputFormat(
    MMP_ISP_INFORMAT    format);

//=============================================================================
/**
* Set isp output format.
**/
//=============================================================================
MMP_RESULT
ISP_SetOutputFormat(
    MMP_ISP_OUTFORMAT   format);
    
//=============================================================================
/**
* Update ISP device.
*
* @return MMP_RESULT_SUCCESS if succeed, error codes of MMP_RESULT_ERROR otherwise.
*/
//=============================================================================
MMP_RESULT
ISP_Update(
    void);

//=============================================================================
/**
* Set color effect
*
*/
//=============================================================================
void
ISP_SetColorCorrMatrix(
    ISP_COLOR_CORRECTION  *pColorCorrect,
    MMP_INT32 brightness,
    MMP_FLOAT contrast,
    MMP_INT32 hue,
    MMP_FLOAT saturation,
    MMP_INT32 colorEffect[2]);

//=============================================================================
/**
 * Update ISP color matrix device.
 *
 * @return ISP_RESULT_SUCCESS if succeed, error codes of ISP_RESULT_ERROR otherwise.
 */
//=============================================================================
MMP_RESULT
ISP_UpdateColorMatrix(
    void);

#ifdef __cplusplus
}
#endif

#endif

