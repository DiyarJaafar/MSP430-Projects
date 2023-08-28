#include <msp430.h>
#include "peripherals.h"

#define CALADC12_25V_30C *((unsigned int *)0x1A22)
#define CALADC12_25V_85C *((unsigned int *)0x1A24)

// Function Prototypes
void startClock();
void displayInfo(unsigned long long time, float temp);
const char* joinDate(char* month, int day);
void displayTime();
void displayDate();
int* ddToMMM(int dd);
void configADC();
void displayTempF(float inAvgTempC);
void displayTempC(float inAvgTempC);
void configButtons();
int daysInMonth(int month);

long int currentTime = 24055756;
int temperatures[30];
unsigned int scrollWheel = 0;
//long int increments[5] = {0, 0, 0, 0, 0}; // MMM-DD hh:mm:ss
int timestamp[5] = {0, 0, 0, 0, 0};
// Main
void main(void) {
    WDTCTL = WDTPW | WDTHOLD;    // Stop watchdog timer. Always need to stop this!!
                                 // You can then configure it properly, if desired

    initLeds();

    configDisplay();
    configKeypad();
    configADC();
    configButtons();

    int i = 0;
    for (i = 0; i < 30; i++)
        temperatures[i] = 0;

    startClock();
}

void configButtons() {
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
    ADC12MCTL1 = ADC12SREF_1 + ADC12INCH_10 + ADC12EOS;
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0;   // ADC12INCH5 = Scroll wheel = A0
    __delay_cycles(100); // delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC;            // Enable conversion
}


float avgTemp() {
    int i = 0;
    float sum = 0;
    for (i = 0; i < 30; i++)
        sum += temperatures[i];
    return sum / 30;
}

void startClock() {
    TA2CTL = TASSEL_1 + ID_0 + MC_1;
    TA2CCR0 = 32768;
    TA2CCTL0 = CCIE;
    _BIS_SR(GIE);
    while (TA2CCR0 != 0) { continue; }
}

void getTemp(void) {
    volatile float temperatureDegC;
    volatile float temperatureDegF;
    volatile float degC_per_bit;
    float dx = CALADC12_25V_85C - CALADC12_25V_30C;
    degC_per_bit = 55.0 / dx;
    // Single conversion (single channel)
    // Poll busy bit waiting for conversion to complete
    unsigned int in_temp = ADC12MEM1 & 0x0FFF; // Read results from conversion
    temperatureDegC = (float)(((long)in_temp-CALADC12_25V_30C) * degC_per_bit + 30.0);
    temperatures[currentTime % 30] = temperatureDegC;
}

int state = 0; // 0=date, 1=time, 2=tempc, 3=tempf
int isEditing = 0, editField = 0;

