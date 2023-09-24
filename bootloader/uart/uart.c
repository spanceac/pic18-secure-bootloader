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

int uart_get_byte(uint8_t *byte, size_t timeout_us, bool block) {
    int ret = -1;

    if (block) {
        while(!PIR1bits.RC1IF);
        *byte = RCREG1;
        return 0;
    }

    for (; timeout_us > 0; timeout_us--) {
        if (PIR1bits.RC1IF == 0) {
            // loop tunning for delaying exactly 1us necessary
           asm("nop");
        } else {
            *byte = RCREG1;
            ret = 0;
            break;
        }
    }

    return ret;
}

int uart_expect_msg(char *msg, size_t _len, size_t timeout_us)
{
    char byte;
    size_t i = 0, len = _len;
    int ret;

    if (RCSTA1bits.OERR) {
        // clear RX overrun error which stops UART RX
        RCSTA1bits.CREN = 0;
        RCSTA1bits.CREN = 1;
    }

    while (len) {
        ret = uart_get_byte((uint8_t *)&byte, 1, false);

        timeout_us--;
        if (!timeout_us) {
            return -1;
        }

        if (ret == 0) {
            if (byte == msg[i]) {
                i++;
                len--;
            } else {
                i = 0;
                len = _len;
            }
        }
    }
    return 0;
}

void uart_write_byte(uint8_t byte) {
    while (!TXSTA1bits.TRMT);
    TXREG1 = byte;
    while (!TXSTA1bits.TRMT);
    return;
}

void uart_send_buf(uint8_t *buf, size_t cnt) {
    while (cnt > 0) {
        uart_write_byte(*buf);
        buf++;
        cnt--;
    }
}

