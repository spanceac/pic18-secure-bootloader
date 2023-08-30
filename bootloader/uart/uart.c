#include <xc.h>
#include "uart.h"

#define _XTAL_FREQ 64000000

void uart_init(void) {
    TRISCbits.RC6 = 1; /* UART TX */
    TRISCbits.RC7 = 1; /* UART RX */
    ANSELCbits.ANSC7 = 0;
    TXSTA1bits.SYNC = 0;
    TXSTA1bits.TXEN = 1;
    RCSTA1bits.SPEN = 1;
    RCSTA1bits.CREN = 1;
    SPBRG1 = 8 ; /* 115200 baudrate */
}

int uart_get_byte(uint8_t *byte, size_t timeout_ms, bool block) {
    int ret = -1;

    if (block) {
        while(!PIR1bits.RC1IF);
        *byte = RCREG1;
        return 0;
    }

    for (; timeout_ms > 0; timeout_ms--) {
        if (PIR1bits.RC1IF == 0) {
            __delay_ms(1);
        } else {
            *byte = RCREG1;
            ret = 0;
            break;
        }
    }
        
    return ret;
}

void uart_write_byte(uint8_t byte) {
    while (!TXSTA1bits.TRMT);
    TXREG1 = byte;
    while (!TXSTA1bits.TRMT);
    return;
}

#ifdef DEBUG
void uart_send_buf(uint8_t *buf, size_t cnt) {
    while (cnt > 0) {
        uart_write_byte(*buf);
        buf++;
        cnt--;
    }
}
#endif
