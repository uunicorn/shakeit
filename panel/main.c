
#include "stm8s_stdlib/stm8s_tim1.h"
#include "stm8s_stdlib/stm8s_clk.h"
#include "stm8s_stdlib/stm8s_gpio.h"

#define TIM1_FREQ  2000
#define PERIOD (F_CPU/TIM1_FREQ)

/* LED Pinout
 *
 *     7
 *    ----                             9, 6
 * 10|    |8            ----|>|---*--------
 *   | 4  |                       |
 *    ----              ----|>|---*
 *  1|    |3                      |
 *   | 2  |             ----|>|---*
 *    ----  * 5
 */

/*
 * MUX X
 *
 * Bit Port  Switch   Segment
 * 0   PC5   S1       5
 * 1   PC4   S2       4
 * 2   PD4   S3       3
 * 3   PD5   S4       2
 * 4   PD6   S5       1
 * 5   PA1   S6       7
 * 6   PA2   S7       8
 * 7   PA3   S8       10
 *
 * Digit    Segments          Mask
 * 0        10,7,8,1,3,2      0xfc
 * 1        8,3               0x44
 * 2        7,8,4,1,2         0x7a
 * 3        7,8,4,3,2         0x6e
 * 4        10,8,4,3          0xc6
 * 5        7,10,4,3,2        0xae
 * 6        7,10,4,3,2,1      0xbe
 * 7        7,8,3             0x64
 * 8        1,2,3,4,7,8,10    0xfe
 * 9        2,3,4,7,8,10      0xee
 * A        1,3,4,7,8,10      0xf6
 * B        1,2,3,4,10        0x9e
 * C        2,7,1,10          0xb8
 * D        1,2,3,4,8         0x5e
 * E        2,4,7,1,10        0xba
 * F        1,4,7,10          0xb2
 * .        5                 0x01
 *
 *
 * MUX Y
 *
 * Port  Function
 * PD1   Switches
 * PB5   LED1.1
 * PB4   LED1.2
 * PC7   LED2.1
 * PC6   LED2.2
 *
 *
 * Outputs
 *
 * PD2   BUZ1
 * PC3   OPTO
 *
 * Inputs
 *
 * PD3   IR
 */
#define MUX_X_MASK_A 0x0e
#define MUX_X_MASK_C 0x30
#define MUX_X_MASK_D 0x70

#define MUX_Y_MASK_D 0x02
#define MUX_Y_MASK_B 0x30
#define MUX_Y_MASK_C 0xc0


static inline void gpio_init()
{
    // MUX X:
    // out: push-pull, in: pull-up
    GPIOA->CR1 |= MUX_X_MASK_A; 
    GPIOC->CR1 |= MUX_X_MASK_C; 
    GPIOD->CR1 |= MUX_X_MASK_D; 

    // MUX Y:

    // input
    // out: open drain, in: high-z

    // ODR: LEDs common cathode - active low (default)
    // ODR: Switches common - active low (default)
}

static inline void mux_set(unsigned char x, unsigned char y)
{
    // turn off all the things (switch Y pins to high-Z)
    GPIOD->DDR &= ~MUX_Y_MASK_D;
    GPIOB->DDR &= ~MUX_Y_MASK_B;
    GPIOC->DDR &= ~MUX_Y_MASK_C;

    // copy bits
    AffBit(GPIOC->ODR, 5, ValBit(x, 0));
    AffBit(GPIOC->ODR, 4, ValBit(x, 1));
    AffBit(GPIOD->ODR, 4, ValBit(x, 2));
    AffBit(GPIOD->ODR, 5, ValBit(x, 3));
    AffBit(GPIOD->ODR, 6, ValBit(x, 4));
    AffBit(GPIOA->ODR, 1, ValBit(x, 5));
    AffBit(GPIOA->ODR, 2, ValBit(x, 6));
    AffBit(GPIOA->ODR, 3, ValBit(x, 7));

    // set X pins as outputs
    GPIOA->DDR |= MUX_X_MASK_A;
    GPIOC->DDR |= MUX_X_MASK_C;
    GPIOD->DDR |= MUX_X_MASK_D;

    // set Y funcion
    switch(y) {
        case 0: // LED1.1
            SetBit(GPIOB->DDR, 5);
            break;
        case 1: // LED1.2
            SetBit(GPIOB->DDR, 4);
            break;
        case 2: // LED2.1
            SetBit(GPIOC->DDR, 7);
            break;
        case 3: // LED2.2
            SetBit(GPIOC->DDR, 6);
            break;
    }
}

