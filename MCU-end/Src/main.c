// Ver 5.0.0
/*
    1. Since the complicate step to setup wifi module(esp8266), we temporary deprecated the wifi transfer method and use USB-TTL instead.

*/

#include "main.h"

#define BC_DISCONNECTED 0              // 0 = Board Controller disconnected to EFM8 UART pins     
#define BC_CONNECTED    1              // 1 = Board Controller connected                                       
#define ADC0P7 7       /* ADC start address, P1.1, check register ADC0MX for more details */            
#define NUM_CHANNELS 3 /* number of ADC channel in this project */                   
#define ADC_BUFFER_SIZE 6
#define BUFFER_SIZE (NUM_CHANNELS)
#define BUFFER_START_ADDR (0x0000)
SI_LOCATED_VARIABLE_NO_INIT(adc_buf, scan_t, SI_SEG_XDATA, BUFFER_START_ADDR);
volatile Mcu mcu;
volatile uint8_t CONVERSION_COMPLETE = 0;
extern volatile Escalator escalator;
extern volatile Uart uart;
extern const uint16_t xdata SPEED_TABLE[6];
extern volatile Wifi wifi; 
SI_SBIT (LED0, SFR_P1, 4);    
SI_SBIT (BC_EN, SFR_P2, 2);

void main()
{
    Init();
    uartInit();
    wifiInit();
    BC_EN = BC_CONNECTED /*BC_CONNECTED*/; /* since we are now temporary using UART to send training data instead of wifi module, hence we assign UART to Board Controller to send data through usb virtual port directly */
    LED0 = 1; /* this indicate that sys is still breathing(~LED0 in interrupt) */
    IE_EA = 1;
        
    ADC_setAutoScanInputs(ADC0P7, NUM_CHANNELS);
    ADC_enableAutoScan(&adc_buf, BUFFER_SIZE);
    //ADC0CN0 |= 0x10; /* trigger ADC on */

    while(1)
    {
        //wifiProcess(); re-enable once wifi got smooth setup and cleanup
        uartTransmission();
        escalatorProcess();
        taskHandler();
        successiveDACIncrement();
        emergencyStop();

    }
}

void emergencyStop(void)
{
    uint8_t savedpage, index;
    
    savedpage = SFRPAGE;
    SFRPAGE = PG4_PAGE;

    for (index = 0; index < 3; index++)
    {
        if (!(escalator.arm[index].currentPos == 0))
            continue;

        switch (index)
        {
            case 0:
                DAC0L = 0;
                DAC0H = 0;
                break;
            case 1:
                DAC1L = 0;
                DAC1H = 0;
                break;
            case 2:
                DAC2L = 0;
                DAC2H = 0;
                break;
        }
    }

    SFRPAGE = savedpage;
}


void successiveDACIncrement(void)
{
    uint8_t index, savedpage;
    uint16_t DACval;
    savedpage = SFRPAGE;
    SFRPAGE = PG4_PAGE;

    for (index = 0; index < 3; index++)
    {
        if (!escalator.arm[index].isDACSuccessive)
            continue;
        if (escalator.arm[index].successiveTimestamp >= mcu.sysTick)
            continue;

        switch (index)
        {
            case 0:
                DACval = 0;
                DACval |= DAC0H;                        
                DACval <<= 8;                        
                DACval |= DAC0L;

                if (DACval == escalator.arm[0].successiveDACTarget) /* reached target speed */
                {
                    escalator.arm[0].isDACSuccessive = 0;
                    continue;
                }

                DACval++;

                DAC0L = (uint8_t) DACval & 0xff; /* As spec mentioned, DAC low byte should be write first */
                DAC0H = (uint8_t) ((DACval >> 8) & 0xff);

                escalator.arm[0].successiveTimestamp = mcu.sysTick + DAC_SUCCESSIVE_TIME_INTERVAL;
                break;
            case 1:
                DACval = 0;
                DACval |= DAC1H;
                DACval <<= 8;
                DACval |= DAC1L;

                if (DACval == escalator.arm[1].successiveDACTarget)
                {
                    escalator.arm[1].isDACSuccessive = 0;
                    continue;
                }

                DACval++;

                DAC1L = (uint8_t) DACval & 0xff;
                DAC1H = (uint8_t) ((DACval >> 8) & 0xff);

                escalator.arm[1].successiveTimestamp = mcu.sysTick + DAC_SUCCESSIVE_TIME_INTERVAL;
                break;
            case 2:
                DACval = 0;
                DACval |= DAC2H;
                DACval <<= 8;
                DACval |= DAC2L;

                if (DACval == escalator.arm[2].successiveDACTarget)
                {
                    escalator.arm[2].isDACSuccessive = 0;
                    continue;
                }

                DACval++;

                DAC2L = (uint8_t) DACval & 0xff;
                DAC2H = (uint8_t) ((DACval >> 8) & 0xff);

                escalator.arm[2].successiveTimestamp = mcu.sysTick + DAC_SUCCESSIVE_TIME_INTERVAL;
                break;
        }            
    }
    SFRPAGE = savedpage;
}

