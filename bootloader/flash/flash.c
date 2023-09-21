#include <xc.h>

#define FLASH_BLOCK_SIZ 64
#define FLASH_SIZE 0x8000

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

static inline void flash_erase_blk(size_t blk_idx)
{
    uint16_t blk_addr = blk_idx * FLASH_BLOCK_SIZ;
    TBLPTRH = blk_addr >> 8;
    TBLPTRL = blk_addr & 0xff;
    EECON1bits.EEPGD = 1; /* point to Flash memory */
    EECON1bits.CFGS = 0; /* access Flash program memory */
    EECON1bits.WREN = 1; /* enable write to memory */
    EECON1bits.FREE = 1; /* enable block erase */
    EECON2 = 0x55;
    EECON2 = 0xaa;
    EECON1bits.WR = 1; /* start programming (CPU stall until done) */
}

void erase_flash(size_t btld_addr) {
    size_t i = 0;
    size_t erase_blk_cnt = btld_addr / FLASH_BLOCK_SIZ;
    uint16_t addr = 0;
    uint8_t save_goto_btld[4];

    read_flash(0, save_goto_btld, 4);
    flash_erase_blk(1);
    write_flash(64, save_goto_btld, 4);

    for (size_t curr_erase_blk = 0; curr_erase_blk < erase_blk_cnt; curr_erase_blk++) {
        flash_erase_blk(curr_erase_blk);

        if (curr_erase_blk == 0) {
            write_flash(0, save_goto_btld, 4);
        }
    }
}
