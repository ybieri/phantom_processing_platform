from pwn import *

#ropper --nocolor  -f libc.so.6 > gadgets.txt 

def is_utf8_encodable(hex_int):
    try:
        p32(hex_int).decode('utf-8')
        return True
    except UnicodeDecodeError:
        return False


OFFSET = 0x76a56000 
gadgets = []

with open('gadgets.txt', 'r') as file:
    for line in file:
        address_str, instructions = line.split(":", 1)
        instructions = instructions.strip()
        address = int(address_str, 16)
        modified_address = address + OFFSET
        gadgets.append([modified_address, instructions])


SYSTEM_OFFSET = 0x47258
SYSTEM = OFFSET + SYSTEM_OFFSET
addrs = [SYSTEM]

def encodable(i):
    for addr in addrs:
        print(f"Checking: {hex(addr+(i << 12))}")
        if(is_utf8_encodable(addr+(i << 12)) is not True):
            return False
    return True


for i in range(0x01, 1024):  # Loop from 0x01 to 0xFF
    if(encodable(i) is False):
        continue
   
    arr = []
    encodable_counts = 0
    print(f"ASLR offset: {hex(i)}")
    
    for gadget, text in gadgets:
        hex_int = gadget+(i << 12)
        if is_utf8_encodable(hex_int):
            encodable_counts += 1
            arr.append([hex(hex_int), text])
    
    if encodable_counts > 0:
        with open(f"gadgets_{i}.txt", "w") as f:
            for i in arr:
                f.write("%s\n" % i)
    
