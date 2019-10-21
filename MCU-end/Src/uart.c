#include "uart.h"
#include "esp8266.h"
#include "main.h"
#include "escalator.h"

extern volatile Wifi wifi;
extern volatile Escalator escalator;
extern volatile uint8_t pdata wifiSendBuffer[SEND_BUFFER_SIZE];
extern volatile uint8_t pdata wifiRecvBuffer[RECV_BUFFER_SIZE];
volatile Uart uart;
extern volatile Mcu mcu;
extern const uint16_t xdata SPEED_TABLE[6];
/* debug */ SI_SBIT (LED0, SFR_P1, 4);
/* WARN: TODO: if this func encountered 0(null) in sendbuffer it will treat it as end of data then stop counting data requeste to send */
void uartSend(uint8_t* buffer, uint8_t byteWaiting)
{
	uint8_t byteSend = 0, index = 0;

	/* WARN */while (buffer[index] != NULL) /* we don't use strlen cuz its implementation is not guaranteed and this is a safe way to check data size */
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
	//SCON0_RI = 0;
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
		
		if (escalator.mode & 0x2) /* DAC speed is not needed in this mode, hence we OR it */
			wifiSendBuffer[0] |= 0x80;

		uartSend(&wifiSendBuffer, UART_DAC_SIZE);
	}
}

void uartTransmission(void)
{
	switch (uart.Tstate)
	{
		case WAIT_KNOCK_DOOR:			
			if (uart.queuingByte != UART_KNOCK_DOOR_SIZE && uart.queuingByte != 0)	/* prevent truncated data */
			{
				if (wifi.currentTick == 0)
				{
					wifi.currentTick = mcu.sysTick;
				}
				if ((wifi.currentTick + UART_TRUNCATED_WAIT_TIME) <= mcu.sysTick)	/* uart data truncation confirmed */
				{
					uart.queuingByte = 0;
					uart.currentPos = 0;	/* this shall implemented as function since we usually use it to reset problemed uart */
					uart.byteWaiting = UART_KNOCK_DOOR_SIZE;
				}
			}
			break;
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
			if (uartIsDataKnockDoor())	break;	/* now, Im not recommend this style due to its exploitibility */
			if (uartIsEndTrainData())	break;		
			if (escalator.mode == NORMAL) uartApplyDACData();
			wifi.isDataChanged--;
			uart.Tstate = IDLE;		
			memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
			memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
			break;
	}
}

void uartApplyDACData(void)
{
	uint8_t savedpage = SFRPAGE;

	SFRPAGE = PG4_PAGE;

	DAC0L = wifiRecvBuffer[UART_DAC_SIZE - 6];
	DAC0H = wifiRecvBuffer[UART_DAC_SIZE - 5];
	DAC1L = wifiRecvBuffer[UART_DAC_SIZE - 4];
	DAC1H = wifiRecvBuffer[UART_DAC_SIZE - 3];
	DAC2L = wifiRecvBuffer[UART_DAC_SIZE - 2];
	DAC2H = wifiRecvBuffer[UART_DAC_SIZE - 1];
	
	SFRPAGE = savedpage;
}

