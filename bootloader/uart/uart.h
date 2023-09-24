#include <stdbool.h>

void uart_init(void);
int uart_get_byte(uint8_t *byte, size_t timeout_us, bool block);
void uart_write_byte(uint8_t byte);
int uart_expect_msg(char *msg, size_t len, size_t timeout_us);
void uart_send_buf(uint8_t *buf, size_t cnt);

