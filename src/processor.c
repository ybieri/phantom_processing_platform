#include <limits.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "packet.h"

#define INITIAL_CAPACITY 10
#define EMOJI_99 "😁👻"
#define EMOJI_75 "🤔👻"
#define EMOJI_50 "😐👻"
#define EMOJI_5 "😕👻"
#define EMOJI_0 "☹️👻"


int is_modern = 0;
char* SOCKET_PATH;

int verifyUTF8String(const char* str, size_t len);
int processCommand(int client_fd, GhostPacketDatabase* db);

void initializeOrExpandDatabase(GhostPacketDatabase* db) {
    if (db->packets == NULL) {
        db->capacity = INITIAL_CAPACITY;
        db->size = 0;
        db->packets = malloc(db->capacity * sizeof(GhostPacket*));
        for (size_t i = 0; i < db->capacity; i++) {
            db->packets[i] = NULL;
        }
    } else {
        size_t oldCapacity = db->capacity;
        db->capacity *= 2;
        db->packets = realloc(db->packets, db->capacity * sizeof(GhostPacket*));
        for (size_t i = oldCapacity; i < db->capacity; i++) {
            db->packets[i] = NULL;
        }
    }
}

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

int addPacket(GhostPacketDatabase* db, GhostPacket* packet) {
    if (db->packets == NULL || db->size >= db->capacity) {
        initializeOrExpandDatabase(db);
        if (db->packets == NULL) {
            return -1;
        }
    }

    appendEmoji(packet);

    uint16_t slotToUse = UINT16_MAX;
    for (size_t i = 0; i < db->capacity; i++) {
        if (db->packets[i] == NULL) {
            slotToUse = i;
            break;
        }
    }

    if (slotToUse == UINT16_MAX) {
        return -1; 
    }

    db->packets[slotToUse] = malloc(sizeof(GhostPacket));
    if (db->packets[slotToUse] == NULL) {
        return -1;
    }
    packet->id = slotToUse;
    *(db->packets[slotToUse]) = *packet;

    if (slotToUse == db->size) {
        db->size++;
    }

    return slotToUse;
}

int deletePacket(GhostPacketDatabase* db, uint16_t id) {
    if (id < db->size && db->packets[id] != NULL) {
        free(db->packets[id]);
        db->packets[id] = NULL;
        return 0;
    }
    return -1;
}

void editPacket(GhostPacketDatabase* db, uint16_t id, GhostPacket* newPacketData) {
    if (id < db->size && db->packets[id] != NULL) {
        newPacketData->id = id;
        *db->packets[id] = *newPacketData;
        return;
    }
}

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

void finalizeCalculationsAndPrint(const int sightingCount, const float totalLatitude, const float totalLongitude,
                                  const float totalConfidence, const unsigned long firstSighting, const unsigned long lastSighting,
                                  int traitOccurrences[NUM_DEFAULT_GHOSTS][TRAITS_PER_GHOST], char* buffer) {
    float averageLatitude = sightingCount > 0 ? totalLatitude / sightingCount : 0;
    float averageLongitude = sightingCount > 0 ? totalLongitude / sightingCount : 0;
    float averageConfidence = sightingCount > 0 ? totalConfidence / sightingCount : 0;

    int offset = 0;
    if (is_modern) {
        offset += snprintf(buffer + offset, MAX_BUFFER_SIZE - offset,
                           "👻 Overall Ghost Packet Statistics: 👻\n"
                           "  First Sighting: %lu\n"
                           "  Last Sighting: %lu\n"
                           "  Average Latitude: %.2f\n"
                           "  Average Longitude: %.2f\n"
                           "  Average Confidence: %.2f%%\n"
                           "  Total Sightings: %d\n",
                           firstSighting, lastSighting,
                           averageLatitude, averageLongitude,
                           averageConfidence, sightingCount);
    } else {
        offset += snprintf(buffer + offset, MAX_BUFFER_SIZE - offset,
                           "+ Overall Ghost Packet Statistics: +\n"
                           "  First Sighting: %lu\n"
                           "  Last Sighting: %lu\n"
                           "  Average Latitude: %.2f\n"
                           "  Average Longitude: %.2f\n"
                           "  Average Confidence: %.2f%%\n"
                           "  Total Sightings: %d\n",
                           firstSighting, lastSighting,
                           averageLatitude, averageLongitude,
                           averageConfidence, sightingCount);
    }

    if (offset >= MAX_BUFFER_SIZE) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_DEFAULT_GHOSTS && offset < MAX_BUFFER_SIZE; i++) {
        offset += snprintf(buffer + offset, MAX_BUFFER_SIZE - offset,
                           "%s Trait Counts:\n", ghostTypeToString(i));
        for (int j = 0; j < TRAITS_PER_GHOST && offset < MAX_BUFFER_SIZE; j++) {
            offset += snprintf(buffer + offset, MAX_BUFFER_SIZE - offset,
                               "  Trait '%s': %d occurrences\n",
                               ghostTraits[i][j], traitOccurrences[i][j]);
        }
    }

    if (offset > MAX_BUFFER_SIZE) {
        exit(EXIT_FAILURE);
    }
}