/* flush out done task */
void taskUpdate(void)
{
    escalator.queueTask >>= 2;
}

void levelupSpeed(uint16_t dac, uint8_t num)
{
    uint8_t i;
    for (i = 0; i < 5; i++)
    {
        if (dac == SPEED_TABLE[i])
        {            
            dac = SPEED_TABLE[i + 1]; /* level up two level of speed */
            escalator.arm[num].isDACSuccessive = 1;
            escalator.arm[num].successiveDACTarget = dac;
            /*
            switch (num)
            {
                case 0:                   
                    DAC0L = (uint8_t) dac & 0xff; // As spec mentioned, DAC low byte should be write first                  
                    DAC0H = (uint8_t) ((dac >> 8) & 0xff);
                    return;
                case 1:
                    DAC1L = (uint8_t) dac & 0xff;
                    DAC1H = (uint8_t) ((dac >> 8) & 0xff);
                    return;
                case 2:
                    DAC2L = (uint8_t) dac & 0xff;
                    DAC2H = (uint8_t) ((dac >> 8) & 0xff);
                    return;
            }
            */
        }
    }
}

void taskProcess(void)
{
    uint8_t savedpage, idx; /* may declare inside the task */
    uint16_t DACval;    
    switch (escalator.queueTask & 0x3) /* clarify the task */
    {
        case 0:            
            break; /* no task queuing */
        case 1:
            /* increase speed by one or two level */
            if (mcu.sysTick >= escalator.autoSpeedTick)
            {                
                escalator.autoSpeedTick += 10000;
                /* level up DAC speed here */
                savedpage = SFRPAGE;
                SFRPAGE = PG4_PAGE;

                for (idx = 0; idx < 3; idx++)
                {
                    DACval = 0; /* reset for each use to prevent trash value */
                    if (idx == 0)
                    {
                        DACval |= DAC0H;                        
                        DACval <<= 8;                        
                        DACval |= DAC0L;                         
                        levelupSpeed(DACval, idx);
                    }
                    else if (idx == 1)
                    {
                        DACval |= DAC1H;
                        DACval <<= 8;
                        DACval |= DAC1L;
                        levelupSpeed(DACval, idx);
                    }
                    else
                    {
                        DACval |= DAC2H;
                        DACval <<= 8;
                        DACval |= DAC2L;
                        levelupSpeed(DACval, idx);
                    }
                    
                }
                SFRPAGE = savedpage;
            }
            else /* task not done yet */
                return;
            break;
        case 2: /* this task don't need time check like case 1 since it */ 
            if (mcu.sysTick >= escalator.autoSpeedTick)
            {
                escalator.mode = NORMAL;
                escalator.queueTask |= 0xC; /* TODO: (this can be done in a func which find 00 field then insert new task) WARN: BE AWARE IF NEW KIND OF TASK ADD IN THE FUTURE, THIS WILL BE A MONSTER */
                wifi.isDataChanged++; /* force sent loc to get a speed at 30 sec */
            }
            else
                return;            
            break;
        case 3:
            if (uart.Tstate != IDLE) /* wait the application of speed from PC */
                return;
            escalator.mode = AUTO_SPEED;
            break;
        default:
            return; /* nothing processed */
    }
    taskUpdate();
}

