import sys
import serial
from struct import pack, unpack
from binascii import hexlify, unhexlify

port = serial.Serial(
        #port = '/dev/serial/by-id/usb-LMI_Stellaris_Evaluation_Board_070001C1-if01-port0',
        port = 'stellarisTty0',
        baudrate = 115200,
        parity = "N",
        stopbits = 1,
        xonxoff = 0,
        timeout = 3,
        interCharTimeout = 0.5 
    )

port.flushInput()
port.flushOutput()

def rotf(addr, l):
    addr = addr & 0xffffff
    port.write(pack('>B B B H', 1, l, addr >> 16, addr & 0xffff))
    s=port.read(1)
    if s[0] != 1:
        raise Exception('cmd reply missmatch: %06x, %d, %02x != 01' % (addr, l, s[0]))
    return port.read(l)

def wotf(addr, s):
    addr = addr & 0xffffff
    port.write(pack('>B B B H', 2, len(s), addr >> 16, addr & 0xffff) + s)
    r=port.read(1)
    if r[0] != 2:
        raise Exception('cmd reply missmatch: %06x, %d, %02x != 02' % (addr, len(s), r[0]))

def upload(addr, b):
    for i in range(0, len(b), 0x80):
        wotf(addr, b[i:i+0x80])
        addr = addr+0x80

def fread(fname):
    f=open(fname, 'rb')
    b=f.read()
    f.close()
    return b 

def unlock_eeprom():
    wotf(0x5064, unhexlify('ae'))
    wotf(0x5064, unhexlify('56'))

def unlock_prog():
    wotf(0x5062, unhexlify('56'))
    wotf(0x5062, unhexlify('ae'))


def disable_rop():
    wotf(0x505b, unhexlify('81'))
    wotf(0x505c, unhexlify('7e'))
    wotf(0x4800, unhexlify('00'))

def disable_ubc():
    wotf(0x505b, unhexlify('81'))
    wotf(0x505c, unhexlify('7e'))
    wotf(0x4801, unhexlify('00ff'))
    
def write_block(addr, b):
    b = b + (b'\0' * (64-len(b)))
    wotf(0x505b, unhexlify('01fe'))
    wotf(addr, b)
    while True:
        iapsr = rotf(0x505f, 1)[0]
        if (iapsr & 1) != 0:
            raise Exception('write protected')

        if (iapsr & 4) != 0:
            break

def wb(addr, b):
    wotf(addr, pack('>B', b))

def ww(addr, w):
    wotf(addr, pack('>H', w))

def we(addr, e):
    wotf(addr, pack('>B H', e >> 16, e & 0xffff))

def rb(addr):
    return rotf(addr, 1)[0]

def ramcode():
    wotf(0x7f00, unhexlify('000000000000000003ff28'))
    wb(0x7f99, rb(0x7f99) | 0x01)
    wb(0x7F97, 0x01)

def load(fname):
    b=fread(fname)
    upload(0, b)
    ramcode()

def go():
    wb(0x7f99, rb(0x7f99) & ~0x08)
    
def stop():
    wb(0x7f99, rb(0x7f99) | 0x08)

def regs():
    b=rotf(0x7f00, 11)
    a, pce, pc, x, y, sp, cc = unpack('>B B H H H H B', b)
    pc |= pce << 16
    print('''
A = %02x
PC= %04x
X = %04x
Y = %04x
SP= %04x
CC= %02x
''' % (a, pc, x, y, sp, cc))
    return { 'a': a, 'pc': pc, 'x': x, 'y': y, 'sp': sp, 'cc': cc }

def reset():
    wotf(0x7f80, unhexlify('a4'))
    port.write(pack('>B', 0))
    r=port.read(1)
    if r[0] != 0:
        raise Exception('cmd reply missmatch: %02x != 00' % (r[0]))

def init():
    port.write(b'!')
    r=port.read(1)
    if r != b'!':
        raise Exception('cmd reply missmatch: %02x != 21' % (r[0]))

def opto(s):
    port.write(pack('>B B ', ord('>'), len(s)) + s)
    r=port.read(1)
    if r[0] != ord('>'):
        raise Exception('cmd reply missmatch: %06x, %d, %02x != %02x' % (addr, len(s), r[0], ord('>')))

