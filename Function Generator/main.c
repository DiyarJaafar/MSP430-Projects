#include <msp430.h>
#include "peripherals.h"


int getButton1();
int getButton2();
int getButton3();
int getButton4();
char getButtons();
void configButtons();
void configTimer(int maxCount, int clk);
void DACInit(void);
void DACSetValue(unsigned int dac_code);
void configADC();
void DC();
void squareWave();
void sawtoothWave();
void triangleWave();
void printMessage(char *msg, int height);
void refreshScreen();
void clearScreen();
void refreshScrollWheel();

long int timer = 0;
int scrollWheel = 0;
int sawtoothSteps = 16;
int triangleSteps = 8;
int state = 4;

void setState(int inState) {
    state = inState;
    TA2CCR0 = 0;
    timer = 0;
}

void config2Buttons() {
    // Setup buttons as Digital I/O
    P1SEL &= ~(BIT1);
    P2SEL &= ~(BIT1);
    // Setup buttons as inputs
    P1DIR &= ~(BIT1);
    P2DIR &= ~(BIT1);
    // Setup pull-down res ?
    P1REN |= (BIT1);
    P2REN |= (BIT1);

    P1OUT |= (BIT1);
    P2OUT |= (BIT1);
}

int main(void) {
    WDTCTL = WDTPW + WDTHOLD; // Stop WDT
    DACInit();
    configADC();
    configDisplay();
    configButtons();
    config2Buttons();

    clearScreen();
    printMessage("1 = DC", 20);
    printMessage("2 = Square Wave", 30);
    printMessage("3 = Sawtooth Wave", 40);
    printMessage("4 = Triangle Wave", 50);
    refreshScreen();

//    DACSetValue(0);
//    return;

    while (1) {
        if      (getButton1()) state = 1;
        else if (getButton2()) state = 2;
        else if (getButton3()) state = 3;
        else if (getButton4()) state = 4;
        else state = 0;
        switch(state) {
            case 0: {
                DACSetValue(0);
                break;
            }
            case 1: {
                configTimer(32767, 1); // 1s
                break;
            }
            case 2: {
                configTimer(162, 1); // 100 Hz
                break;
            }
            case 3: {
                float tint = (1.0 / 50.0) / (float)sawtoothSteps;
                float b = 32768 * tint;
                int maxCount = b - 1;
                configTimer(maxCount, 1);
                break;
            }
            case 4: {
//                float freq = (scrollWheel * (900.0 / 4095.0)) + 100.0;
                float tint = (1.0 / 50.0) / (float)triangleSteps;
                float b = 32768 * tint;
                int maxCount = b - 1;
                configTimer(maxCount, 1);
                break;
            }
        }

        while (TA2CCR0 != 0) {
            int cancel = !((P2IN & 0x2) >> 1);
            if (cancel) {
                setState(0);
                clearScreen();
                printMessage("1 = DC", 20);
                printMessage("2 = Square Wave", 30);
                printMessage("3 = Sawtooth Wave", 40);
                printMessage("4 = Triangle Wave", 50);
                refreshScreen();
                break;
            }

            switch(state) {
                case 1: {
                    DC();
                    break;
                }
                case 2: {
                    squareWave();
                    break;
                }
                case 3: {
                    sawtoothWave();
                    break;
                }
                case 4: {
                    triangleWave();
                    break;
                }
            }
        }

    }
}

void clearScreen() {
    Graphics_clearDisplay(&g_sContext); // Clear the display
}

void refreshScreen() {
    Graphics_flushBuffer(&g_sContext);

}

void printMessage(char *msg, int height) {
    Graphics_drawStringCentered(&g_sContext, msg, AUTO_STRING_LENGTH, 48, height, TRANSPARENT_TEXT);
}

void configTimer(int maxCount, int clk) {
    if (clk == 1) {
        TA2CTL = TASSEL_1 + MC_1 + ID_0;
    }
    if (clk == 2) {
        TA2CTL = TASSEL_2 + MC_1 + ID_0;
    }
    TA2CCR0 = maxCount;
    TA2CCTL0 = CCIE;
    _BIS_SR(GIE);
}

void refreshScrollWheel() {
    ADC12CTL0 &= ~ADC12SC;      // clear the start bit
    ADC12CTL0 |= ADC12SC;               // Sampling and conversion start
    // Poll busy bit waiting for conversion to complete
    while (ADC12CTL1 & ADC12BUSY)
        __no_operation();
    scrollWheel = ADC12MEM0;               // Read results if conversion done
}

void DC() {
    refreshScrollWheel();
//    DACSetValue(scrollWheel);

    int value = scrollWheel;
    DACSetValue(value);
    unsigned int input = ADC12MEM1 & 0x0FFF; // Read results from conversion
    clearScreen();
    char v[] = "DAC: xxxx";
    v[5] = ((value / 1000) % 10) + 0x30;
    v[6] = ((value / 100) % 10) + 0x30;
    v[7] = ((value / 10) % 10) + 0x30;
    v[8] = ((value / 1) % 10) + 0x30;
    printMessage(v, 45);
    char v2[] = "ADC: xxxx";
    v2[5] = ((input / 1000) % 10) + 0x30;
    v2[6] = ((input / 100) % 10) + 0x30;
    v2[7] = ((input / 10) % 10) + 0x30;
    v2[8] = ((input / 1) % 10) + 0x30;
    printMessage(v2, 60);
    refreshScreen();
    return;
}