void taskHandler(void)
{
    if (escalator.mode == AUTO_SPEED && uart.Tstate == IDLE)
    {
        /* may add task if there is no task */
        if (!escalator.queueTask) /* no task remained, we add task here */
        {             
            escalator.queueTask = 0x25; /* 3 tasks, which 10 01 01 stands for MANUAL_SPEED, autoSpeed and autoSpeed respectively */
            escalator.autoSpeedTick = mcu.sysTick + 10000;
        }
    }
    taskProcess();
}

void ADC_enableAutoScan(scan_t* pbuffer, uint8_t numElements)
{
    uint16_t addr = (uint16_t) pbuffer; /* WARN: this buffer address which is adc_buf's address should be even-numbered since it use two bytes everytime */
    uint8_t ADCM_save;
    uint8_t savedPage = SFRPAGE;
    SFRPAGE = 0x30;
    
    ADCM_save = ADC0CN2 & ADC0CN2_ADCM__FMASK;
	ADC0CN2 = (ADC0CN2 & ~ADC0CN2_ADCM__FMASK) | ADC0CN2_ADCM__ADBUSY;

    ADC0ASAH = (ADC0ASAH & ~ADC0ASAH_STADDRH__FMASK) | ((addr >> 8) & ADC0ASAH_STADDRH__FMASK);
	ADC0ASAL = (ADC0ASAL & ~ADC0ASAL_STADDRL__FMASK) | (addr & ADC0ASAL_STADDRL__FMASK);

    numElements--; /* array-like index use, ex buffer size is an array which has size 5 bytes, then the index should be array[0] ~ array[4] */
    ADC0ASCT = (ADC0ASCT & ~ADC0ASCT_ASCNT__FMASK) | numElements;

    ADC0ASCF |= ADC0ASCF_ASEN__START_SCAN;

    SFRPAGE = savedPage;
}

void ADC_setAutoScanInputs(uint8_t startChannel, uint8_t numChannels)
{
    uint8_t endChannel, savedPage;
    savedPage = SFRPAGE;
    SFRPAGE = 0x30;

    numChannels--; /* like array index, start from 0 */
    ADC0ASCF = (ADC0ASCF & ~ADC0ASCF_NASCH__FMASK) | numChannels;

    endChannel = startChannel + numChannels;
    ADC0CN0_TEMPE = 0;

    ADC0MX = startChannel; /* indicates which to start with raw number specified in spec, check spec for more details */

    SFRPAGE = savedPage;

}

