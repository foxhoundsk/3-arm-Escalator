#include "uart.h"
#include "esp8266.h"

extern volatile Wifi wifi;
volatile Uart uart;

/* WARN: TODO: if this func encountered 0(null) in sendbuffer it will treat it as end of data then stop counting data requeste to send */
void uartSend(uint8_t* buffer, uint8_t byteWaiting)
{
	uint8_t byteSend = 0, index = 0;

	while (buffer[index] != NULL) /* we don't use strlen cuz its implementation is not guaranteed and this is a safe way to check data size */
	{
		byteSend++;
		index++;	
	}

	if (byteSend > SEND_BUFFER_SIZE)	/* WARN: data requested to send has exceeded send buffer size, and the handler haven't implemented, just note that if your UART didn't send you should check this */
	{
		return;
	}
	
	uart.queuingByte = byteSend;

	if ((byteWaiting > 0) && (byteWaiting <= RECV_BUFFER_SIZE)) /* set the size intend to receive, zero is no receive needed */
	{
		uart.byteWaiting = byteWaiting;
	}
	else
	{
		uart.byteWaiting = 0;
	}

	uart.state = SEND_START; /* TODO */
	wifi.state = DATA_SENDING;

	//IE_EA = 0;
	//IE |= IE_ES0__BMASK;
	SCON0_RI = 0;
	SCON0_TI = 1; /* trigger UART0 Tx interrupt */
	//IE_EA = 1;
	//SCON0_TI = 1;
}

/*
for data has 0 want to send, you should use this func or modify the orign send function
void uartSendPosData(void)
{
	uart.queuingByte = POS_DATA_SIZE;

	uart.state = SEND_START;
	wifi.state = DATA_SENDING;

	SCON0_RI = 0;
	SCON0_TI = 1; /* trigger UART0 Tx interrupt 
}
*/
void uartInit(void)
{
	uart.state = STANDBY;
	uart.currentPos = 0;
	uart.queuingByte = 0;
	uart.byteWaiting = 0;
}

/*************flawless0714 * END OF FILE****/