#pragma vector=TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    ADC12CTL0 &= ~ADC12SC;      // clear the start bit
    ADC12CTL0 |= ADC12SC;               // Sampling and conversion start
    while (ADC12CTL1 & ADC12BUSY)
        __no_operation();

    getTemp();
    scrollWheel = ADC12MEM0 & 0x0FFF; // Read results if conversion done
    float MAX_ADC_VAL = 4095.0;
    int editRequest = (P2IN & 0x2) >> 1;
    if (!editRequest) {
        if (isEditing == 0) {
            state = 0; // Go back to MMM-DD view
            editField = 0; // Edit MMM field
            isEditing = 1; // Set edit mode active
        } else {
            editField++;
            if (editField == 2) // HH
                state = 1;
            if (editField == 5) { // Go back
                editField = 0;
                state = 0;
            }
        }
    }
    int confirmRequest = (P1IN & 0x2) >> 1;
    if (!confirmRequest) {
        long int newTimestamp = 0;
        int i = 0;
        long int totDays = 0;
        for (i = 0; i < timestamp[0]; i++) {
            totDays += daysInMonth(i);
        }
        timestamp[1] -= 1;
        newTimestamp += timestamp[4]; // Seconds
        newTimestamp += (long int)timestamp[3] * (long int)60; // Minutes
        newTimestamp += (long int)timestamp[2] * (long int)3600; // Hours
        newTimestamp += (long int)timestamp[1] * (long int)86400; // Days
        newTimestamp += (long int)totDays * (long int)86400;
        currentTime = newTimestamp;
        isEditing = 0;
    }

    if (!isEditing) {
        currentTime++;
        long int inTime = currentTime;
        int dd = inTime / 86400;
        inTime = inTime % 86400;
        timestamp[2] = inTime / 3600;
        inTime = inTime % 3600;
        timestamp[3] = inTime / 60;
        timestamp[4] = inTime % 60;
        int *mmmdd = ddToMMM(dd);
        timestamp[0] = mmmdd[0];
        timestamp[1] = mmmdd[1];
    }

    Graphics_clearDisplay(&g_sContext); // Clear the display

    int counter = currentTime % 3;
    if (isEditing == 0 && counter == 0) { state++; state = state % 4; }

    if (isEditing) {
        Graphics_drawStringCentered(&g_sContext, "Editing...", AUTO_STRING_LENGTH, 48, 50, TRANSPARENT_TEXT);
    }

    switch(state) {
        case 0: {
            if (isEditing && editField == 0) {
                Graphics_drawStringCentered(&g_sContext, "___   ", AUTO_STRING_LENGTH, 48, 40, TRANSPARENT_TEXT);
                timestamp[editField] = (scrollWheel / MAX_ADC_VAL) * 11;
            }
            if (isEditing && editField == 1) {
                Graphics_drawStringCentered(&g_sContext, "    __", AUTO_STRING_LENGTH, 48, 40, TRANSPARENT_TEXT);
                timestamp[editField] = (scrollWheel / MAX_ADC_VAL) * daysInMonth(timestamp[0]);
                if (timestamp[editField] == 0)
                    timestamp[editField] = 1;
            }
            displayDate();
            break;
        }
        case 1: {
            if (isEditing && editField == 2) {
                Graphics_drawStringCentered(&g_sContext, "__      ", AUTO_STRING_LENGTH, 48, 40, TRANSPARENT_TEXT);
                timestamp[editField] = (scrollWheel / MAX_ADC_VAL) * 23;
            }
            if (isEditing && editField == 3) {
                Graphics_drawStringCentered(&g_sContext, "   __   ", AUTO_STRING_LENGTH, 48, 40, TRANSPARENT_TEXT);
                timestamp[editField] = (scrollWheel / MAX_ADC_VAL) * 59;
            }
            if (isEditing && editField == 4) {
                Graphics_drawStringCentered(&g_sContext, "      __", AUTO_STRING_LENGTH, 48, 40, TRANSPARENT_TEXT);
                timestamp[editField] = (scrollWheel / MAX_ADC_VAL) * 59;
            }
            displayTime();
            break;
        }
        case 2: {
            float tempC = avgTemp();
            displayTempC(tempC);
            break;
        }
        case 3: {
            float tempC = avgTemp();
            displayTempF(tempC);
            break;
        }
    }
    Graphics_flushBuffer(&g_sContext);
}

void displayTempC(float inAvgTempC) {   // 32.5 - '32' '5'
    int dddC = inAvgTempC * 10; // 32
    int fC = (dddC % 10);
    dddC /= 10;
    char tempString[] = "CCC.C C";
    tempString[0] = ((dddC / 100) % 10) + 0x30;
    tempString[1] = ((dddC / 10) % 10) + 0x30;
    tempString[2] = (dddC % 10) + 0x30;
    tempString[4] = (fC % 10) + 0x30;
    // Write temp to the display
    Graphics_drawStringCentered(&g_sContext, tempString, AUTO_STRING_LENGTH, 48, 30, TRANSPARENT_TEXT);
}

