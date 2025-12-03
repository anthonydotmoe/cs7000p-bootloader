#include "delay.h"

void delayMs(unsigned int mseconds) {
    register const unsigned int count = 133000;

    for (unsigned int i = 0; i < mseconds; i++) {
        // This delay has been calibrated to take 1 millisecond
        asm volatile("           mov   r1, #0     \n"
                     "___loop_m: cmp   r1, %0     \n"
                     "           itt   lo         \n"
                     "           addlo r1, r1, #1 \n"
                     "           blo   ___loop_m  \n"::"r"(count):"r1");
    }
}

void delayUs(unsigned int useconds) {
    // This delay has been calibrated to take x microseconds
    // It is written in assembler to be independent on compiler optimization
    asm volatile("           mov   r1, #133  \n"
                 "           mul   r2, %0, r1 \n"
                 "           mov   r1, #0     \n"
                 "___loop_u: cmp   r1, r2     \n"
                 "           itt   lo         \n"
                 "           addlo r1, r1, #1 \n"
                 "           blo   ___loop_u  \n"::"r"(useconds):"r1","r2");

}