bool uartIsDataKnockDoor(void)	/* with this implementation, data similarity should be considered, AND WARN THAT IF THE PACKET IS MALFORMED */
{
	uint8_t savedpage;
	if (wifiRecvBuffer[0] >> 7) /* verify mode bit */
	{
		escalator.mode = AUTO_SPEED;
		wifiRecvBuffer[0] &= 0x7f; /* clear mode bit */

		savedpage = SFRPAGE;

		SFRPAGE = PG4_PAGE;
		/* set all motor to the lowest speed */
		DAC0L = (uint8_t) (SPEED_TABLE[1] & 0xff);
		DAC0H = (uint8_t) (SPEED_TABLE[1] >> 8);
		DAC1L = (uint8_t) (SPEED_TABLE[1] & 0xff);
		DAC1H = (uint8_t) (SPEED_TABLE[1] >> 8);
		DAC2L = (uint8_t) (SPEED_TABLE[1] & 0xff);
		DAC2H = (uint8_t) (SPEED_TABLE[1] >> 8);

		SFRPAGE = savedpage;
	}
	if ((wifiRecvBuffer[0] == 'R') && (wifiRecvBuffer[1] == 'D') && (wifiRecvBuffer[2] == 'Y'))	
	{		
		wifiSendBuffer[0] = 'A';
		wifiSendBuffer[1] = 'C';
		wifiSendBuffer[2] = 'K';
		uartSend(&wifiSendBuffer, NO_DATA_EXPECTED);
		while (uart.state != STANDBY) {}
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
		uart.Tstate = IDLE;	/* training is about to begin */
		return true;
	}
	/* WARN: with this implementation, we must guarantee that there are no other data sent at KNOCK_DOOR state, otherwise it will make mcu starts training once received some data (TODO) */
	return false;
}

bool uartIsEndTrainData(void)	/* with this implementation, data similarity should be considered */
{
	uint8_t index;
	if ((wifiRecvBuffer[0] == 'E') && (wifiRecvBuffer[1] == 'N') && (wifiRecvBuffer[2] == 'D'))	
	{		
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		wifiSendBuffer[0] = 'A';
		wifiSendBuffer[1] = 'C';
		wifiSendBuffer[2] = 'K';
		uartSend(&wifiSendBuffer, NO_DATA_EXPECTED);
		while (uart.state != STANDBY); /* wait till ACK sent */
		/* reset state for get ready for next training*/
		uart.queuingByte = 0;
		uart.byteWaiting = UART_KNOCK_DOOR_SIZE;
		uart.Tstate = WAIT_KNOCK_DOOR;
		uart.state = STANDBY;
		/* re-init training args and reset DAC---- */		
		escalator.mode = NORMAL;
		escalator.autoSpeedTick = 0;
		escalator.queueTask = 0;
    	for (index = 0; index < 3; index++)
    	{
    	    escalator.arm[index].variability[0] = 0;
    	    escalator.arm[index].variability[1] = 0;
    	    escalator.arm[index].variability[2] = 0;
    	    escalator.arm[index].variability[3] = 0;
    	    escalator.arm[index].lastPos = POS_INIT;
    	    escalator.arm[index].currentPos = POS_2of4;
			escalator.arm[index].isDACSuccessive = 0;
        	escalator.arm[index].successiveDACTarget = 0;
			escalator.arm[index].successiveTimestamp = 0;
    	}
		index = SFRPAGE; /* we continued use index to reduce memory usage and index here is simple enough */
		SFRPAGE = PG4_PAGE;
    	DAC0L = 0x0;
		DAC0H = 0x00;
		DAC1L = 0x0;
		DAC1H = 0x00;
		DAC2L = 0x0;
		DAC2H = 0x00;
		SFRPAGE = index;
		wifi.currentTick = mcu.sysTick;
		while ((wifi.currentTick + DAC_APPLY_TIME) >= mcu.sysTick);
		wifi.currentTick = 0;
		wifi.isDataChanged = 0;
		mcu.sysTick = 0;
		/*--------------------------*/
		memset(&wifiRecvBuffer, 0, RECV_BUFFER_SIZE);
		memset(&wifiSendBuffer, 0, SEND_BUFFER_SIZE);
		/*----------------------------------------------*/
		return true;
	}
	return false;
}
void uartInit(void)
{
	uart.state = STANDBY;
	uart.currentPos = 0;
	uart.queuingByte = 0; 
	uart.byteWaiting = UART_KNOCK_DOOR_SIZE;

	uart.Tstate = WAIT_KNOCK_DOOR;
	SCON0 |= SCON0_REN__RECEIVE_ENABLED; /* enable this permanently due to we need to receive training end notification during training from PC, wifi shall need this style */
}

/*************flawless0714 * END OF FILE****/
