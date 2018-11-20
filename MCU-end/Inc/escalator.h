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
}Arm;

typedef struct
{
    Arm arm[3];
    uint8_t intervalFlag;
    MODE mode;
    uint32_t autoSpeedTick;
    uint8_t queueTask;
}Escalator;

















#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/