/************** ECE2049 DEMO CODE ******************/
/**************  25 August 2021   ******************/
/***************************************************/

#include <stdlib.h>
#include <msp430.h>

/* Peripherals.c and .h are where the functions that implement
 * the LEDs and keypad, etc are. It is often useful to organize
 * your code by putting like functions together in files.
 * You include the header associated with that file(s)
 * into the main file of your project. */
#include "peripherals.h"

// Function Prototypes
void swDelay(char numLoops);
void countdown();
void reset();
int playGame(int level);
void setScreen(int screen[5][5]);
void bringDownGeo(int screen[5][5]);
void shoot(int column, int screen[5][5]);
int hasLost(int screen[5][5]);
int hasWon(int screen[5][5]);
void printMessage(char *msg, int delay);
void ledsAndBuzzer();
// Declare globals here

// Main
void main(void)

{
    WDTCTL = WDTPW | WDTHOLD;    // Stop watchdog timer. Always need to stop this!!
                                 // You can then configure it properly, if desired

    // Useful code starts here
    initLeds();

    configDisplay();
    configKeypad();

    reset();

    unsigned int state = 0; // 0: reset, 1: ready, 2: playing
    while (1) {
        // State of game
        switch(state) {
            case 0: {
                reset();
                state = 1;
                break;
            }
            case 1: {
                if (getKey() == '*') {
                    countdown();
                    state = 2;
                    swDelay(1);
                }
                break;
            }
            case 2: {
                int level, totLevels = 2;
                for (level = 1; level <= totLevels; level++) {
                    char levelStr[9] = {'L', 'e', 'v', 'e', 'l', ' ', 0x30 + level, '!', '\0'};
                    printMessage(levelStr, 5);
                    if (playGame(level) == 0) { // Lost
                        char levelStr[] = "GAME OVER";
                        printMessage(levelStr, 0);
                        ledsAndBuzzer();
                        break;
                    } else {
                        if (level == totLevels) {
                            char levelStr[] = "YOU WON";
                            printMessage(levelStr, 5);
                            break;
                        }
                    }
                }
                state = 0;
                break;
            }
            default:
                 break;
        }
    }
}

void ledsAndBuzzer() {
    BuzzerOn();
    int i = 0;
    for (i = 0; i < 6; i++) {
        P6OUT = P6OUT ^ BIT2;
        P6OUT = P6OUT ^ BIT1;
        P6OUT = P6OUT ^ BIT3;
        P6OUT = P6OUT ^ BIT4;
        swDelay(1);
    }
    BuzzerOff();
}

void printMessage(char *msg, int delay) {
    Graphics_clearDisplay(&g_sContext); // Clear the display
    Graphics_drawStringCentered(&g_sContext, msg, AUTO_STRING_LENGTH, 48, 35, TRANSPARENT_TEXT);
    Graphics_flushBuffer(&g_sContext);
    swDelay(delay);
}

void buzzerOnFr(int frequency)
{
    // Initialize PWM output on P3.5, which corresponds to TB0.5
    P3SEL |= BIT5; // Select peripheral output mode for P3.5
    P3DIR |= BIT5;

    TB0CTL  = (TBSSEL__ACLK|ID__1|MC__UP);  // Configure Timer B0 to use ACLK, divide by 1, up mode
    TB0CTL  &= ~TBIE;                       // Explicitly Disable timer interrupts for safety

    // Now configure the timer period, which controls the PWM period
    // Doing this with a hard coded values is NOT the best method
    // We do it here only as an example. You will fix this in Lab 2.
    TB0CCR0   = frequency;                    // Set the PWM period in ACLK ticks
    TB0CCTL0 &= ~CCIE;                  // Disable timer interrupts

    // Configure CC register 5, which is connected to our PWM pin TB0.5
    TB0CCTL5  = OUTMOD_7;                   // Set/reset mode for PWM
    TB0CCTL5 &= ~CCIE;                      // Disable capture/compare interrupts
    TB0CCR5   = TB0CCR0/2;                  // Configure a 50% duty cycle
}

int playGame(int level) {
    int screen[5][5] = { {0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0}};
    bringDown(screen, 1);
    setScreen(screen);
    int freq = 128;
    while (1) {
        long int i = 0;
        buzzerOnFr(freq);
        freq = 64 + (freq % 512);
        while (i < 10000 / level) {
            char keypress = getKey();
            if (keypress >= '1' && keypress <= '5') {
                int column = keypress - 0x30;
                shoot(column, screen);
                setScreen(screen);
            }
            i++;
        }
        if (hasWon(screen)) {
            BuzzerOff();
            return 1;
        }
        if (hasLost(screen)) {
            BuzzerOff();
            return 0;
        }
        bringDown(screen, 1);
        setScreen(screen);
    }
}

