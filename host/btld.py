import serial
import sys
import math
import hashlib
import time
import ecdsa
from ecdsa import SigningKey
from ecdsa.util import sigencode_string

HOST_MSG_START = b'@'
HOST_MSG_END = b'\n'
HOST_MSG_ESC = b'\\'

HOST_MSG_PROGRAM_SIZE = b'M'
HOST_MSG_PROGRAM_SIGNAT = b'N'
HOST_MSG_FLASH_DATA = b'D'
HOST_MSG_FLASH_STOP = b'X'

MCU_MSG_OP_SUCCESS = b'F'

MCU_ERR_INVALID_PAYLOAD = b'I'
MCU_ERR_DENIED_ADDR = b'A'

HOST_HANDSHAKE_MSG = b'@BTL\n'
MCU_HANDSHAKE_RESP = b'@OK\n'

def ascii2dec(x):
    if ord(x) >= ord("0") and ord(x) <= ord("9"):
        return ord(x) - 48
    elif ord(x) >= ord("A") and ord(x) <= ord("F"):
        return ord(x) - 55
    elif ord(x) >= ord("a") and ord(x) <= ord("f"):
        return ord(x) - 87
    else:
        return 0


def encode_for_uart(data):
    encoded = []
    encoded.append(ord(HOST_MSG_START))
    for d in data:
        if d == ord(HOST_MSG_START) or d == ord(HOST_MSG_END) or d == ord(HOST_MSG_ESC):
            encoded.append(ord(HOST_MSG_ESC))
        encoded.append(d)
    encoded.append(ord(HOST_MSG_END))
    return encoded


def encode_data(data, count, addr):
    encoded = []
    addr_hi = (addr >> 8) & 0xff
    addr_lo = addr & 0xff

    encoded.append(ord(HOST_MSG_FLASH_DATA))
    encoded.append(count)
    encoded.append(addr_hi)
    encoded.append(addr_lo)

    for d in data:
        encoded.append(d)

    return encode_for_uart(encoded)


def encode_size(fw_size):
    encoded = []
    hi = (fw_size >> 8) & 0xff
    lo = fw_size & 0xff

    encoded.append(ord(HOST_MSG_PROGRAM_SIZE))
    encoded.append(hi)
    encoded.append(lo)

    return encode_for_uart(encoded)


def encode_signat(signat):
    encoded = []

    encoded.append(ord(HOST_MSG_PROGRAM_SIGNAT))

    for s in signat:
        encoded.append(s)

    return encode_for_uart(encoded)


def wait_for_mcu(ser):
    print("Waiting for MCU")
    while True:
        resp = ser.read(1)

        if resp == MCU_MSG_OP_SUCCESS:
            return
        elif resp == MCU_ERR_INVALID_PAYLOAD:
            print("ERR: MCU reported invalid payload")
            sys.exit(-1)
        elif resp == MCU_ERR_DENIED_ADDR:
            print("ERR: MCU reported denied write address")
            sys.exit(-1)


def main():
    ser = serial.Serial(sys.argv[1], baudrate=115200, timeout=0.5)
    f = open(sys.argv[2], 'r', encoding="utf-8")
    fw_hash = hashlib.new('sha256')

    next_expect_addr = 0
    ignore_next_data_rec = False
    fw_size = 0

    while (True):
        print("Sending start sequence to MCU")
        for byte in HOST_HANDSHAKE_MSG:
            ser.write(byte.to_bytes(1, 'big'))
            time.sleep(0.001)

        mcu_resp = ser.read(4)
        if  mcu_resp == MCU_HANDSHAKE_RESP:
            break
        else:
            print("Received unexepcted:", mcu_resp)
    print("MCU ready")

    for line in f:
        if line[0] != ':':
            print("Unexpected start of hex file")
            return -1

        count = ascii2dec(line[1]) * 16 + ascii2dec(line[2])

        addr = ascii2dec(line[3]) * (16 * 16 * 16) + ascii2dec(line[4]) * (16 * 16) + ascii2dec(line[5]) * 16 + ascii2dec(line[6])

        rec_type = ascii2dec(line[8])

        print("Count", count, "addr", addr, "rec_type", rec_type)


        if rec_type == 0:
            if ignore_next_data_rec:
                print("Ignoring this record")
                continue

            fw_size += count

            if next_expect_addr < addr:
                fill_data = []
                fill_addr = next_expect_addr
                fill_cnt = addr - next_expect_addr
                fw_size += fill_cnt

                print("Fill with", fill_cnt, "0xFF")
                for i in range(64):
                    fill_data.append(0xFF)

                for i in range(math.floor(fill_cnt / 64)):
                    fill_enc = encode_data(fill_data, 64, fill_addr)
                    fill_addr += 64
                    fw_hash.update(bytes(fill_data))
                    ser.write(bytes(fill_enc))
                    wait_for_mcu(ser)

                if fill_cnt % 64:
                    fill_enc = encode_data(fill_data[:fill_cnt % 64], fill_cnt % 64, fill_addr)
                    fw_hash.update(bytes(fill_data[:fill_cnt % 64]))
                    ser.write(bytes(fill_enc))
                    wait_for_mcu(ser)

            data = []
            for i in range(count):
                data.append(ascii2dec(line[9 + i * 2]) * 16 + ascii2dec(line[10 + i * 2]))

            fw_hash.update(bytes(data))
            print("data is", data)
            encoded = encode_data(data, count, addr)
            print("encoded is", encoded)

            ser.write(bytes(encoded))
            next_expect_addr = addr + count
            wait_for_mcu(ser)

        elif rec_type == 1:
            # hex done
            print("Sending size", fw_size)
            enc_siz = encode_size(fw_size)
            ser.write(bytes(enc_siz))
            wait_for_mcu(ser)

            print("fw sha256:", fw_hash.hexdigest())

            with open(sys.argv[3]) as f:
               sk = SigningKey.from_pem(f.read(), hashlib.sha256)

            # sign hash and write signature
            fw_sig = sk.sign_digest_deterministic(fw_hash.digest(), sigencode=sigencode_string)

            print("signat is:", fw_sig.hex())

            ser.write(bytes(encode_signat(fw_sig)))
            wait_for_mcu(ser)

            ser.write(HOST_MSG_START + HOST_MSG_FLASH_STOP +  HOST_MSG_END)

            print("Flashing done")
            return

        elif rec_type == 4:
            ignore_next_data_rec = False
            for i in range(count * 2):
                if (ascii2dec(line[9 + i]) != 0):
                    ignore_next_data_rec = True
                    break
        else:
            print("Ingnoring record type", rec_type)

    return 0


if __name__ == '__main__':
    sys.exit(main())
