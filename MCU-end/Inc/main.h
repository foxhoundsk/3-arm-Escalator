#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <SI_EFM8LB1_Register_Enums.h>
#include <stdlib.h>
#include "uart.h"
#include "esp8266.h"
#include "Escalator.h"

#define DAC_APPLY_TIME 25

void Init(void);
void ADC_setAutoScanInputs(uint8_t startChannel, uint8_t numChannels); /* @param 0~19 indicates p0.1 ~ p2.6 exclude p0.3 @param number of channels want to convert, maximum is 4 */


typedef struct
{
    uint32_t sysTick; /* tick per second */
}Mcu;

typedef struct{
  uint16_t p1_1;
  uint16_t p1_2;
  uint16_t p1_3;
}scan_t;

void ADC_enableAutoScan(scan_t* pbuffer, uint8_t numElements);

#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/
