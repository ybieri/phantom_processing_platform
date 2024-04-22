#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define TITLE_LEN 0x20
#define DESCRIPTION_SIZE 0x100
#define NUM_GHOST_TYPES DOPPELGANGER - ETHEREAL_VOYAGER
#define NO_KING -1
#define NUM_TRAITS 3
#define COMMAND_SUFFIX "END"
#define ADD_CMD "ADD"
#define DELETE_CMD "DEL"
#define SHOW_CMD "SHW"
#define EDIT_CMD "ED1"
#define EDIT_CMD_DATA "ED2"
#define ANALYZE_CMD "ALZ"
#define SUCCESS_CMD "SUC"
#define FAILURE_CMD "FAL"
#define SWITCH_MODE_CMD "MOD"
#define MARKER_SIZE 4
#define MAX_BUFFER_SIZE 4096 




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

typedef struct {
    GhostType type;
    const char* description;
} GhostDescription;

GhostDescription ghostStories[] = {
    {ETHEREAL_VOYAGER, "Wanders through realms, unseen by most"},
    {WHISPERING_SHADE, "Its faint murmurs chill the air with secrets long forgotten"},
    {POLTERGEIST, "Mischief echoes in empty halls"},
    {WRAITH, "Lost souls whisper in the night"},
    {SHADOW_FIGURE, "A dark silhouette watches silently"},
    {REVENANT, "Vengeance rises from the grave"},
    {SPECTRE, "Chilling presence felt, unseen"},
    {DOPPELGANGER, "Your mirror image, but twisted"}};



#define TRAITS_PER_GHOST 3
#define NUM_DEFAULT_GHOSTS 8

const char* ghostTraits[NUM_DEFAULT_GHOSTS][TRAITS_PER_GHOST] = {
    {"Traversing starlit paths", "Crossing the celestial seas", "Guiding lost souls"},                 
    {"Murmuring ancient secrets", "Whispering through the night", "Echoing in the silent forests"},    
    {"Misplacing objects with glee", "Knocking books from shelves", "Tapping on windows"},             
    {"Shrouded in misty veils", "Sighing in the wind", "Seeking vengeance"},                           
    {"Looming at the edge of sight", "Fleeing when noticed", "Watching from dark corners"},             
    {"Bound by unresolved fury", "Seeking its lost name", "Chained to this realm"},                     
    {"Haunting old battlefields", "Crying out in the night", "Clad in sorrow"},                         
    {"Mirroring your every move", "Stealing glimpses in the mirror", "Wearing your face in the crowd"}  
};

const char* getRandomTrait(int ghostType) {
    return ghostTraits[ghostType][rand() % TRAITS_PER_GHOST];
}


int appendRandomTrait(GhostPacket* packet) {
    const char* trait = NULL;

    if (packet->type >= ETHEREAL_VOYAGER && packet->type < NUM_DEFAULT_GHOSTS) {
        trait = getRandomTrait(packet->type);
    } else {
        return -1;
    }

    if (trait) {
        size_t currentLength = strlen(packet->description);
        size_t availableSpace = DESCRIPTION_SIZE - currentLength;

        int requiredLength = snprintf(NULL, 0, " ~ %s", trait) + 1;  
        if (requiredLength < availableSpace) {
            snprintf(packet->description + currentLength, availableSpace, " ~ %s", trait);
        }
        return 0;
    } else {
        return -1;
    }
}




typedef struct {
    GhostPacket** packets;  
    size_t size;
    size_t capacity;
} GhostPacketDatabase;

GhostType stringToGhostType(const char* str) {
    if (strcmp(str, "ethereal_voyager") == 0) return ETHEREAL_VOYAGER;
    if (strcmp(str, "whispering_shade") == 0) return WHISPERING_SHADE;
    if (strcmp(str, "poltergeist") == 0) return POLTERGEIST;
    if (strcmp(str, "wraith") == 0) return WRAITH;
    if (strcmp(str, "shadow_figure") == 0) return SHADOW_FIGURE;
    if (strcmp(str, "revenant") == 0) return REVENANT;
    if (strcmp(str, "spectre") == 0) return SPECTRE;
    if (strcmp(str, "doppelganger") == 0) return DOPPELGANGER;
    if (strcmp(str, "custom_ghost") == 0) return CUSTOM_GHOST;
    return -1;
}

