/************** ECE2049 LAB 2 CODE ******************/
/**************  25 August 2021    ******************/
/****************************************************/

#include <msp430.h>

/* Peripherals.c and .h are where the functions that implement
 * the LEDs and keypad, etc are. It is often useful to organize
 * your code by putting like functions together in files.
 * You include the header associated with that file(s)
 * into the main file of your project. */
#include "peripherals.h"

// Function Prototypes
void swDelay(char numLoops);
void configButtons();
char getButtons();
void lightLEDs(char state);
void lightFourLEDs(char state);
void playNote(int frequency, int duration);
void printMessage(char *msg, int height);
void countdown();
void playGame();
void BuzzerOnFreq(int frequencyNote);
void playGame();
void waitSeconds(int t);

int notesN = 28;
int notesDuration[] = { 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int song[] = {523, 587, 622, 523, 587, 698, 622, 523, 587, 622, 587, 523, 523, 587, 622, 523, 587, 698, 622, 523, 587, 622, 587, 523, 740, 784, 831, 880};
char noteToLED(int note) {
    switch(note) {
        case 523: { return 1; } // 0001
        case 587: { return 2; } // 0010
        case 622: { return 4; } // 0100
        case 698: { return 8; } // 1000
        case 740: { return 1; } // 0010
        case 784: { return 2; } // 0010
        case 831: { return 4; } // 0010
        case 880: { return 8; } // 0010
        default:  { return 0; } // 0000
    }
}

unsigned int state = 0; // 0: reset, 1: ready, 2: playing
int countdownCounter = 3;
unsigned long long int toneTimer = 0;
int noteIdx = 0;
int mistakes = 0, currentMistake = 0;


// Main
void main(void) {
    WDTCTL = WDTPW | WDTHOLD;    // Stop watchdog timer. Always need to stop this!!
                                 // You can then configure it properly, if desired

    // Useful code starts here
    initLeds();

    configDisplay();
    configKeypad();
    configButtons();
    while (1) {                                                                     // Tint = maxCount + 1 * 1/timerFreq
        // State of game
        switch(state) {
            case 0: {
                Graphics_clearDisplay(&g_sContext); // Clear the display
                Graphics_drawStringCentered(&g_sContext, "BUZZER HERO", AUTO_STRING_LENGTH, 48, 30, TRANSPARENT_TEXT);
                Graphics_drawStringCentered(&g_sContext, "Press * to begin", AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
                Graphics_flushBuffer(&g_sContext);
                state = 1;
                break;
            }
            case 1: {
                if (getKey() == '*') {
                    countdown();
                    state = 2;
                }
                break;
            }
            case 2: {
                playGame();
                break;
            }
            case 3: {  // Winning state
                printMessage("You won!!!", 1);
                waitSeconds(1);
                state = 0;
                break;
            }
            case 4: { // Losing state
                printMessage("You lost!!!", 1);
                waitSeconds(1);
                state = 0;
                break;
            }
            default:
                 break;
        }
    }
}

void exitGame() {
    TA2CCR0 = 0;
    noteIdx = 0;
    toneTimer = 0;
    mistakes = 0;
    currentMistake = 0;
    BuzzerOff();
    setLeds(0x0);
    lightLEDs(0);
}

void playGame() {
    TA2CTL = TASSEL_1 + ID_0 + MC_1;
    // 32768 = maxCount + 1
    TA2CCR0 = 163; // (maxCount + 1) * 1/freq = 1 => ~0.005s
    TA2CCTL0 = CCIE;
    _BIS_SR(GIE);
    while (TA2CCR0 != 0) {
        if (getKey() == '#') {
            exitGame();
            state = 0; // Home screen
            return;
        }
        char key = getKey();
        if (key < 0x30 || key > 0x39) continue;
    }
    if (state == 4) { return; }
    state = 3; // Winning
}

void countdown() {
    countdownCounter = 3;
    TA2CTL = TASSEL_1 + ID_0 + MC_1;
    // 32768 = maxCount + 1
    TA2CCR0 = 32767; // (maxCount + 1) * 1/freq = 1
    TA2CCTL0 = CCIE;
    _BIS_SR(GIE);
    while (TA2CCR0 != 0) { continue; }
    lightLEDs(0);
}

void countdownCallback() {
    if (countdownCounter == 0) {
        lightLEDs(0x03);
        printMessage("GO", 1);
        TA2CCR0 = 0;
        countdownCounter--;
        return;
    }
    char countdownString[] = {'x', '\0'};
    countdownString[0] = countdownCounter + 0x30;
    printMessage(countdownString, 1);
    if (countdownCounter == 1 || countdownCounter == 3) lightLEDs(0x01);
    if (countdownCounter == 2) lightLEDs(0x02);
    countdownCounter--;
}

void toneCallback() {
    if (mistakes >= 5) {
        state = 4; // Losing
        exitGame();
        return;
    }
    if (noteIdx == notesN) {
        exitGame();
        return;
    }
    if (notesDuration[noteIdx] * 150 == toneTimer) {
        toneTimer = 0;
        noteIdx += 1;
    }
    char noteID = noteToLED(song[noteIdx]);
    if (toneTimer == 0) {
        setLeds(noteID);
        BuzzerOnFreq(song[noteIdx]);
        mistakes += currentMistake; //
        currentMistake = 1;
        char m[] = "Lives left: x";
        m[12] = (5 - mistakes) + 0x30;
        printMessage(m, 1);
    }
    char btn = getButtons();
    if (btn == noteID) { // Not missed
        currentMistake = 0;  // 1 -> 0  -> 1
        lightLEDs(2);
    } else { // Missed
        lightLEDs(1);
    }
    toneTimer += 1;
}


void waitSeconds(int t) {
    countdownCounter = 3;
    TA2CTL = TASSEL_1 + ID_0 + MC_1;
    // 32768 = maxCount + 1       t = (maxcount + 1) * 1/f => f * t - 1
    TA2CCR0 = (32768 * t) - 1; // (maxCount + 1) * 1/freq = 1
    TA2CCTL0 = CCIE;
    _BIS_SR(GIE);
    while (TA2CCR0 != 0) { continue; }

}

#pragma vector=TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    // Don't run ISR if timer is cancelled - prevents already scheduled runs
    if (TA2CCR0 == 0) { return; }
    if (state == 2) {
        toneCallback();
    } else if (state == 1){
        countdownCallback();
    } else {
        TA2CCR0 = 0;
    }
}

void printMessage(char *msg, int height) {
    Graphics_clearDisplay(&g_sContext); // Clear the display
    Graphics_drawStringCentered(&g_sContext, msg, AUTO_STRING_LENGTH, 48, height * 35, TRANSPARENT_TEXT);
    Graphics_flushBuffer(&g_sContext);
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

char getButtons() {
    //
    char p0 = ((~P7IN) & BIT0) << 3; // S1         000x -> x000
    char p4 = ((~P7IN) & BIT4) >> 4; // S4     x 0000 -> 000x
    char p6 = ((~P3IN) & BIT6) >> 4; // S2   x00 0000 -> 0x00
    char p2 = ((~P2IN) & BIT2) >> 1; //S3          0x00 -> 00x0
    char state = p0 | p4 | p6 | p2;
    return state;
}

void lightLEDs(char state) {
    P1SEL &= ~BIT0; // Red
    P1DIR |= BIT0;
    P4SEL &= ~BIT7; // Green
    P4DIR |= BIT7;

    if (state & BIT0)
        // Turn on LED 1 on 1.0
        P1OUT |= BIT0; // xxxx xxx0 => 0000 0000
    else
        P1OUT &= ~BIT0;

    if (state & BIT1)
        P4OUT |= BIT7;
    else
        P4OUT &= ~BIT7;
}


void BuzzerOnFreq(int frequencyNote)
{
    // Initialize PWM output on P3.5, which corresponds to TB0.5
    P3SEL |= BIT5; // Select peripheral output mode for P3.5
    P3DIR |= BIT5;

    TB0CTL  = (TBSSEL__ACLK|ID__1|MC__UP);  // Configure Timer B0 to use ACLK, divide by 1, up mode
    TB0CTL  &= ~TBIE;                       // Explicitly Disable timer interrupts for safety

    // Now configure the timer period, which controls the PWM period
    // Doing this with a hard coded values is NOT the best method
    // We do it here only as an example. You will fix this in Lab 2.
    float periodNote = 1.0 / frequencyNote;
    int maxCount = (periodNote * 32768.0) - 1;
    TB0CCR0   = maxCount;                    // Set the PWM period in ACLK ticks
    TB0CCTL0 &= ~CCIE;                  // Disable timer interrupts

    // Configure CC register 5, which is connected to our PWM pin TB0.5
    TB0CCTL5  = OUTMOD_7;                   // Set/reset mode for PWM
    TB0CCTL5 &= ~CCIE;                      // Disable capture/compare interrupts
    TB0CCR5   = TB0CCR0/2;                  // Configure a 50% duty cycle
}