void analyzeData(GhostPacketDatabase* db, char* buffer) {
    int traitOccurrences[NUM_DEFAULT_GHOSTS][TRAITS_PER_GHOST] = {0};
    GhostPacket* packet;
    unsigned long firstSighting = ULONG_MAX;
    unsigned long lastSighting = 0;
    float totalLatitude = 0.0f, totalLongitude = 0.0f, totalConfidence = 0.0f;
    int sightingCount = 0;

    for (size_t i = 0; i < db->size; i++) {
        packet = db->packets[i];

        if (packet == NULL) {
            continue;
        }

        sightingCount++;

        totalLatitude += packet->latitude;
        totalLongitude += packet->longitude;
        totalConfidence += packet->confidence;

        if (packet->timestamp < firstSighting) firstSighting = packet->timestamp;
        if (packet->timestamp > lastSighting) lastSighting = packet->timestamp;
        extractAndMatchTraits(packet, traitOccurrences);
    }

    finalizeCalculationsAndPrint(sightingCount, totalLatitude, totalLongitude, totalConfidence,
                                 firstSighting, lastSighting, traitOccurrences, buffer);
}

int start_socket(GhostPacketDatabase* db) {
    int server_fd, client_fd;
    struct sockaddr_un server_addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    puts("Listening for connections...\n");

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    puts("Connection accepted.\n");

    int status = -1;
    do {
        status = processCommand(client_fd, db);
    } while (status == 0);

    close(client_fd);
    puts("Connection closed. Listening for new connections...\n");
    return 0;
}

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

int verifyUTF8String(const char* str, size_t len) {
    char* bytes = (char*)str;
    size_t i = 0;
    while (i < len) {
        int charLen = 0;
        decode_utf8_char(bytes + i, &charLen);
        if (charLen == 0) {
            return -1;
        }
        i += charLen;
    }
    return 0;
}

void sendFailure(int client_fd, uint16_t id) {
    sendCmd(client_fd, FAILURE_CMD);
    sendId(client_fd, id);
    sendEnd(client_fd);
}