const char* ghostTypeToString(GhostType type) {
    switch (type) {
        case ETHEREAL_VOYAGER:
            return "ethereal_voyager";
        case WHISPERING_SHADE:
            return "whispering_shade";
        case POLTERGEIST:
            return "poltergeist";
        case WRAITH:
            return "wraith";
        case SHADOW_FIGURE:
            return "shadow_figure";
        case REVENANT:
            return "revenant";
        case SPECTRE:
            return "spectre";
        case DOPPELGANGER:
            return "doppelganger";
        default:
            return "custom_ghost";
    }
}

GhostPacket createGhostPacket() {
    GhostPacket packet;                           
    memset(packet.title, 0, TITLE_LEN);               
    memset(packet.description, 0, DESCRIPTION_SIZE);  
    return packet;
}

void generateRandomString(char* str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    if (size) {
        --size; 
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof(charset) - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
}


int decode_utf8_char(char* bytes, int* len) {
    unsigned int code_point = 0;
    *len = 0;  

    if (bytes[0] < 128) {
        // 1-byte character (ASCII)
        *len = 1;
        code_point = bytes[0];
    } else if ((bytes[0] & 0xE0) == 0xC0) {
        // 2-byte character
        if ((bytes[1] & 0xC0) != 0x80) return 0;  
        code_point = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
        if (code_point < 0x80) return 0;  
        *len = 2;
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        // 3-byte character
        if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return 0;  
        code_point = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
        if (code_point < 0x800) return 0;  
        *len = 3;
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        // 4-byte character
        if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return 0;  
        code_point = ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) | ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
        if (code_point < 0x10000 || code_point > 0x10FFFF) return 0; 
        *len = 4;
    } else {
        return 0;
    }

    return code_point;
}

int isValidUtf8(char* string) {
    char* temp = string;
    int len = 0;

    while (*temp != '\0') {
        unsigned int code_point = decode_utf8_char(temp, &len);
        if (len <= 0 || code_point == 0) {
            return 0;
        }
        temp += len; 
    }
    return 1;
}

void initFunction() {
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    setbuf(stderr, NULL);
    srand(time(NULL));
}

void* readNBytes(int client_fd, size_t dataSize) {
    void* buffer = malloc(dataSize);
    if (buffer == NULL) {
        return NULL;
    }

    size_t readTotal = 0;  
    ssize_t bytesRead;     
    while (readTotal < dataSize) {
        bytesRead = read(client_fd, (char*)buffer + readTotal, dataSize - readTotal);
        if (bytesRead <= 0) {
            free(buffer);
            buffer = NULL;
            return NULL;
        }
        readTotal += bytesRead;
    }
    return buffer;
}

void sendCmd(int fd, char* command) {
    write(fd, command, MARKER_SIZE);
};

void sendGhostPacket(int sock_fd, GhostPacket* packet) {
    write(sock_fd, packet, sizeof(GhostPacket));
}

void sendId(int fd, uint16_t id) {
    write(fd, &id, sizeof(uint16_t));
}

void sendStatistics(int sock_fd, const char* data) {
    write(sock_fd, data, strlen(data) + 1);
}


void sendEnd(int fd) {
    int status = write(fd, COMMAND_SUFFIX, MARKER_SIZE);
    if (status <= 0) {
        exit(1);
    }
}

char* readCmd(int fd) {
    void* buffer = readNBytes(fd, MARKER_SIZE);
    if (buffer == NULL) {
        return NULL;
    } else {
        return buffer;
    }
}

int readId(int fd) {
    uint16_t id;
    int status = read(fd, &id, sizeof(id));
    if (status <= 0) {
        exit(1);
    }
    return id;
}

GhostPacket* readGhostPacket(int fd) {
    GhostPacket* packet = readNBytes(fd, sizeof(GhostPacket));
    if (packet == NULL) {
        return NULL;
    } else {
        return packet;
    }
}

int readEnd(int fd) {
    void* buffer = readNBytes(fd, MARKER_SIZE);
    if(buffer == NULL){
        return -1;
    }
    if (strcmp(buffer, "END") != 0) {
        exit(EXIT_FAILURE);
    } else {
        free(buffer);
        buffer = NULL;
        return 0;
    }
}