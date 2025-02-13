#include <stdbool.h>

#include <xc.h>

enum rx_state {
    RX_STATE_DISABLED,
    RX_STATE_ENABLED,
};

void uart_init(enum rx_state rx_state);
int uart_get_byte(uint8_t *byte, size_t timeout_us, bool block);
void uart_write_byte(uint8_t byte);
int uart_expect_msg(char *msg, size_t len, size_t timeout_us);
void uart_send_buf(uint8_t *buf, size_t cnt);

static inline void uart_rx_disable(void)
{
    RCSTA1bits.CREN = 0;
}

static inline void uart_rx_enable(void)
{
    RCSTA1bits.CREN = 1;
}

