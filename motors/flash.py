import sys
from stm8s import write_block, unlock_prog

addr=0x8000

unlock_prog()

while True:
    data = sys.stdin.buffer.read(64)
    if not data:
        break
    write_block(addr, data)
    addr = addr + len(data)

