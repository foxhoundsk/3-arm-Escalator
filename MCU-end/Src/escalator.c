#include "Escalator.h"
#include "esp8266.h"


volatile Escalator escalator;
extern volatile uint8_t CONVERSION_COMPLETE;
//extern volatile uint16_t xdata adc_buf[3];
extern volatile Wifi wifi;
extern volatile scan_t xdata adc_buf[1];
const uint16_t SPEED_TABLE[6] = { 0x0, 0x0106, 0x020d, 0x03b1, 0x0521, 0x0628};

/* TODO: if very early state the escalator should run? Technically, should not,
         since it may change the pos val at certain circumstance, but we haven't
         encountered so far */
void escalatorProcess(void)
{   
    uint8_t index, index2; /* for loop and array index use (WARN) */
	/*	WARN: debug use
    if (wifi.state != RUNNING_TRAINING)
    {
        return;
    }
    */
    if (escalator.intervalFlag == 1)
    {
        escalator.intervalFlag = 0;
        for (index = 0; index < 3; index++)
        {
            escalator.arm[index].variability[0] = 0;    /* our buffer size is 4 */
            escalator.arm[index].variability[1] = 0;
            escalator.arm[index].variability[2] = 0;
            escalator.arm[index].variability[3] = 0;
        }
    }
    for (index = 0; index < 3; index++) /* Since all sensors have its characteristic, so we done the detection seperately */
    {
        switch (index) /* p1_1 to p1_3 stands for escalator arm from left to right respectively*/
        {
            case 0:                
                if (0x3fd <= adc_buf[0].p1_1 && adc_buf[0].p1_1 <= 0x3ff)  escalator.arm[index].variability[POS_1of4]++;
                else if (0x0170 <= adc_buf[0].p1_1 && adc_buf[0].p1_1 <= 0x01df) escalator.arm[index].variability[POS_2of4]++;
                else if (0x00a0 <= adc_buf[0].p1_1 && adc_buf[0].p1_1 <= 0x0100) escalator.arm[index].variability[POS_3of4]++;
                //else if (0x00a0 <= adc_buf[0].p1_2 && adc_buf[0].p1_2 <= 0x0100) escalator.arm[index].variability[POS_4of4]++;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[index].variability[index2] >= 3000)
                    {
                        escalator.arm[index].variability[POS_1of4] = 0; /* clear all variability to prevent near-side trigger*/
												escalator.arm[index].variability[POS_2of4] = 0;
												escalator.arm[index].variability[POS_3of4] = 0;
												escalator.arm[index].variability[POS_4of4] = 0;
                        escalator.arm[index].currentPos = index2;
                        if (escalator.arm[index].lastPos != escalator.arm[index].currentPos) /* check if the current pos equal to previous pos, if it doesn't, trigger isDataChanged flag which inform wifi module we got data to send */
                        {
                            escalator.arm[index].lastPos = escalator.arm[index].currentPos;
                            wifi.isDataChanged = 1; /* since we have three place uses this flag, so be aware that if you put this flag detector into interrupt */
                        }                        
                    }
                }  
                break;
					
            case 1:
                if (0x3fd <= adc_buf[0].p1_2 && adc_buf[0].p1_2 <= 0x3ff)  escalator.arm[index].variability[POS_1of4]++;
                /*0x256*/else if (0x0170 <= adc_buf[0].p1_2 && adc_buf[0].p1_2 <= 0x01df) escalator.arm[index].variability[POS_2of4]++;
                /*0x157*/else if (0x00a0 <= adc_buf[0].p1_2 && adc_buf[0].p1_2 <= 0x0100) escalator.arm[index].variability[POS_3of4]++;
                ///*0x0d9*/else if (0x00d8 <= adc_buf[0].p1_2 && adc_buf[0].p1_2 <= 0x00da) escalator.arm[index].variability[POS_4of4]++;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[index].variability[index2] >= 3000)
                    {
                        escalator.arm[index].variability[POS_1of4] = 0;
												escalator.arm[index].variability[POS_2of4] = 0;
												escalator.arm[index].variability[POS_3of4] = 0;
												escalator.arm[index].variability[POS_4of4] = 0;
                        escalator.arm[index].currentPos = index2;
                        if (escalator.arm[index].lastPos != escalator.arm[index].currentPos)
                        {
                            escalator.arm[index].lastPos = escalator.arm[index].currentPos;
                            wifi.isDataChanged = 1;
                        }                        
                    }
                }  
                break;
            case 2:   
                if (0x3fd <= adc_buf[0].p1_3 && adc_buf[0].p1_3 <= 0x3ff)  escalator.arm[index].variability[POS_1of4]++;
                else if (0x0170 <= adc_buf[0].p1_3 && adc_buf[0].p1_3 <= 0x01df) escalator.arm[index].variability[POS_2of4]++;
                else if (0x00a0 <= adc_buf[0].p1_3 && adc_buf[0].p1_3 <= 0x0100) escalator.arm[index].variability[POS_3of4]++;
                //else if (0x00d8 <= adc_buf[0].p1_3 && adc_buf[0].p1_3 <= 0x00da) escalator.arm[index].variability[POS_4of4]++;
                for (index2 = 0; index2 < 4; index2++)
                {
                    if (escalator.arm[index].variability[index2] >= 3000)
                    {
                        escalator.arm[index].variability[POS_1of4] = 0;
												escalator.arm[index].variability[POS_2of4] = 0;
												escalator.arm[index].variability[POS_3of4] = 0;
												escalator.arm[index].variability[POS_4of4] = 0;
                        escalator.arm[index].currentPos = index2;
                        if (escalator.arm[index].lastPos != escalator.arm[index].currentPos)
                        {
                            escalator.arm[index].lastPos = escalator.arm[index].currentPos;
                            wifi.isDataChanged = 1;
                        }                        
                    }
                }                                    
                break;
							
        }
    }
}


















/*************flawless0714 * END OF FILE****/