void Init(void)
{
    uint8_t index;
    uint8_t SFRPAGE_save = SFRPAGE; /* save SFRPAGE */
    /* WDT disable ------------------*/
    SFRPAGE = 0x00;
    WDTCN = 0xDE; //First key (use this write sequence to disable the WDT)
	WDTCN = 0xAD; //Second key
    /*-------------------------------*/
    /* SYSCLK select ----------------*/
    CLKSEL = CLKSEL_CLKSL__HFOSC0 | CLKSEL_CLKDIV__SYSCLK_DIV_1; // SYSCLK = 24 Mhz divided by 1.
    while (CLKSEL & CLKSEL_DIVRDY__BMASK == CLKSEL_DIVRDY__NOT_READY);
    /*-------------------------------*/

    /* GPIO setting -----------------*/ // WARN: any change of pinout should check out this context
    P0MASK = P0MASK_B0__IGNORED  | P0MASK_B1__IGNORED | P0MASK_B2__IGNORED
			| P0MASK_B3__IGNORED | P0MASK_B4__IGNORED | P0MASK_B5__IGNORED
			| P0MASK_B6__IGNORED | P0MASK_B7__IGNORED;

    P0MDOUT = P0MDOUT_B0__OPEN_DRAIN | P0MDOUT_B1__OPEN_DRAIN
			| P0MDOUT_B2__OPEN_DRAIN | P0MDOUT_B3__OPEN_DRAIN
			| P0MDOUT_B4__PUSH_PULL  | P0MDOUT_B5__OPEN_DRAIN
			| P0MDOUT_B6__OPEN_DRAIN | P0MDOUT_B7__OPEN_DRAIN;

    P0MDIN = P0MDIN_B0__DIGITAL  | P0MDIN_B1__DIGITAL | P0MDIN_B2__DIGITAL
			| P0MDIN_B3__DIGITAL | P0MDIN_B4__DIGITAL | P0MDIN_B5__DIGITAL
			| P0MDIN_B6__DIGITAL | P0MDIN_B7__DIGITAL;

    P0SKIP = P0SKIP_B0__SKIPPED | P0SKIP_B1__SKIPPED
			| P0SKIP_B2__SKIPPED | P0SKIP_B3__SKIPPED
			| P0SKIP_B4__NOT_SKIPPED | P0SKIP_B5__NOT_SKIPPED
			| P0SKIP_B6__SKIPPED | P0SKIP_B7__SKIPPED;

    
    P1MASK = P1MASK_B0__IGNORED  | P1MASK_B1__IGNORED | P1MASK_B2__IGNORED
			| P1MASK_B3__IGNORED | P1MASK_B4__IGNORED | P1MASK_B5__IGNORED
			| P1MASK_B6__IGNORED | P1MASK_B7__IGNORED;

    P1MDOUT = P1MDOUT_B0__OPEN_DRAIN | P1MDOUT_B1__OPEN_DRAIN
			| P1MDOUT_B2__OPEN_DRAIN | P1MDOUT_B3__OPEN_DRAIN
			| P1MDOUT_B4__PUSH_PULL | P1MDOUT_B5__OPEN_DRAIN
			| P1MDOUT_B6__OPEN_DRAIN | P1MDOUT_B7__OPEN_DRAIN;

    P1MDIN = P1MDIN_B0__DIGITAL  | P1MDIN_B1__ANALOG | P1MDIN_B2__ANALOG
			| P1MDIN_B3__ANALOG | P1MDIN_B4__DIGITAL | P1MDIN_B5__DIGITAL
			| P1MDIN_B6__DIGITAL | P1MDIN_B7__DIGITAL;

    P1SKIP = P1SKIP_B0__SKIPPED  | P1SKIP_B1__SKIPPED
			| P1SKIP_B2__SKIPPED | P1SKIP_B3__SKIPPED
			| P1SKIP_B4__SKIPPED | P1SKIP_B5__SKIPPED
			| P1SKIP_B6__SKIPPED | P1SKIP_B7__SKIPPED;


    P2MDOUT = P2MDOUT_B0__OPEN_DRAIN | P2MDOUT_B1__OPEN_DRAIN
			| P2MDOUT_B2__OPEN_DRAIN | P2MDOUT_B3__OPEN_DRAIN
			| P2MDOUT_B4__OPEN_DRAIN | P2MDOUT_B5__OPEN_DRAIN
			| P2MDOUT_B6__OPEN_DRAIN;

    SFRPAGE = 0x20;
    P2SKIP = P2SKIP_B0__SKIPPED | P2SKIP_B1__SKIPPED
			| P2SKIP_B2__SKIPPED | P2SKIP_B3__SKIPPED;


    SFRPAGE = 0x00;
    XBR2 = XBR2_WEAKPUD__PULL_UPS_ENABLED | XBR2_XBARE__ENABLED
			| XBR2_URT1E__DISABLED | XBR2_URT1RTSE__DISABLED
			| XBR2_URT1CTSE__DISABLED;

    XBR0 = XBR0_URT0E__ENABLED | XBR0_SPI0E__DISABLED | XBR0_SMB0E__DISABLED
			| XBR0_CP0E__DISABLED | XBR0_CP0AE__DISABLED | XBR0_CP1E__DISABLED
			| XBR0_CP1AE__DISABLED | XBR0_SYSCKE__DISABLED;

    /*-------------------------------*/

    /* Timer setting ----------------*/
    TCON &= ~TCON_TR0__BMASK & ~TCON_TR1__BMASK;    
    TH1 = (/*0xe7*/0x96 << TH1_TH1__SHIFT);

    CKCON0 = CKCON0_SCA__SYSCLK_DIV_12 | CKCON0_T0M__PRESCALE
			| CKCON0_T2MH__EXTERNAL_CLOCK | CKCON0_T2ML__EXTERNAL_CLOCK
			| CKCON0_T3MH__EXTERNAL_CLOCK | CKCON0_T3ML__EXTERNAL_CLOCK
			| CKCON0_T1M__SYSCLK;
            
    TMOD = TMOD_T0M__MODE0 | TMOD_T1M__MODE2 | TMOD_CT0__TIMER
			| TMOD_GATE0__DISABLED | TMOD_CT1__TIMER | TMOD_GATE1__DISABLED;

    TCON |= TCON_TR1__RUN;

    /* with current SYSCLK and timer2 setting, tick of timer is 0.4897us, and with 63493 set to timer2's H and L byte we get a timer which interrupt interval is 1 ms */
    
    TMR2H = 0xf8; /* may be this is not necessary */
    TMR2L = 0x05; /* may be this is not necessary */
    TMR2RLH = 0xf8;
    TMR2RLL = 0x05;
    TMR2CN0 = 0x04; 
    IP |= 0x20;
    IPH |= 0x20;
    TMR2CN1 = 0x00;
    /*-------------------------------*/

    /* ADC setting -----------------*/
    ADC0CN2 &= ~0x8f;
    ADC0CN1 = 0x00;
    ADC0CF2 = 0x60;
    ADC0CF0 = 0x08;
    ADC0CF1 = 0x3f;
    SFRPAGE = 0x30;
    ADC0ASCF = 0x40;
    SFRPAGE = 0x00;
    ADC0CN0 &= ~0x60;
    ADC0CN0 |= 0x80;
    /*-------------------------------*/

    /* DAC setting ------------------*/
    
    SFRPAGE = 0x20; 
    P3MDOUT = P3MDOUT_B0__OPEN_DRAIN | P3MDOUT_B1__OPEN_DRAIN
			| P3MDOUT_B2__OPEN_DRAIN | P3MDOUT_B3__OPEN_DRAIN
			| P3MDOUT_B4__PUSH_PULL | P3MDOUT_B7__OPEN_DRAIN;
    
    SFRPAGE = 0x30;
    DACGCF0 = 0x88;
    DACGCF1 |= 0x08;
    DACGCF1 &= ~0x07;
    DACGCF2 &= ~0x33;
    DAC0CF0 = 0x80; /* last should have TODO which makes system more efficient  */
    DAC0CF1 = 0x00;
    DAC1CF0 = 0x80;
    DAC1CF1 = 0x00;
    DAC2CF0 = 0x80;
    DAC2CF1 = 0x00;
    
    /*-------------------------------*/

    /* Uart setting -----------------*/
    // inside the timer setting
    /*-------------------------------*/

    /* Struct member ----------------*/
    mcu.sysTick = 0;

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
    
    SFRPAGE = PG4_PAGE;
    DAC0L = 0x0;
	DAC0H = 0x00;
	DAC1L = 0x0;
	DAC1H = 0x00;
	DAC2L = 0x0;
	DAC2H = 0x00;
    /*-------------------------------*/

    /* Interrupt setting-------------*/
    IE = IE_EA__DISABLED | IE_EX0__DISABLED | IE_EX1__DISABLED
		    | IE_ESPI0__DISABLED | IE_ET0__DISABLED | IE_ET1__DISABLED
			| IE_ET2__ENABLED | IE_ES0__ENABLED;
    SFRPAGE = 0x00;
    EIE1 &= ~0xf7;
    EIE1 |= EIE1_EADC0__ENABLED;
    /*-------------------------------*/
    SFRPAGE = SFRPAGE_save; /* restore SFRPAGE */
}

SI_INTERRUPT (ADC0EOC_ISR, ADC0EOC_IRQn)
{  
    uint8_t savedPage;

    /*
    if ((debugTick + 1000) < mcu.sysTick)
    {
        LED0 = ~LED0;
        debugTick = mcu.sysTick;
    }
    */

    savedPage = SFRPAGE;
    SFRPAGE = 0x30; /* ADC0 sfr page */
    ADC0CN0 &= ~0x20; /* this bit is ADINT */
    SFRPAGE = savedPage;
    CONVERSION_COMPLETE = 1;

}





/*************flawless0714 * END OF FILE****/
