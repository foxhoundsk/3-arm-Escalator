#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <SI_EFM8LB1_Register_Enums.h>
#include <stdlib.h>
#include "uart.h"
#include "esp8266.h"
#include "escalator.h"

#define DAC_APPLY_TIME 25
#define DAC_SUCCESSIVE_TIME_INTERVAL 15

void Init(void);
void ADC_setAutoScanInputs(uint8_t startChannel, uint8_t numChannels); /* @param 0~19 indicates p0.1 ~ p2.6 exclude p0.3 @param number of channels want to convert, maximum is 4 */
void taskHandler(void);
void taskUpdate(void);
void taskProcess(void);
void levelupSpeed(uint16_t dac, uint8_t num);
void successiveDACIncrement(void);
void emergencyStop(void);

typedef struct
{
    uint32_t sysTick; /* tick per second */
}Mcu;

#define L_ARM_IO p1_1
#define M_ARM_IO p1_2
#define R_ARM_IO p1_3

typedef struct{
  uint16_t L_ARM_IO;
  uint16_t M_ARM_IO;
  uint16_t R_ARM_IO;
}scan_t;

void ADC_enableAutoScan(scan_t* pbuffer, uint8_t numElements);

#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/