void displayTempF(float inAvgTempC) {   // 32.5 - '32' '5'
    float inAvgTempF = (inAvgTempC * 9.0 / 5.0) + 32.0;
    int dddF = inAvgTempF * 10;
    int fF = dddF % 10;
    dddF /= 10;
    char tempString[] = "FFF.F F";

    tempString[0] = ((dddF / 100) % 10) + 0x30;
    tempString[1] = ((dddF / 10) % 10) + 0x30;
    tempString[2] = (dddF % 10) + 0x30;
    tempString[4] = (fF % 10) + 0x30;

    // Write temp to the display
    Graphics_drawStringCentered(&g_sContext, tempString, AUTO_STRING_LENGTH, 48, 30, TRANSPARENT_TEXT);
}


char* monthString(int m) {
    switch(m) {
        case 0:  { return "JAN"; }
        case 1:  { return "FEB"; }
        case 2:  { return "MAR"; }
        case 3:  { return "APR"; }
        case 4:  { return "MAY"; }
        case 5:  { return "JUN"; }
        case 6:  { return "JUL"; }
        case 7:  { return "AUG"; }
        case 8:  { return "SEP"; }
        case 9:  { return "OCT"; }
        case 10: { return "NOV"; }
        case 11: { return "DEC"; }
    }
    return 0;
}

void displayDate() {
    char* month = monthString(timestamp[0]);
    char mmmdd[] = "mmm-dd";
    mmmdd[0] = month[0];
    mmmdd[1] = month[1];
    mmmdd[2] = month[2];
    mmmdd[3] = '-';
    mmmdd[4] = (timestamp[1] / 10) % 10 + 0x30;
    mmmdd[5] = (timestamp[1]) % 10 + 0x30;
    // Write the date to the display
    Graphics_drawStringCentered(&g_sContext, mmmdd, AUTO_STRING_LENGTH, 48, 30, TRANSPARENT_TEXT);
}

void displayTime() {
    char hhmmss[] = "hh:mm:ss";
    hhmmss[0] = (timestamp[2] / 10) % 10 + 0x30;;
    hhmmss[1] = (timestamp[2]) % 10 + 0x30;;
    hhmmss[3] = (timestamp[3] / 10) % 10 + 0x30;;
    hhmmss[4] = (timestamp[3]) % 10 + 0x30;;
    hhmmss[6] = (timestamp[4] / 10) % 10 + 0x30;;
    hhmmss[7] = (timestamp[4]) % 10 + 0x30;;
    // Write the date to the display
    Graphics_drawStringCentered(&g_sContext, hhmmss, AUTO_STRING_LENGTH, 48, 30, TRANSPARENT_TEXT);
}

int* ddToMMM(int dd) {
    dd++;
    int mmm = 0;
    int day = 0;
    if (dd <= 31)               { mmm = 0; day = dd; }
    if (dd >= 32 && dd <= 59)   { mmm = 1; day = dd - 31; }
    if (dd >= 60 && dd <= 90)   { mmm = 2; day = dd - 59; }
    if (dd >= 91 && dd <= 120)  { mmm = 3; day = dd - 90; }
    if (dd >= 121 && dd <= 151) { mmm = 4; day = dd - 120; }
    if (dd >= 152 && dd <= 181) { mmm = 5; day = dd - 151; }
    if (dd >= 182 && dd <= 212) { mmm = 6; day = dd - 181; }
    if (dd >= 213 && dd <= 243) { mmm = 7; day = dd - 212; }
    if (dd >= 244 && dd <= 273) { mmm = 8; day = dd - 243; }
    if (dd >= 274 && dd <= 304) { mmm = 9; day = dd - 273; }
    if (dd >= 305 && dd <= 334) { mmm = 10; day = dd - 304; }
    if (dd >= 335 && dd <= 365) { mmm = 11; day = dd - 334; }
    static int x[2];
    x[0] = mmm;
    x[1] = day;
    return x;
}

int daysInMonth(int month) {
    if (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11)
        return 31;
    if (month == 10 || month == 3 || month == 5 || month == 8)
        return 30;
    if (month == 1)
        return 28;
    return 0;
}

int secondsInDay(int dd) {
    return 86400 * dd;
}
