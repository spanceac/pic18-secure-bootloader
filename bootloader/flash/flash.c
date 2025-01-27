#include <xc.h>

#define FLASH_BLOCK_SIZ 64
#define FLASH_SIZE 0x8000

struct table_pointers {
    uint8_t up;
    uint8_t hi;
    uint8_t lo;
};

static void save_table_pointers(struct table_pointers *tp)
{
    tp->up = TBLPTRU;
    tp->hi = TBLPTRH;
    tp->lo = TBLPTRL;
}

static void restore_table_pointers(struct table_pointers *tp)
{
    TBLPTRU = tp->up;
    TBLPTRH = tp->hi;
    TBLPTRL = tp->lo;
}

int write_flash(uint24_t addr, const uint8_t *buf, size_t count) {
    size_t i = 0;
    struct table_pointers tp;
    save_table_pointers(&tp);

    TBLPTRU = (uint8_t)(addr >> 16);
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

    restore_table_pointers(&tp);
    return 0;
}

void read_flash(uint24_t address, uint8_t *buf, size_t count) {
    struct table_pointers tp;
    save_table_pointers(&tp);

    TBLPTRU = (uint8_t)(address >> 16);
    TBLPTRH = (uint8_t)(address >> 8);
    TBLPTRL = (uint8_t)address & 0xff;

    for (size_t i = 0; i < count; i++) {
        asm("TBLRD*+");
        buf[i] = TABLAT;
    }

    restore_table_pointers(&tp);
}

static inline void flash_erase_blk(size_t blk_idx)
{
    uint24_t blk_addr = (uint24_t)blk_idx * FLASH_BLOCK_SIZ;
    struct table_pointers tp;
    save_table_pointers(&tp);

    TBLPTRU = (uint8_t)(blk_addr >> 16);
    TBLPTRH = (uint8_t)(blk_addr >> 8);
    TBLPTRL = (uint8_t)(blk_addr & 0xff);
    EECON1bits.EEPGD = 1; /* point to Flash memory */
    EECON1bits.CFGS = 0; /* access Flash program memory */
    EECON1bits.WREN = 1; /* enable write to memory */
    EECON1bits.FREE = 1; /* enable block erase */
    EECON2 = 0x55;
    EECON2 = 0xaa;
    EECON1bits.WR = 1; /* start programming (CPU stall until done) */

    restore_table_pointers(&tp);
}

void erase_flash(uint24_t btld_addr) {
    size_t erase_blk_cnt = (size_t)(btld_addr / FLASH_BLOCK_SIZ);
    uint8_t save_goto_btld[4];

    read_flash(0, save_goto_btld, 4);
    flash_erase_blk(1);
    write_flash(FLASH_BLOCK_SIZ, save_goto_btld, 4);

    for (size_t curr_erase_blk = 0; curr_erase_blk < erase_blk_cnt; curr_erase_blk++) {
        flash_erase_blk(curr_erase_blk);

        if (curr_erase_blk == 0) {
            write_flash(0, save_goto_btld, 4);
        }
    }
}
