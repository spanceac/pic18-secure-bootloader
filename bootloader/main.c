    /*
 * File:   newmain.c
 * Author: spanceac
 *
 * Created on April 13, 2023, 10:07 AM
 */


#include <xc.h>
#include "sha256/sha256.h"
#include "uECC/uECC.h"
#include <stdbool.h>
#include <stdio.h>

#pragma config FOSC = INTIO67   // Oscillator Selection bits (Internal oscillator block)
#pragma config PLLCFG = ON     // 4X PLL Enable (Oscillator used directly)
#pragma config PRICLKEN = ON    // Primary clock enable bit (Primary clock enabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config WDTEN = OFF      // Watchdog Timer Enable bits (Watch dog timer is always disabled. SWDTEN has no effect.)

#define FLASH_BLOCK_SIZ 64
#define FLASH_SIZE 0x8000

#define BTLD_OFFSET 0x1000
#define SIGNAT_SIZE 40
#define CODE_SIZE_OFFSET BTLD_OFFSET - 2
#define SIGNAT_OFFSET CODE_SIZE_OFFSET - SIGNAT_SIZE


#define _XTAL_FREQ 64000000

//#define DEBUG

const uint8_t ec_pub_key[] = {0x47,0xcc,0xad,0x79,0x7c,0x38,0xe3,0xca,0x13,0x3b,0x58,0x61,
    0x3f,0x9c,0x53,0x32,0x68,0x78,0x69,0xf0,0xb7,0x9c,0x80,0x75,0x7a,0xb2,0xe8,0x45,0xe3,
    0x25,0xbb,0x28,0x49,0x3c,0x5f,0x58,0x79,0x4a,0xcb,0x9a};

int write_flash(uint16_t addr, const uint8_t *buf, size_t count) {
    size_t i = 0;

    TBLPTRH = (uint8_t)(addr >> 8);
    TBLPTRL = (uint8_t)addr & 0xff;


    for (i = 0; i < count; i++) {
        TABLAT = buf[i];
        
        if (((addr + 1) % FLASH_BLOCK_SIZ == 0) || (i == count - 1)) {
            /* don't advance table pointer, to keep it in block range */
            asm("TBLWT*");
            /* start the hw flashing procedure */
            EECON1bits.EEPGD = 1; /* point to Flash memory */
            EECON1bits.CFGS = 0; /* access Flash program memory */
            EECON1bits.WREN = 1; /* enable write to memory */

            EECON2 = 0x55;
            EECON2 = 0xaa;
           
            /* start programming (CPU stall until done) */
            EECON1bits.WR = 1;
            asm("TBLRD*+"); /* increment the pointer but don't write anything, quirk */
        } else {
            asm("TBLWT*+");
        }

        addr++;
    }
    return 0;
}

void read_flash(uint16_t address, uint8_t *buf, size_t count) {
    TBLPTRH = (uint8_t)(address >> 8);
    TBLPTRL = (uint8_t)address & 0xff;

    for (size_t i = 0; i < count; i++) {
        asm("TBLRD*+");
        buf[i] = TABLAT;
    }
}

void erase_flash(void) {
    size_t i = 0;
    size_t erase_blk_cnt = BTLD_OFFSET / FLASH_BLOCK_SIZ;
    uint16_t addr = 0;
    uint8_t save_goto_btld[4];

    read_flash(0, save_goto_btld, 4);

    while (erase_blk_cnt > 0) {
        TBLPTRH = (uint8_t)addr >> 8;
        TBLPTRL = (uint8_t)addr & 0xff;
        EECON1bits.EEPGD = 1; /* point to Flash memory */
        EECON1bits.CFGS = 0; /* access Flash program memory */
        EECON1bits.WREN = 1; /* enable write to memory */
        EECON1bits.FREE = 1; /* enable block erase */
        EECON2 = 0x55;
        EECON2 = 0xaa;
        EECON1bits.WR = 1; /* start programming (CPU stall until done) */
        erase_blk_cnt--;
        addr += 64;
    }

    write_flash(0, save_goto_btld, 4);
}

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

