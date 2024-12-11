int write_flash(uint24_t addr, const uint8_t *buf, size_t count);
void read_flash(uint24_t address, uint8_t *buf, size_t count);
void erase_flash(uint24_t btld_addr);

