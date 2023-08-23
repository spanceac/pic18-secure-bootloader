import sys

goto_instr_1 = 0xEF
goto_instr_2 = 0xF0

def main():

    hex_in = open(sys.argv[1], 'r')
    hex_out = open('bootloader_patched.hex', 'w')

    btld_offset = int(sys.argv[2], base=16)

    btld_offset = btld_offset >> 1 # match how goto loads PC

    btld_off_hi = (btld_offset >> 16) & 0x0f
    btld_off_mid = (btld_offset >> 8) & 0xff
    btld_off_lo = btld_offset & 0xff

    # place with MCU endianess
    goto_instr = [btld_off_lo, goto_instr_1, btld_off_mid, btld_off_hi | goto_instr_2]
    goto_line = ":04000000"

    cksum = 4 # count 4 + addr 0 + code 0
    for i in goto_instr:
        cksum += i

    cksum %= 256
    cksum = 256 - cksum

    for i in goto_instr:
        goto_line += '%02X' % i

    goto_line += '%02X' % cksum
    goto_line += '\n'

    print(goto_line)

    hex_out.write(goto_line)
    for line in hex_in:
        hex_out.write(line)


if __name__ == '__main__':
    sys.exit(main())
