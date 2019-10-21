#include "Escalator.h"
#include "esp8266.h"


volatile Escalator escalator;
extern volatile uint8_t CONVERSION_COMPLETE;
//extern volatile uint16_t xdata adc_buf[3];
extern volatile Wifi wifi;
extern volatile Uart uart;
extern volatile scan_t xdata adc_buf;
const uint16_t xdata SPEED_TABLE[6] = { 0x0, 0x0106, 0x020d, 0x03b1, 0x0521, 0x0628};

/* TODO: if very early state the escalator should run? A: should not, since it may change IR read position */
void escalatorProcess(void)
{   
    uint8_t index, index2; /* for loop and array index use (WARN) */

    if (uart.Tstate == WAIT_KNOCK_DOOR) return;
    
    for (index = L_ARM; index <= R_ARM; index++) /* Since all sensors have its characteristic, so we done the detection seperately */
    {
        switch (index) /* p1_1 to p1_3 stands for escalator arm from left to right respectively*/
        {
            case L_ARM:
                if (is_DAC_RANGE(L_ARM_IO, L_H_10, L_L_10))  escalator.arm[L_ARM].variability[POS_1of4]++; else escalator.arm[L_ARM].variability[POS_1of4] = 0;
                if (is_DAC_RANGE(L_ARM_IO, L_H_40, L_L_40)) escalator.arm[L_ARM].variability[POS_2of4]++; else escalator.arm[L_ARM].variability[POS_2of4] = 0;
                if (is_DAC_RANGE(L_ARM_IO, L_H_70, L_L_70)) escalator.arm[L_ARM].variability[POS_3of4]++; else escalator.arm[L_ARM].variability[POS_3of4] = 0;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[L_ARM].variability[index2] >= ARM_TRIGGER_THRES)
                    {
                        /* if we don't have overlapped trigger interval (currently `R_H_70` and `R_L_40`), we 
                           can deprecate these reset operations except the triggered position since we have
                           already done this before entering the loop
                        */
                        escalator.arm[L_ARM].variability[POS_1of4] = 0;
                        escalator.arm[L_ARM].variability[POS_2of4] = 0;
                        escalator.arm[L_ARM].variability[POS_3of4] = 0;
                        escalator.arm[L_ARM].variability[POS_4of4] = 0;
                        escalator.arm[L_ARM].currentPos = index2;
                        if (escalator.arm[L_ARM].lastPos != escalator.arm[L_ARM].currentPos) /* if the detected pos is duplocated with last position we detected, we give up the update since the rat is at same position */
                        {
                            escalator.arm[L_ARM].lastPos = escalator.arm[L_ARM].currentPos;
                            wifi.isDataChanged++;
                        }                        
                    }
                }
                break;
					
            case M_ARM:
                if (is_DAC_RANGE(M_ARM_IO, M_H_10, M_L_10))  escalator.arm[M_ARM].variability[POS_1of4]++; else escalator.arm[M_ARM].variability[POS_1of4] = 0;
                if (is_DAC_RANGE(M_ARM_IO, M_H_40, M_L_40)) escalator.arm[M_ARM].variability[POS_2of4]++; else escalator.arm[M_ARM].variability[POS_2of4] = 0;
                if (is_DAC_RANGE(M_ARM_IO, M_H_70, M_L_70)) escalator.arm[M_ARM].variability[POS_3of4]++; else escalator.arm[M_ARM].variability[POS_3of4] = 0;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[M_ARM].variability[index2] >= ARM_TRIGGER_THRES)
                    {
                        escalator.arm[M_ARM].variability[POS_1of4] = 0;
                        escalator.arm[M_ARM].variability[POS_2of4] = 0;
                        escalator.arm[M_ARM].variability[POS_3of4] = 0;
                        escalator.arm[M_ARM].variability[POS_4of4] = 0;
                        escalator.arm[M_ARM].currentPos = index2;
                        if (escalator.arm[M_ARM].lastPos != escalator.arm[M_ARM].currentPos)
                        {
                            escalator.arm[M_ARM].lastPos = escalator.arm[M_ARM].currentPos;
                            wifi.isDataChanged++;
                        }                        
                    }
                }
                break;
            case R_ARM:   
                if (is_DAC_RANGE(R_ARM_IO, R_H_10, R_L_10))  escalator.arm[R_ARM].variability[POS_1of4]++; else escalator.arm[R_ARM].variability[POS_1of4] = 0;
                if (is_DAC_RANGE(R_ARM_IO, R_H_40, R_L_40)) escalator.arm[R_ARM].variability[POS_2of4]++; else escalator.arm[R_ARM].variability[POS_2of4] = 0;
                if (is_DAC_RANGE(R_ARM_IO, R_H_70, R_L_70)) escalator.arm[R_ARM].variability[POS_3of4]++; else escalator.arm[R_ARM].variability[POS_3of4] = 0;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[R_ARM].variability[index2] >= ARM_TRIGGER_THRES)
                    {
                        escalator.arm[R_ARM].variability[POS_1of4] = 0;
                        escalator.arm[R_ARM].variability[POS_2of4] = 0;
                        escalator.arm[R_ARM].variability[POS_3of4] = 0;
                        escalator.arm[R_ARM].variability[POS_4of4] = 0;
                        escalator.arm[R_ARM].currentPos = index2;
                        if (escalator.arm[R_ARM].lastPos != escalator.arm[R_ARM].currentPos)
                        {
                            escalator.arm[R_ARM].lastPos = escalator.arm[R_ARM].currentPos;
                            wifi.isDataChanged++;
                        }                        
                    }
                }                                    
                break;
							
        }
    }
}


















/*************flawless0714 * END OF FILE****/