    /*
 * File:   newmain.c
 * Author: spanceac
 *
 * Created on April 13, 2023, 10:07 AM
 */

#include <stdbool.h>
#include <stdio.h>

#include "sha256/sha256.h"
#include "uECC/uECC.h"
#include "uart/uart.h"
#include "flash/flash.h"

#include "mcu/mcu.h"

#define SIGNAT_SIZE 64
#define CODE_SIZE_BYTES 3
#define CODE_SIZE_OFFSET BTLD_OFFSET - CODE_SIZE_BYTES
#define SIGNAT_OFFSET CODE_SIZE_OFFSET - SIGNAT_SIZE

#define HOST_MSG_START '@'
#define HOST_MSG_END '\n'
#define HOST_MSG_ESC '\\'

#define HOST_MSG_PROGRAM_SIZE 'M'
#define HOST_MSG_PROGRAM_SIGNAT 'N'
#define HOST_MSG_FLASH_DATA 'D'
#define HOST_MSG_FLASH_STOP 'X'

#define MCU_MSG_OP_SUCCESS 'F'

#define MCU_MSG_SIG_CHECK_OK 'S'
#define MCU_MSG_SIG_CHECK_FAIL 'K'

#define MCU_ERR_INVALID_PAYLOAD 'I'
#define MCU_ERR_DENIED_ADDR 'A'

#define HOST_HANDSHAKE_MSG "@BTL\n"
#define MCU_HANDSHAKE_RESP "@OK\n"

#define HOST_MSG_DATA_PAYLOAD_OFFSET 4

enum flashing_status {
    STATUS_NO_ERR,
    STATUS_ERR_INVALID_PAYLOAD,
    STATUS_ERR_DENIED_ADDR,
};

char mcu_errs[] = {MCU_MSG_OP_SUCCESS, MCU_ERR_INVALID_PAYLOAD, MCU_ERR_DENIED_ADDR};
//#define DEBUG

const uint8_t ec_pub_key[] = {
                              0xce,0x30,0x36,0x7c,0xc1,0x6e,0xb0,0x8c,0x5f,0x0e,0xb0,0x2c,0x11,0x4f,
                              0x8f,0x78,0x08,0x85,0xec,0xcf,0xdb,0x73,0xc8,0xda,0x6d,0x9a,0x00,0x6a,
                              0x33,0x95,0xa2,0x20,0xcb,0xdd,0xb2,0x9d,0x97,0xa0,0x5c,0x0f,0x0f,0x4f,
                              0x66,0x66,0x28,0xd2,0xe6,0x29,0x3e,0x3b,0x28,0x72,0x46,0xeb,0xd9,0x9f,
                              0xa0,0xe2,0x9a,0xa8,0xa6,0x9e,0xfc,0x2c
};

enum flashing_status message_handle(uint8_t op, uint8_t *data, size_t len) {
    uint24_t addr = 0;

    switch(op) {
        case HOST_MSG_PROGRAM_SIZE:
            if (len != CODE_SIZE_BYTES) {
                return STATUS_ERR_INVALID_PAYLOAD;
            }
            write_flash(CODE_SIZE_OFFSET, data, CODE_SIZE_BYTES);
            break;
        case HOST_MSG_PROGRAM_SIGNAT:
            if (len != SIGNAT_SIZE) {
                return STATUS_ERR_INVALID_PAYLOAD;
            }

#if 0
            char print[SIGNAT_SIZE * 2 + 20];
            char *p_buf = print;
            memcpy(p_buf, "recv_signat: ", strlen("recv_signat: "));
            p_buf += strlen("recv_signat: ");
            for (size_t i = 0; i < len; i++) {
                sprintf(p_buf, "%02X", data[i]);
                p_buf += 2;
            }
            memcpy(p_buf, "\n\0", 2);
            uart_send_buf(print, strlen(print));
#endif

            write_flash(SIGNAT_OFFSET, data, SIGNAT_SIZE);
            break;
        case HOST_MSG_FLASH_DATA:
            if (len < 5) { /* 1 byte cnt, 3 bytes addr, 1 data min */
                return STATUS_ERR_INVALID_PAYLOAD;
            }

            uint8_t payload_size = data[0];

            addr = (uint24_t)data[1] << 16 | (uint24_t)data[2] << 8 | data[3];

            if (addr + payload_size >= SIGNAT_OFFSET) {
                return STATUS_ERR_DENIED_ADDR;
            }

            if (addr == 0) {
                /* TODO: check if GOTO */
                /* TODO: check if 4 -7 is only 0xFF */
                /* flash user code GOTO */
                write_flash(4, &data[HOST_MSG_DATA_PAYLOAD_OFFSET], 4);
                /* skip address 4 to 7 and flash what starts at offset 8 */
                if (payload_size > 8) {
                    write_flash(8, &data[HOST_MSG_DATA_PAYLOAD_OFFSET + 8], payload_size - 8);
                }
                break;
            }
            write_flash(addr, &data[HOST_MSG_DATA_PAYLOAD_OFFSET], payload_size);
            break;
        case HOST_MSG_FLASH_STOP:
            if (len > 0) {
                return STATUS_ERR_INVALID_PAYLOAD;
            }
            asm("reset");
            break;
        default:
            return STATUS_ERR_INVALID_PAYLOAD;
            break;
    }
    return STATUS_NO_ERR;
}