void squareWave() {
    refreshScrollWheel();
    if (timer % 2 == 0) {
        DACSetValue(0);
    } else {
        DACSetValue(scrollWheel);
    }
}

void sawtoothWave() {
    int stepN = timer % sawtoothSteps;
    int stepV = 4095 / sawtoothSteps;
    DACSetValue(stepV * stepN);
}

void triangleWave() {
    int stepV = 4095 / (triangleSteps / 2);
    int res = timer % (triangleSteps / 2);
    if (timer % triangleSteps < (triangleSteps / 2)) {
        DACSetValue(stepV * res);
    } else {
        int step = (triangleSteps / 2) - res;
        DACSetValue(stepV * step);
    }
}

#pragma vector=TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    timer++;
}

char getButtons() {
    //
    char p0 = ((~P7IN) & BIT0) << 3; // S1         000x -> x000
    char p4 = ((~P7IN) & BIT4) >> 4; // S4     x 0000 -> 000x
    char p6 = ((~P3IN) & BIT6) >> 4; // S2   x00 0000 -> 0x00
    char p2 = ((~P2IN) & BIT2) >> 1; //S3          0x00 -> 00x0
    char state = p0 | p4 | p6 | p2;
    return state;
}

int getButton1() {
    return getButtons() & 0x8;
}

int getButton2() {
    return getButtons() & 0x4;
}

int getButton3() {
    return getButtons() & 0x2;
}

int getButton4() {
    return getButtons() & 0x1;
}

void configButtons() {
    // Setup buttons as Digital I/O
    P7SEL &= ~(BIT0 | BIT4);
    P3SEL &= ~(BIT6);
    P2SEL &= ~(BIT2);
    // Setup buttons as inputs
    P7DIR &= ~(BIT0 | BIT4);
    P3DIR &= ~(BIT6);
    P2DIR &= ~(BIT2);
    // Setup pull-down res ?
    P7REN |= (BIT0 | BIT4);
    P3REN |= (BIT6);
    P2REN |= (BIT2);

    P7OUT |= (BIT0 | BIT4);
    P3OUT |= (BIT6);
    P2OUT |= (BIT2);
}

void configADC() {
    P8SEL &= ~BIT0;
    P8DIR |= BIT0;
    P8OUT |= BIT0;
    // Reset REFMSTR to hand over control of internal reference
    // voltages to ADC12_A control registers
    REFCTL0 &= ~REFMSTR;
    // Internal ref is on and set to 1.5V
    ADC12CTL0 = ADC12SHT0_9 | ADC12SHT1_9 | ADC12REFON | ADC12ON | ADC12MSC | ADC12REF2_5V;
    ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1; // Enable sample timer
    ADC12MCTL1 = ADC12SREF_1 + ADC12INCH_1 + ADC12EOS; // A1
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0;   // ADC12INCH5 = Scroll wheel = A0
    __delay_cycles(100); // delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC;            // Enable conversion
}


void DACInit(void) {
    // Configure LDAC and CS for digital IO outputs
    DAC_PORT_LDAC_SEL &= ~DAC_PIN_LDAC;
    DAC_PORT_LDAC_DIR |= DAC_PIN_LDAC;
    DAC_PORT_LDAC_OUT |= DAC_PIN_LDAC; // Deassert LDAC
    DAC_PORT_CS_SEL &= ~DAC_PIN_CS;
    DAC_PORT_CS_DIR |= DAC_PIN_CS;
    DAC_PORT_CS_OUT |= DAC_PIN_CS; // Deassert CS
}

void DACSetValue(unsigned int dac_code) {
    // Start the SPI transmission by asserting CS (active low)
    // This assumes DACInit() already called
    DAC_PORT_CS_OUT &= ~DAC_PIN_CS;
    // Write in DAC configuration bits. From DAC data sheet
    // 3h=0011 to highest nibble.
    // 0=DACA, 0=buffered, 1=Gain=1, 1=Out Enbl
    dac_code |= 0x3000; // Add control bits to DAC word
    uint8_t lo_byte = (unsigned char)(dac_code & 0x00FF);
    uint8_t hi_byte = (unsigned char)((dac_code & 0xFF00) >> 8);
    // First, send the high byte
    DAC_SPI_REG_TXBUF = hi_byte;
    // Wait for the SPI peripheral to finish transmitting
    while(!(DAC_SPI_REG_IFG & UCTXIFG)) {
        _no_operation();
    }
    // Then send the low byte
    DAC_SPI_REG_TXBUF = lo_byte;
    // Wait for the SPI peripheral to finish transmitting
    while(!(DAC_SPI_REG_IFG & UCTXIFG)) {
        _no_operation();
    }
    // We are done transmitting, so de-assert CS (set = 1)
    DAC_PORT_CS_OUT |= DAC_PIN_CS;
    // This DAC is designed such that the code we send does not
    // take effect on the output until we toggle the LDAC pin.
    // This is because the DAC has multiple outputs. This design
    // enables a user to send voltage codes to each output and
    // have them all take effect at the same time.
    DAC_PORT_LDAC_OUT &= ~DAC_PIN_LDAC; // Assert LDAC
    __delay_cycles(10); // small delay
    DAC_PORT_LDAC_OUT |= DAC_PIN_LDAC; // De-assert LDAC
}