int processCommand(int client_fd, GhostPacketDatabase* db) {
    char* prefix = readCmd(client_fd);
    if (prefix == NULL) {
        return -1;
    }

    //printf("Received prefix: %s\n", prefix);

    if (strcmp(prefix, ADD_CMD) == 0) {
        //printf("Received ADD command.\n");
        GhostPacket* packet = readGhostPacket(client_fd);
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }

        status = validateGhostPacket(packet);
        //printf("Status: %d\n", status);

        if (status == 0) {
            uint16_t id = addPacket(db, packet);
            if (id >= 0) {
                //printf("Packet added to database with ID %d.\n", id);
                sendCmd(client_fd, SUCCESS_CMD);
                sendId(client_fd, packet->id);
                sendEnd(client_fd);
            } else {
                sendFailure(client_fd, id);
            }
        } else {
            //printf("Failed to validate packet.\n");
            return -1;
        }
    } else if (strcmp(prefix, DELETE_CMD) == 0) {
        //printf("Received DELETE command.\n");

        uint16_t id = readId(client_fd);
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }
        //printf("Received ID for delete: %hu\n", id);
        if (id >= db->size || db->packets[id] == NULL) {
            sendFailure(client_fd, id);
            return 0;
        }
        if (deletePacket(db, id) == 0) {
            //printf("Packet with ID %hu deleted successfully.\n", id);
            sendCmd(client_fd, SUCCESS_CMD);
            sendId(client_fd, id);
            sendEnd(client_fd);
        } else {
            //printf("Sendin failure in delete\n");
            sendFailure(client_fd, id);
        }
    } else if (strcmp(prefix, SHOW_CMD) == 0) {
        //printf("Received SHOW command.\n");

        uint16_t id = readId(client_fd);
        //printf("Received ID: %hu\n", id);
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }

        if (id < db->size && db->packets[id] != NULL) {
           //printf("Packet with ID %hu exists.\n", id);
            sendCmd(client_fd, SUCCESS_CMD);
            sendGhostPacket(client_fd, db->packets[id]);
            sendEnd(client_fd);
        } else {
            //printf("SHOW: Packet with ID %hu does not exist.\n", id);
            sendFailure(client_fd, id);
        }
    } else if (strcmp(prefix, EDIT_CMD) == 0) {
        //printf("Received EDIT command.\n");
        uint16_t id = readId(client_fd);
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }
        //printf("Received ID: %hu\n", id);
        if (id < db->size && db->packets[id] != NULL) {
            //printf("Packet with ID %hu exists.\n", id);
            sendCmd(client_fd, SUCCESS_CMD);
            sendGhostPacket(client_fd, db->packets[id]);
            sendEnd(client_fd);

            char* prefix2 = readCmd(client_fd);
            if (strcmp(prefix2, EDIT_CMD_DATA) == 0) {
                free(prefix2);
                prefix2 = NULL;

                //printf("Received EDIT_CMD_DATA.\n");
                GhostPacket* packet = readGhostPacket(client_fd);
                int status = readEnd(client_fd);
                if (status == -1) {
                    return -1;
                }

                status = validateGhostPacket(packet);
                //printf("Status: %d\n", status);
                //printf("Packet description: %s\n", packet->description);

                if (status == 0) {
                    editPacket(db, id, packet);
                    //printf("Packet with ID %hu edited successfully.\n", id);
                    sendCmd(client_fd, SUCCESS_CMD);
                    sendId(client_fd, id);
                    sendEnd(client_fd);
                } else {
                    sendFailure(client_fd, id);
                }
            } else {
                //printf("Invalid command format.\n");
                exit(EXIT_FAILURE);
            }
        } else {
            sendFailure(client_fd, id);
        }
    } else if (strcmp(prefix, ANALYZE_CMD) == 0) {
        //printf("Received ANALYZE command.\n");
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }

        char* buffer = malloc(MAX_BUFFER_SIZE);
        analyzeData(db, buffer);
        //printf("Buffer: %s\n", buffer);
        sendCmd(client_fd, SUCCESS_CMD);
        sendStatistics(client_fd, buffer);
        sendEnd(client_fd);
        free(buffer);
        buffer = NULL;
    } else if (strcmp(prefix, SWITCH_MODE_CMD) == 0) {
        //printf("Received SWITCH command.\n");
        int status = readEnd(client_fd);
        if (status == -1) {
            return -1;
        }
        is_modern = !is_modern;
        sendCmd(client_fd, SUCCESS_CMD);
        sendEnd(client_fd);
    } else {
        //printf("Invalid command format.\n");
        exit(EXIT_FAILURE);
    }

    free(prefix);
    prefix = NULL;
    return 0;
}



void assignDefaultGhostDescriptionToPacket(GhostPacket* packet) {
    const char* description = ghostStories[packet->type].description;
    strncpy(packet->description, description, DESCRIPTION_SIZE - 1);
    packet->description[DESCRIPTION_SIZE - 1] = '\0';
    appendRandomTrait(packet);
}


const char* ghostSightingsTitles[] = {
    "Whispers",
    "Midnight Shadows",
    "Forgotten Apparition",
    "Eerie Lights",
    "Old Mill Specter",
    "Empty Hall Steps",
    "Weeping Phantom",
    "Spectral Chill",
    "Vanishing Mystery",
    "Crossroads Haunt"
};

void generateRandomGhostPackets(GhostPacketDatabase* db, int numberOfPackets) {
    const int titlesCount = sizeof(ghostSightingsTitles) / sizeof(ghostSightingsTitles[0]);
    
    for (uint16_t i = 0; i < numberOfPackets; i++) {
        GhostPacket packet;

        // Randomly assign values to each field in the GhostPacket
        packet.type = (rand() % (DOPPELGANGER - ETHEREAL_VOYAGER + 1)) + ETHEREAL_VOYAGER;
        packet.timestamp = time(NULL) - rand() % 100000;
        packet.latitude = ((float)rand() / (float)(RAND_MAX)) * 180.0f - 90.0f;
        packet.longitude = ((float)rand() / (float)(RAND_MAX)) * 360.0f - 180.0f;
        packet.confidence = rand() % 101;

        int titleIndex = rand() % titlesCount;
        strncpy(packet.title, ghostSightingsTitles[titleIndex], TITLE_LEN - 1);
        packet.title[TITLE_LEN - 1] = '\0'; // Ensure null-termination
        assignDefaultGhostDescriptionToPacket(&packet);

        addPacket(db, &packet);
    }
}

int main(int argc, char *argv[]) {
     if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    SOCKET_PATH = argv[1];

    initFunction();

    
    GhostPacketDatabase db = {0};

    generateRandomGhostPackets(&db, 100);

    start_socket(&db);

    return 0;
}