enum flashing_status fw_receive(void) {
    uint8_t msg[70];
    uint8_t byte;
    size_t i = 0;
    bool escaped = false;

    while (1) {
        uart_get_byte(&byte, 0, true);

        if (byte == HOST_MSG_ESC && !escaped) {
            /* next byte needs escaping */
            escaped = true;
            continue;
        }

        msg[i] = byte;

        if (escaped) {
            escaped = false;
            i++;
            if (i == sizeof(msg)) {
                i = 0;
            }
            continue;
        }

        if (byte == HOST_MSG_START && i > 0) {
            /* unexpected start of message, reset buffer */
            i = 1;
            msg[0] = HOST_MSG_START;
            continue;
        }

        if (msg[i] == HOST_MSG_END && msg[0] == HOST_MSG_START) {
            if (i > 1) { /* at least the opcode as payload */
                /* should check if byte cnt matches data received cnt */
                enum flashing_status status = message_handle(msg[1], msg + 2, i - 2);
                if (status != STATUS_NO_ERR) {
                    return status;
                }
            }
            i = 0;
            uart_write_byte(MCU_MSG_OP_SUCCESS);
        } else {
            i++;
            if (i == sizeof(msg)) {
                i = 0;
            }
        }
    }
}

int signature_valid() {
    uint24_t siz = 0;
    uint8_t d[CODE_SIZE_BYTES];
    uint8_t signat[SIGNAT_SIZE];
    size_t i;
    SHA256_CTX ctx;
    BYTE cksum[SHA256_BLOCK_SIZE];
    uint8_t flread[64];
    uint24_t addr = 0;
    const struct uECC_Curve_t * curve = uECC_secp256k1();
#ifdef DEBUG
    char print[SIGNAT_SIZE * 2 + 20];
#endif

    sha256_init(&ctx);

    read_flash(CODE_SIZE_OFFSET, d, sizeof(d));

    siz = ((uint24_t)d[0] << 16) | ((uint24_t)d[1] << 8) | d[2] ;

#ifdef DEBUG
    snprintf(print, sizeof(print) - 1, "fw_siz: %lu\n\0", siz);
    uart_send_buf(print, strlen(print));
#endif

    read_flash(addr, flread, sizeof(flread));
    siz -= sizeof(flread);
    addr += sizeof(flread);

    /* checksum goto user code as being at addr 0, and 
     * addr 4-8 put 0xff as in original hex */
    flread[0] = flread[4];
    flread[1] = flread[5];
    flread[2] = flread[6];
    flread[3] = flread[7];

    memset(flread + 4, 0xff, 4);

#ifdef DEBUG
    for (i = 0; i < 8; i++) {
        snprintf(print, sizeof(print) - 1, "print[%d]: 0x%02X\n\0", i, flread[i]);
        uart_send_buf(print, strlen(print));
    }
#endif

    sha256_update(&ctx, flread, sizeof(flread));

    while (siz >= sizeof(flread)) {
        read_flash(addr, flread, sizeof(flread));
        siz -= sizeof(flread);
        addr += sizeof(flread);
        sha256_update(&ctx, flread, sizeof(flread));
    }
    if (siz) {
        read_flash(addr, flread, (size_t)siz);
        sha256_update(&ctx, flread, (size_t)siz);
    }

    sha256_final(&ctx, cksum);

    read_flash(SIGNAT_OFFSET, signat, sizeof(signat));

#ifdef DEBUG
    char *p_buf = print;
    memcpy(p_buf, "sha256: ", strlen("sha256: "));
    p_buf += strlen("sha256: ");
    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        sprintf(p_buf, "%02X", cksum[i]);
        p_buf += 2;
    }
    memcpy(p_buf, "\n\0", 2);
    uart_send_buf(print, strlen(print));

    p_buf = print;
    memcpy(p_buf, "signat: ", strlen("signat: "));
    p_buf += strlen("signat: ");
    for (i = 0; i < SIGNAT_SIZE; i++) {
        sprintf(p_buf, "%02X", signat[i]);
        p_buf += 2;
    }
    memcpy(p_buf, "\n\0", 2);
    uart_send_buf(print, strlen(print));
    return 0;
#else
    return uECC_verify(ec_pub_key, cksum, SHA256_BLOCK_SIZE, signat, curve);
#endif
}

void main(void) {
    uint8_t byte;
    int ret;
    enum flashing_status status;

    mcu_init();
    uart_init(RX_STATE_DISABLED);

    uart_rx_enable();
    ret = uart_expect_msg(HOST_HANDSHAKE_MSG, 5, 60000);
    if (ret) {
        /* nothing interesting from PC, booting old code */
        if (signature_valid()) {
            uart_write_byte(MCU_MSG_SIG_CHECK_OK);
            asm("goto 4"); /* jump to user code */
        } else {
            uart_write_byte(MCU_MSG_SIG_CHECK_FAIL);
            asm("reset");
        }
    }


    uart_rx_disable();
    erase_flash(BTLD_OFFSET);

    uart_send_buf((uint8_t *)MCU_HANDSHAKE_RESP, strlen(MCU_HANDSHAKE_RESP));

    uart_rx_enable();
    status = fw_receive();
    if (status != STATUS_NO_ERR) {
        uart_write_byte(mcu_errs[status]);
    }

    asm("reset");
}
