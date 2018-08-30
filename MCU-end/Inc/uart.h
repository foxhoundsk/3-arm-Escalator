#ifndef __UART_H
#define __UART_H

#ifdef __cplusplus
 extern "C" {
#endif


#include <SI_EFM8LB1_Register_Enums.h>

void uartSend(uint8_t* buffer, uint8_t byteWaiting);
void uartInit(void);
void uartSendPosData(void);

typedef enum
{
    RECV_ERROR = -1,   /* TODO: check of specific flag which indicate that UART parity error, overrun... haven't implemented */
    STANDBY,
    RECV_START,
    RECV_DONE,
    SEND_START,
    SEND_DONE,
}State; 

typedef struct
{
    uint8_t currentPos;
    uint8_t queuingByte;
    uint8_t byteWaiting;
    State state;
}Uart;



















#ifdef __cplusplus
}
#endif

#endif

/*************flawless0714 * END OF FILE****/