static inline unsigned char 
mux_input()
{
    // turn off all the things (switch Y pins to high-Z)
    GPIOD->DDR &= ~MUX_Y_MASK_D;
    GPIOB->DDR &= ~MUX_Y_MASK_B;
    GPIOC->DDR &= ~MUX_Y_MASK_C;

    // set X pins as inputs
    GPIOA->DDR &= ~MUX_X_MASK_A;
    GPIOC->DDR &= ~MUX_X_MASK_C;
    GPIOD->DDR &= ~MUX_X_MASK_D;

    // set Y function
    SetBit(GPIOD->DDR, 1);
}

static inline unsigned char 
mux_get()
{
    if(ValBit(GPIOC->IDR, 5) == 0) return 1;
    if(ValBit(GPIOC->IDR, 4) == 0) return 2;
    if(ValBit(GPIOD->IDR, 4) == 0) return 3;
    if(ValBit(GPIOD->IDR, 5) == 0) return 4;
    if(ValBit(GPIOD->IDR, 6) == 0) return 5;
    if(ValBit(GPIOA->IDR, 1) == 0) return 6;
    if(ValBit(GPIOA->IDR, 2) == 0) return 7;
    if(ValBit(GPIOA->IDR, 3) == 0) return 8;

    return 0;
}

volatile unsigned short tick, repeat;
unsigned char state, pressed;

unsigned leds[4], dots;

unsigned char a, b;

const unsigned char hex2mask[] = { 0xfc, 0x44, 0x7a, 0x6e, 0xc6, 0xae, 0xbe, 0x64, 0xfe, 0xee, 0xf6, 0x9e, 0xb8, 0x5e, 0xba, 0xb2 };

static void update_a()
{
    leds[0] = hex2mask[(a / 10) % 10];
    leds[1] = hex2mask[a % 10];
}

static void update_b()
{
    leds[2] = hex2mask[(b / 10) % 10];
    leds[3] = hex2mask[b % 10];
}

static void
handle_click(char repeat)
{
    switch(pressed) {
        case 1:
            b--;
            update_b();
            break;
        case 2:
            b++;
            update_b();
            break;
        case 8:
            a++;
            update_a();
            break;
        case 7:
            a--;
            update_a();
            break;
    }
}


void
tim1_cc_isr() __interrupt(11)
{
    unsigned char button;

    TIM1->SR1 &= ~1; // clear interrupt

    if(state == 4) {
        // configure pins for input, but dont read the state of inputs - it must have enough time to settle down
        mux_input();
        tick++;
    } else {
        if(state == 0) {
            // get the state of input.
            button = mux_get();
            if(button) {
                if(pressed) {
                    if(--repeat == 0) {
                        handle_click(1);
                        repeat = 40; // 100ms
                    }
                } else {
                    pressed = button;
                    repeat = 200; // 500ms
                    handle_click(0);
                }
            } else {
                pressed = 0;
            }
        }

        mux_set(leds[state] | (ValBit(dots, state) != 0), state);
    }

    state++;
    state %= 5;
}

static inline void
tim1_init()
{
    state = 0;
    tick = 0;

    TIM1->ARRH = PERIOD >> 8;       // auto-reload register
    TIM1->ARRL = PERIOD & 0xff;

    TIM1->PSCRH = 0;    // no prescaler
    TIM1->PSCRL = 0;

    TIM1->CR1 = 0;      // up, edge-aligned, no preload for arr, off
    TIM1->RCR = 0;      // no repetition

    TIM1->OISR = 0;     // default idle state

    TIM1->IER = 0x1; // UIE (overflow) interrupt enable

    TIM1->CR1 |= TIM1_CR1_CEN; // enable timer1
}


int main() 
{
    volatile unsigned short int i, j;

    CLK->CKDIVR = 0; // Set no prescalers, 16MHz master clock

    pressed = 0;
    repeat = 0;

    a = b = 0;
    dots = 0;

    update_a();
    update_b();

    gpio_init();
    tim1_init();


    for(i=0;;) {
        dots = i & 0xf;

        rim();
        for(j=tick;tick - j < 500;)
            ;
        sim();
    }
}

