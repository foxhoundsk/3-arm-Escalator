#ifndef __ESCLATOR_H
#define __ESCLATOR_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "esp8266.h"

void escalatorProcess(void);


typedef enum
{ 
    POS_1of4 = 0,
    POS_2of4,
    POS_3of4,
    POS_4of4,
    POS_INIT
}RAT_POS;

typedef enum
{
    NORMAL = 1,
    AUTO_SPEED
}MODE;

typedef struct
{
    uint32_t variability[4];
    uint8_t lastPos;
    uint8_t currentPos;
    uint8_t isDACSuccessive;
    uint16_t successiveDACTarget;
    uint32_t successiveTimestamp;
}Arm;

typedef struct
{
    Arm arm[3];
    MODE mode;
    uint32_t autoSpeedTick;
    uint8_t queueTask;
}Escalator;

#define ARM_TRIGGER_THRES 1000

#define L_ARM 0 // left
#define M_ARM 1 // middle
#define R_ARM 2 // right

// ADC converted dist. interval (we use 10-bit ADC)
// you may notice that the value is pretty similar to each arm, but it is possible that we only calibrate single arm's sensor
#define L_L_10 0x03fd // ADC **low** (L) value at **left** (L) arm **10cm** (10)
#define L_H_10 0x03ff
#define L_L_40 0x0170
#define L_H_40 0x01df
#define L_L_70 0x00b0
#define L_H_70 0x017f

#define M_L_10 0x03fd
#define M_H_10 0x03ff
#define M_L_40 0x0170
#define M_H_40 0x01df
#define M_L_70 0x00a0
#define M_H_70 0x0167

#define R_L_10 0x03fd
#define R_H_10 0x03ff
#define R_L_40 0x0170
#define R_H_40 0x01df
#define R_L_70 0x00a0
#define R_H_70 0x017f

/**
 * is_DAC_RANGE - evaluate if given input (ARM_NAME) is within the `UPPER` bound and the `LOWER` bound
 * 
 * ARM_NAME I/O of the given arm, it's another macro, you can refer to this macro's reference to checkout that macro
 * UPPER upper bound of the interval
 * LOWER lower bound of the interval
 */
#define is_DAC_RANGE(ARM_NAME, UPPER, LOWER) \
        (((LOWER) <= (adc_buf. ## ARM_NAME)) && \
        ((adc_buf. ## ARM_NAME) <= (UPPER)))

#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/