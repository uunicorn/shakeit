
#include "stm8s_stdlib/stm8s_tim1.h"
#include "stm8s_stdlib/stm8s_clk.h"
#include "stm8s_stdlib/stm8s_gpio.h"

#define HYSTERESIS_I 10 // ~280 mA
#define HYSTERESIS_V 20 // ~20 V

#define PWM_FREQ  20000
#define PERIOD (F_CPU/PWM_FREQ)
/*

Description	U(V)	I(A)	P1(W)	M(N.m)	n(rpm)	P2(W)	EFF(%)
No_Load	        219.9	0.111	24.42	0.000	2848	0	0
Max_Eff	        219.9	0.900	198.0	0.627	2573	168.9	85.3
Max_Pout	219.9	4.280	941.2	3.311	1397	484.4	51.4
Max_Torque	219.9	8.296	1824	6.500	0	0	0
Locked_Rotor	219.9	8.296	1824	6.500	0	0	0
*/

const unsigned char clist[] = { 
    3, // U1  = 0x13(19) = 19V       ADC=Uin*2/103/5*256 ~Uin
    2, // U2
    5, // I1  = 0x45(69) = 2.15A     ADC=Iin*7/10/5*256
    6  // I2
};
unsigned char channels[sizeof(clist)], chan;
volatile unsigned char pulse, edge, byte, cmd[8], state[8], crc;

static inline void
init_tim1()
{
    TIM1->ARRH = PERIOD >> 8;       // auto-reload register
    TIM1->ARRL = PERIOD & 0xff;

    TIM1->PSCRH = 0;    // no prescaler
    TIM1->PSCRL = 0;

    TIM1->CR1 = 0;      // up, edge-aligned, no preload for arr, off
    TIM1->RCR = 0;      // no repetition

    TIM1->CCER2 = TIM1_CCER2_CC3E | TIM1_CCER2_CC3P; // OC3 enable, inverted
    TIM1->CCMR3 = 0x68; // PWM1 (active while cntr < CCR3), shadow register for CCR3
    TIM1->OISR = 0;     // default idle state

    TIM1->CCR3H = 0;
    TIM1->CCR3L = 0;

    TIM1->IER = 0x1; // UIE (overflow) interrupt enable

    TIM1->BKR |= TIM1_BKR_MOE; // enable output
    TIM1->CR1 |= TIM1_CR1_CEN; // enable timer1
}

static inline void
init_tim2()
{
    TIM2->ARRH = PERIOD >> 8;       // auto-reload register
    TIM2->ARRL = PERIOD & 0xff;

    TIM2->PSCR = 0;    // no prescaler

    TIM2->CR1 = 0;      // up, edge-aligned, no preload for arr, off

    TIM2->CCER1 = TIM2_CCER1_CC2E | TIM2_CCER1_CC2P; // OC2 enable, inverted
    TIM2->CCMR2 = 0x68; // PWM1 (active while cntr < CCR2), shadow register for CCR2

    TIM2->CCR2H = 0;
    TIM2->CCR2L = 0;

    TIM2->CR1 |= TIM2_CR1_CEN; // enable timer1
}

static inline void
init_adc()
{
    chan = 0;
    ADC1->CSR = 0x20 | clist[chan]; // enable interrupts on completion
    ADC1->CR1 = 0x70; // one shot, clk=Fmaster/18, power off
    ADC1->CR2 = 0x00; // no scan, no ext trig, left align (ADC_DRH has 8 MSB bits)
    ADC1->CR3 = 0x00; // no buf
    ADC1->CR1 |= 0x01; // power up

    ADC1->CR1 |= 0x01; // start the first conversion
}

// called after each conversion is complete
// 1 conversion = 14 adc clock sources
// adc freq = Fmaster/18
// this isr is called at 16e6/18/14 ~ 63492Hz
void 
adc1_isr() __interrupt(22) 
{
    ADC1->CSR &= ~0x80; // clear EOC flag

    channels[chan] = ADC1->DRH;
    chan = (chan+1) % sizeof(clist);
    ADC1->CSR = 0x20 | clist[chan]; // enable interrupts on completion
    ADC1->CR1 |= 0x01; // start the next conversion
}

void
tim1_cc_isr() __interrupt(11)
{
    TIM1->SR1 &= ~1; // clear interrupt

    if(pulse != 255) // stop counting after the max value reached
        pulse++;
}

void
portc_isr() __interrupt(5)
{
    if(pulse == 255) {
        // first edge after a while (255/20e3 ~13ms), reset the edge counter
        edge = 0;
        crc = 0;
    }

    // 0 -> 1
    if(edge & 1) {      // only consider odd edges
        byte <<= 1;
        if(pulse < 5)   // pulses < 250us = 1, 0 otherwise
            byte |= 1;
    }

    pulse=0;
    edge++;
    if((edge & 0xf) == 0) { // counted up to 16 edges => byte completed
        cmd[(edge >> 4)-1] = byte;
        crc ^= byte;
        if(edge == 0x80) {
            edge = 0;
            if(crc == 0) {
                unsigned char i;
                for(i=0;i<8;i++)
                    state[i] = cmd[i];
            }
        }
    }
}

static void
dec_duty(volatile unsigned char *ccr)
{
    unsigned char h, l;
    
    h = ccr[0];
    l = ccr[1];

    if(h == 0 && l ==0)
        return;

    l--;

    if(l == 0xff)
        h--;

    ccr[0] = h;
    ccr[1] = l;
}

static void
inc_duty(volatile unsigned char *ccr)
{
    unsigned char h, l;
    
    h = ccr[0];
    l = ccr[1];

    if(h == (PERIOD >> 8) && l == (PERIOD & 0xff))
        return;

    l++;

    if(l == 0x00)
        h++;

    ccr[0] = h;
    ccr[1] = l;
}

int main() 
{
    CLK->CKDIVR = 0; // Set no prescalers, 16MHz master clock

    GPIOC->CR2 = 1 << 7; // enable portc interrupt
    EXTI->CR1 = 3 << 4; // both rise and fall

    init_tim1();
    init_tim2();
    init_adc();

    pulse = 255;
    state[0] = state[1] = 220;
    state[2] = state[3] = 72; // ~ 2 A
    rim();

    while(1) {
        volatile unsigned char i, j;

        if(channels[2] > state[2] || channels[0] > state[0]) {
            dec_duty(&TIM1->CCR3H);
        } else if(channels[2] + HYSTERESIS_I < state[2] && channels[0] + HYSTERESIS_V < state[0]) {
            inc_duty(&TIM1->CCR3H);
        } // else - sweet spot, do nothing

        if(channels[3] > state[3] || channels[1] > state[1]) {
            dec_duty(&TIM2->CCR2H);
        } else if(channels[3] + HYSTERESIS_I < state[3] && channels[1] + HYSTERESIS_V < state[1]) {
            inc_duty(&TIM2->CCR2H);
        } // else - sweet spot, do nothing

        for(i=0;i<20;i++)
            for(j=0;j<0xff;j++)
                ;
    }
}