int message_handle(uint8_t op, uint8_t *data, size_t len) {
    uint16_t addr = 0;

    switch(op) {
        case 'M':
            if (len < 2) {
                return -1;
            }
            write_flash(CODE_SIZE_OFFSET, data, 2);
            break;
        case 'N':
            if (len < SIGNAT_SIZE) {
                return -2;
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
        case 'D':
            if (len < 4) { /* 1 byte cnt, 2 bytes addr, 1 data min */
                return -3;
            }

            /* ignoring data count byte data[0] */

            addr = (uint16_t)data[1] << 8 | data[2];

            if (addr + len - 3 >= SIGNAT_OFFSET) {
                return -4;
            }
            
            if (addr == 0) {
                /* TODO: check if GOTO */
                /* TODO: check if 4 -7 is only 0xFF */
                /* flash user code GOTO */
                write_flash(4, data + 3, 4);
                /* skip address 4 to 7 and flash what starts at offset 8 */
                if (len > 3 + 8) {
                    write_flash(8, data + 3 + 8, len - 3 - 8);
                }
                break;
            }
            if (write_flash(addr, data + 3, len - 3)) {
                return -88;
            }
            break;
        case 'X':
            if (len > 0) {
                return -5;
            }
            asm("reset");
            break;
        default:
            return -6;
            break;
    }
    return 0;
}

void fw_receive(void) {
    uint8_t msg[70];
    uint8_t byte;
    size_t i = 0;
    bool escaped = false;

    while (1) {
        uart_get_byte(&byte, 0, true);
        
        if (escaped) {
            escaped = false;
        } else {
            if (byte == '\\') {
                escaped = true;
                continue;
            }
        }

        if (byte == '@') {
            i = 0;
        }

        msg[i] = byte;

        if (msg[i] == '\n' && msg[0] == '@') {
            if (i > 1) { /* at least the opcode as payload */
                /* should check if byte cnt matches data received cnt */
                message_handle(msg[1], msg + 2, i - 2);
            }
            i = 0;
            uart_write_byte('F');
        } else {
            i++;
            if (i == sizeof(msg)) {
                i = 0;
            }
        }

    }
}

int signature_valid() {
    uint16_t siz = 0;
    uint8_t d[2];
    uint8_t signat[SIGNAT_SIZE];
    size_t i;
	SHA256_CTX ctx;
    BYTE cksum[SHA256_BLOCK_SIZE];
    uint8_t flread[64];
    uint16_t addr = 0;
    const struct uECC_Curve_t * curve = uECC_secp160r1();
#ifdef DEBUG
    char print[SIGNAT_SIZE * 2 + 20];
#endif

 	sha256_init(&ctx);

    read_flash(CODE_SIZE_OFFSET, d, 2);

    siz = ((uint16_t)d[0] << 8) | (d[1] & 0xff);
    
#ifdef DEBUG
    snprintf(print, sizeof(print) - 1, "fw_siz: %u\n\0", siz);
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
        read_flash(addr, flread, siz);
        sha256_update(&ctx, flread, siz);
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

    OSCCON = 0x70; /* select 16 MHz internal oscillator */
    OSCTUNEbits.PLLEN = 1;
    while(!OSCCONbits.HFIOFS);
    while(!OSCCON2bits.PLLRDY);
    uart_init();
    uart_write_byte('N');
#if 0
  	BYTE text1[] = {"abc"};
	BYTE text2[] = {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};
	BYTE text3[] = {"aaaaaaaaaa"};
	BYTE hash1[SHA256_BLOCK_SIZE] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
	                                 0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
	
    uint8_t flread[64];
    
	SHA256_CTX ctx;
	int idx;
	int pass = 1;


const uint8_t signat[] = {0x55,0xC8,0x44,0xF3,0xAC,0xC9,0xBD,0xBE,0x08,0x62,0xC3,0x58,0xA2,0x52,0x09,0xC8,0x9E,0x39,0x94,0x02
,0x83,0x33,0xA7,0x5C,0xB0,0x8C,0x40,0x33,0xF2,0xA0,0x2F,0xC1,0xD2,0xAB,0x3D,0xAA,0xA5,0x4B,0x0A,0x9F};
    const struct uECC_Curve_t * curve = uECC_secp160r1();
    
	sha256_init(&ctx);
	sha256_update(&ctx, text1, strlen(text1));
	sha256_final(&ctx, buf);
	pass = pass && !memcmp(hash1, buf, SHA256_BLOCK_SIZE);

    read_flash(0x162e, flread,64);
    int x = uECC_verify(ec_pub_key, hash1, SHA256_BLOCK_SIZE, signat, curve);
    if (x == 0)
        idx = 3;
#endif

    ret = uart_get_byte(&byte, 1200, false);
    if (ret || byte != '#') {
        /* nothing interesting from PC, booting old code */
        if (signature_valid()) {
            uart_write_byte('S');
            asm("goto 4"); /* jump to user code */
        } else {
            uart_write_byte('K');
            asm("reset");
        }
    }

    erase_flash();
    uart_write_byte('F');
    fw_receive();

    asm("reset");
}
