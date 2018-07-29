
#include "stm8s_stdlib/stm8s.h"

#define PWM_FREQ  20000
#define PERIOD F_CPU/PWM_FREQ

int 
main()
{
    volatile long int i;
    
    GPIOC->DDR |= 1 << 3;
    GPIOC->CR1 |= 1 << 3;
    GPIOC->CR2 &= ~(1 << 3);
    
    while(1) {
        GPIOC->ODR ^= 1 << 3;

        for(i=0;i<F_CPU/200;i++)
            ;
    }
}
