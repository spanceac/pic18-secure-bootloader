#include <stdbool.h>

void uart_init(void);
int uart_get_byte(uint8_t *byte, size_t timeout_ms, bool block);
void uart_write_byte(uint8_t byte);
#ifdef DEBUG
void uart_send_buf(uint8_t *buf, size_t cnt);
#endif
