#include <xc.h>

void mcu_init(void)
{
    OSCCON = 0x70; /* select 16 MHz internal oscillator */
    OSCTUNEbits.PLLEN = 1;
    while(!OSCCONbits.HFIOFS);
    while(!OSCCON2bits.PLLRDY);
}
