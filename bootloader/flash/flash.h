int write_flash(uint16_t addr, const uint8_t *buf, size_t count);
void read_flash(uint16_t address, uint8_t *buf, size_t count);
void erase_flash(size_t btld_addr);