int hasWon(int screen[5][5]) {
    int r, c;
    for (r = 0; r < 5; r++) {
       for (c = 0; c < 5; c++) {
           if (screen[r][c] != 0) {
               return 0;
           }
       }
    }
    return 1;
}

int hasLost(int screen[5][5]) {
    int col;
    for (col = 0; col < 5; col++) {
        if (screen[4][col] != 0) {
            return true;
        }
    }
    return false;
}

void bringDown(int screen[5][5], int makeNew) {
    int row, col;
    for (row = 4; row >= 1; row--) {
        for (col = 0; col < 5; col++) {
            screen[row][col] = screen[row - 1][col];
        }
    }
    for (col = 0; col < 5; col++) {
        if (makeNew) {
//            screen[0][col] = (col+1) * (rand() % 2);
            screen[0][col] = 0x1e9 * (rand() % 2);
        } else {
            screen[0][col] = 0;
        }
    }
}

void bringDownGeo(int screen[5][5]) {
    int rowN;
    for (rowN = 0; rowN < 1; rowN++) {
        int row, col;
        for (row = 4; row >= 1; row--) {
            for (col = 0; col < 5; col++) {
                screen[row][col] = screen[row - 1][col];
            }
        }
    }
    int col;
    for (col = 0; col < 5; col++) {
        screen[0][col] = 0;
        screen[1][col] = 0;
    }
    int shape = rand() % 2; // 0 is triangle, 1 is square
    int originColumn = rand() % 4;
    if (shape == 1) {
        screen[0][originColumn] = originColumn + 1;
        screen[0][originColumn + 1] = originColumn + 2;
        screen[1][originColumn] = originColumn + 1;
        screen[1][originColumn + 1] = originColumn + 2;
    } else {
        screen[0][originColumn] = originColumn + 1;
        screen[0][originColumn + 1] = originColumn + 2;
        screen[1][originColumn] = originColumn + 1;
    }
}

void shoot(int column, int screen[5][5]) {
    int row;
    for (row = 4; row >= 0; row--) {
        if (screen[row][column - 1] != 0) {
            screen[row][column - 1] = 0;
            break;
        }
    }
}

void reset() {
    // *** Intro Screen ***
    Graphics_clearDisplay(&g_sContext); // Clear the display
    // Write some text to the display
    Graphics_drawStringCentered(&g_sContext, "SPACE", AUTO_STRING_LENGTH, 48, 15, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "INVADERS", AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "ECE2049-A21!", AUTO_STRING_LENGTH, 48, 35, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "Press *", AUTO_STRING_LENGTH, 48, 65, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "to start", AUTO_STRING_LENGTH, 48, 75, TRANSPARENT_TEXT);

    Graphics_Rectangle box = {.xMin = 5, .xMax = 91, .yMin = 5, .yMax = 91 };
    Graphics_drawRectangle(&g_sContext, &box);

    Graphics_flushBuffer(&g_sContext);
}

void swDelay(char numLoops)
{
    // This function is a software delay. It performs
    // useless loops to waste a bit of time
    //
    // Input: numLoops = number of delay loops to execute
    // Output: none
    //
    // smj, ECE2049, 25 Aug 2021

    volatile unsigned int i,j;  // volatile to prevent removal in optimization
                                // by compiler. Functionally this is useless code

    for (j=0; j<numLoops; j++)
    {
        i = 40000 ;                 // SW Delay
        while (i > 0)               // could also have used while (i)
           i--;
    }
}

void setScreen(int screen[5][5]) {
    Graphics_clearDisplay(&g_sContext); // Clear the display
    int r = 0, c = 0;
    for(r = 0; r < 5; r++) {
        for (c = 0; c < 5; c++) {
            if (screen[r][c] == 0) {
                continue;
            }
            if (screen[r][c] < 6) {
                char n[2];
                n[0] = screen[r][c] + 0x30;
                n[1] = '\0';

                // System function
                Graphics_drawString(&g_sContext, n,
                                            AUTO_STRING_LENGTH, 15 + c * 15, 15 + r * 15,
                                            TRANSPARENT_TEXT);
            } else {
                struct Graphics_Rectangle ret;
                ret.xMin = 15 + (c * 15);
                ret.xMax = 15 + (c * 15 + 8);
                ret.yMin = 15 + (r * 15);
                ret.yMax = 15 + (r * 15 + 8);
                Graphics_drawRectangle(&g_sContext, &ret);
            }
        }
    }
    Graphics_flushBuffer(&g_sContext);
}

void countdown() {
    swDelay(1);
    int a = 0;
    for (a = 3; a >= 1; a--) {
        int m[5][5] = { {0, 0,  0,  0, 0},
                        {0, 0,  0,  0, 0},
                        {0, 0,  a,  0, 0},
                        {0, 0,  0,  0, 0},
                        {0, 0,  0,  0, 0}};
        setScreen(m);
        swDelay(2);
    }
}
