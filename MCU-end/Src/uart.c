#include "uart.h"
#include "esp8266.h"

extern volatile Wifi wifi;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE]
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

	uart.state = SEND_START; /* TODO (deprecated, this state can be used to indicate that uart is not idling) */
	wifi.state = DATA_SENDING;

	/* uart DAC temp */	uart.Tstate = TX_BUSY;

	//IE_EA = 0;
	//IE |= IE_ES0__BMASK;
	SCON0_RI = 0;
	SCON0_TI = 1; /* trigger UART0 Tx interrupt */
	//IE_EA = 1;
	//SCON0_TI = 1;
}
/* this is a temporary func to replace wifi module's function, hence some func appear in this func has name prefixed "wifi" */
void uartIsDataQueue(void)
{
	if (wifi.isDataChanged) /* the flag is cleared once DAC data send back */
	{
		wifiPosDataEncode();
		uartSend(&wifiSendBuffer, UART_DAC_RECV_SIZE);
	}
}

void uartTransmission(void)
{
	switch (uart.Tstate)
	{
		case IDLE:
			uartIsDataQueue();
			break;
		case TX_BUSY:

			break;
		case RX_BUSY:
			if ((wifi.currentTick + UART_DAC_MAX_WAIT_TIME) <= mcu.sysTick) /* recv timeout detect */
			{
				uart.Tstate = IDLE;
				memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
				memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			}
			break;
		case RX_DONE:
			wifiApplyDACdata();
			wifi.isDataChanged = 0;
			uart.Tstate = IDLE;
			memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
			memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			break;
	}
}

void uartInit(void)
{
	uart.state = STANDBY;
	uart.currentPos = 0;
	uart.queuingByte = 0;
	uart.byteWaiting = 0;

	uart.Tstate = IDLE;
}

/*************flawless0714 * END OF FILE****/
