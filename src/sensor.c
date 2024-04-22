#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "packet.h"


#define EMOJI_99 "😁👻"
int is_modern = 0;
char* SOCKET_PATH;
int sock_fd;
int readResponse(const char* command);



int readInput(char* buffer, size_t maxSize) {
    int bytesRead = read(0, buffer, maxSize - 1);  
    if (bytesRead < 0) {
        exit(1);  
    }

    if (bytesRead == 0) {
        buffer[0] = '\0';  
        return 0;          
    }

    if (buffer[bytesRead - 1] == '\n') {
        buffer[bytesRead - 1] = '\0';  
    } else {
        buffer[bytesRead] = '\0';  
    }

    return bytesRead;  
}





void assignDefaultGhostDescriptionToPacket(GhostPacket* packet) {
    const char* description = ghostStories[packet->type].description; 
    strncpy(packet->description, description, DESCRIPTION_SIZE - 1);
    packet->description[DESCRIPTION_SIZE - 1] = '\0';
    appendRandomTrait(packet);
}

void assignCustomGhostDescriptionToPacket(GhostPacket* packet) {
    printf("Enter description for the custom ghost: ");
    readInput(packet->description, DESCRIPTION_SIZE);
}

void assignDescriptionToPacket(GhostPacket* packet) {
    if (packet->type >= ETHEREAL_VOYAGER && packet->type <= DOPPELGANGER) {
        assignDefaultGhostDescriptionToPacket(packet);
    } else {
        assignCustomGhostDescriptionToPacket(packet);
    }
}

int readPacket(GhostPacket* packet) {

    printf("Enter title of your ghost observation report: \n");
    if (readInput(packet->title, TITLE_LEN-strlen(EMOJI_99)) <= 0) {
        printf("Failed to read title.\n");
        return -1;
    }


    printf("Enter timestamp of your ghost observation (unix epoch): \n");
    scanf("%u", &packet->timestamp);
    if (packet->timestamp > time(NULL)) {
        printf("Invalid timestamp.\n");
        return -1;
    }

    printf("Enter latitude and longitude of your ghost observation: \n");
    scanf("%f %f", &packet->latitude, &packet->longitude);

    if (packet->latitude < -90.0 || packet->latitude > 90.0) {
        printf("Invalid latitude. It must be between -90 and 90.\n");
        return -1;
    }

    if (packet->longitude < -180.0 || packet->longitude > 180.0) {
        printf("Invalid longitude. It must be between -180 and 180.\n");
        return -1;
    }

    printf("Enter confidence level of your ghost observation (0-100): \n");
    scanf("%hhu", &packet->confidence);

    if (packet->confidence < 0 || packet->confidence > 100) {
        printf("Invalid confidence level.\n");
        return -1;
    }
    getchar();

    char typeInput[0x20];  
    printf("Enter ghost type (poltergeist, wraith, shadow_figure, revenant, spectre, doppelganger, custom_ghost): \n");
    if (readInput(typeInput, sizeof(typeInput)) <= 0) {
        printf("Failed to read ghost type.\n");
        return -1;
    }
    int8_t type = stringToGhostType(typeInput);

    if (type == -1) {
        printf("Invalid ghost type.\n");
        return -1;
    }
    packet->type = type;

    assignDescriptionToPacket(packet);

    return 0;
}


void showMenu() {
    printf("\nGhost Data Management System\n");
    printf("1. Add Ghost Data\n");
    printf("2. Show Ghost Data\n");
    printf("3. Edit Ghost Data\n");
    printf("4. Delete Ghost Data\n");
    printf("5. Analyze Ghost Data\n");
    printf("6. Switch Mode\n");
    printf("7. Exit\n");
    printf("\n");
    printf("Enter your choice: ");
}

void readGhostData() {
    GhostPacket ghost = createGhostPacket();

    if (readPacket(&ghost) == 0) {
        sendCmd(sock_fd, ADD_CMD);
        sendGhostPacket(sock_fd, &ghost);
        sendEnd(sock_fd);

        char* cmd = readCmd(sock_fd);
        if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {

            u_int16_t id = readId(sock_fd);
            if (id != -1) {
                printf("Successfully added ghost data with ID %hu.\n", id);
            } else {
                printf("Failed to add ghost data.\n");
            }
            int status = readEnd(sock_fd);
            if (status == -1) {
                return;
            }
        } else {
            printf("Failed to add ghost data.\n");
        }
    } else {
        printf("Failed to add ghost data.\n");
    }
}

int editGhostDataPacket(GhostPacket* packet) {
    printf("Enter new title of your ghost observation report: \n");
    if (readInput(packet->title, TITLE_LEN) <= 0) {
        return -1;
    }

    if (packet->type >= ETHEREAL_VOYAGER && packet->type <= DOPPELGANGER) {
        printf("This is a default ghost type. Description cannot be changed.\n");
        printf("Do you want to change the type instead? (y/n): ");

        char choice;
        scanf("%c", &choice);
        getchar();  
        if (choice == 'y') {
            char typeInput[0x20];  
            printf("Enter ghost type (poltergeist, wraith, shadow_figure, revenant, spectre, doppelganger): \n");
            if (readInput(typeInput, sizeof(typeInput)) <= 0) {
                printf("Failed to read ghost type.\n");
                return -1;
            }
            int8_t type = stringToGhostType(typeInput);
            if (type == -1) {
                printf("Invalid ghost type.\n");
                return -1;  
            } else {
                packet->type = type;
            }
            assignDefaultGhostDescriptionToPacket(packet);
        }
    } else {
        assignCustomGhostDescriptionToPacket(packet);
    }
    printf("Ghost data updated.\n");
    return 0;
}


int handleSuccess(char* cmd, uint16_t index) {
    if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {
        int id = readId(sock_fd);
        if (id != -1) {
            printf("Successfully edited ghost data with ID %hu.\n", id);
        } else {
            printf("Failed to edit ghost data.\n");
        }
        readEnd(sock_fd);
        return 0;
    } else {
        printf("Failed to edit ghost data.\n");
        return -1;
    }
}

int editGhostData() {
    uint16_t index; 
    printf("Enter the id of the ghost data to edit: ");
    scanf("%hu", &index); 
    getchar();           

    sendCmd(sock_fd, EDIT_CMD);
    sendId(sock_fd, index);
    sendEnd(sock_fd);

    char* cmd = readCmd(sock_fd);
    GhostPacket* packet;

    if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {
        //printf("Successfully read ghost data with ID %hu.\n", index);
        packet = readGhostPacket(sock_fd);
        int status = readEnd(sock_fd);
        if (status == -1) {
            return -1;
        }
        if (editGhostDataPacket(packet) == 0) {
            sendCmd(sock_fd, EDIT_CMD_DATA);
            sendGhostPacket(sock_fd, packet);
            sendEnd(sock_fd);

            char* cmd = readCmd(sock_fd);
            handleSuccess(cmd, index);
        }
        free(packet);
        packet = NULL;

    } else {
        uint16_t idx = readId(sock_fd);
        int status = readEnd(sock_fd);
        if (status == -1) {
            return -1;
        }
        printf("Failed to show ghost data with ID %hu.\n", idx);
    }

    return 0;
}


void printGhostData(GhostPacket* packet) {
    printf("Title: %s\n", packet->title);
    printf("Timestamp: %u\n", packet->timestamp);
    printf("Latitude: %f\n", packet->latitude);
    printf("Longitude: %f\n", packet->longitude);
    printf("Confidence: %hhu\n", packet->confidence);
    printf("Type: %s\n", ghostTypeToString(packet->type));
    printf("Description: %s\n", packet->description);
}


int handleResponse(char* buffer) {
    uint16_t id;
    if (sscanf(buffer, "SUCCESS %hu", &id) == 1) {
        return 0;
    } else if (sscanf(buffer, "FAILURE %hu", &id) == 1) {
        return -1;
    } else {
        return -1;
    }
}

int readResponse(const char* command) {
    char buffer[1024];
    ssize_t numRead = read(sock_fd, buffer, sizeof(buffer) - 1);
    if (numRead <= 0) {
        exit(1);
    }
    buffer[numRead] = '\0';
    if (command != NULL && (strcmp(command, DELETE_CMD) == 0 || strcmp(command, ADD_CMD) == 0)) {
        return handleResponse(buffer);
    } else if (command != NULL && (strcmp(command, SHOW_CMD) == 0)) {
        int result = handleResponse(buffer);
        if (result == -1) {
            return -1;
        }

        GhostPacket* packet = readNBytes(sock_fd, sizeof(GhostPacket));
        if (packet == NULL) {
            return -1;
        }
        printGhostData(packet);
        free(packet);
        packet = NULL;
        return 0;
    }
    return -1;
}

void deleteGhostData() {
    int index;
    printf("Enter the index of the ghost data to delete: ");
    scanf("%d", &index);
    getchar(); 

    sendCmd(sock_fd, DELETE_CMD);
    sendId(sock_fd, index);
    sendEnd(sock_fd);

    char* cmd = readCmd(sock_fd);
    int id = readId(sock_fd);
    int status = readEnd(sock_fd);
    if (status == -1) {
        return;
    }

    if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {
        if (id != -1) {
            printf("Successfully deleted ghost data with ID %d.\n", id);
        }
    } else {
        printf("Failed to delete ghost data with ID %d.\n", id);
    }
    free(cmd);
    cmd = NULL;
}

void showGhostData() {
    int index;
    printf("Enter the index of the ghost data to show: ");
    scanf("%d", &index);
    getchar();  

    sendCmd(sock_fd, SHOW_CMD);
    sendId(sock_fd, index);
    sendEnd(sock_fd);

    char* cmd = readCmd(sock_fd);
    //printf("Read command %s\n", cmd);
    if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {
        printf("Successfully read ghost data with ID %d.\n", index);
        GhostPacket* packet = readGhostPacket(sock_fd);
        printGhostData(packet);
        free(packet);
        packet = NULL;
    } else {
        int id = readId(sock_fd);
        printf("Failed to show ghost data with ID %d.\n", id);
    }
    int status = readEnd(sock_fd);
    if (status == -1) {
        return;
    }
    free(cmd);
    cmd = NULL;
}


void switch_mode(){
    sendCmd(sock_fd, SWITCH_MODE_CMD);
    sendEnd(sock_fd);

    char* cmd = readCmd(sock_fd);
    int status = readEnd(sock_fd);
    if(status == -1){
        return;
    }

    if(cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0){
        printf("Successfully switched mode.\n");
        is_modern = !is_modern;
    } else {
        printf("Failed to switch mode.\n");
    }
    free(cmd);
    cmd = NULL;
}

void switch_classic_modern(){
    char* mode = is_modern == 0 ? "modern" : "classic";
    printf("Do you want to switch to %s mode? (y/n): ", mode);

    char choice;
    scanf("%c", &choice);
    getchar();  

    if (choice == 'y') {
        switch_mode();
    }
}


void receiveAndPrintStatistics(int sock_fd) {
    char buffer[MAX_BUFFER_SIZE] = {0};  
    size_t totalRead = 0;
    int endFound = 0;

    while (!endFound && totalRead < MAX_BUFFER_SIZE - 1) {
        char ch;
        ssize_t bytesRead = read(sock_fd, &ch, 1);  
        if (bytesRead <= 0) {
            return;
        }

        buffer[totalRead++] = ch;  
        if (totalRead >= 3 && strncmp(&buffer[totalRead - 3], COMMAND_SUFFIX, 3) == 0) {
            endFound = 1;
            buffer[totalRead - 3] = '\0';  
        }
    }

    int status = read(sock_fd, &buffer[totalRead], 1);
    if (status <= 0) {
        exit(1);
    }
    if (endFound) {
        printf("%s\n", buffer);
    }
}


void analyzeGhostData(){
    printf("Analyzing ghost data...\n");
    sendCmd(sock_fd, ANALYZE_CMD);
    sendEnd(sock_fd);

    char* cmd = readCmd(sock_fd);
    if (cmd != NULL && strcmp(cmd, SUCCESS_CMD) == 0) {
        receiveAndPrintStatistics(sock_fd);
    }
}


int setup_socket() {
    struct sockaddr_un server_addr;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        exit(1);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_un)) == -1) {
        exit(1);
    }
    return 0;
}

int close_socket(int sock_fd) {
    close(sock_fd);
    return 0;
}


const char* getGhostArt =
"⠀⠀⠀⠀⠀⠀⢀⣤⣶⣶⣖⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⢀⣾⡟⣉⣽⣿⢿⡿⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⢠⣿⣿⣿⡗⠋⠙⡿⣷⢌⣿⣿⠀⠀⠀⠀⠀⠀⠀\n"
"⣷⣄⣀⣿⣿⣿⣿⣷⣦⣤⣾⣿⣿⣿⡿⠀⠀⠀⠀⠀⠀⠀\n"
"⠈⠙⠛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⡀⠀⢀⠀⠀⠀⠀\n"
"⠀⠀⠀⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⠻⠿⠿⠋⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⠹⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀\n"
"⠀⠀⠀⠀⠀⠈⢿⣿⣿⣿⣿⣿⣿⣇⠀⠀⠀⠀⠀⠀⠀⡄\n"
"⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠀⢀⡾⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣿⣿⣿⣿⣷⣶⣴⣾⠏⠀⠀\n"
"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠛⠛⠛⠋⠁⠀⠀⠀\n";




int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    SOCKET_PATH = argv[1];

    initFunction();
    setup_socket();

    printf("Welcome to the Phantom Processing Platform\n");
    printf("\n%s", getGhostArt);
    

    int choice;
    while (1) {
        showMenu();
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1:
                readGhostData();
                break;
            case 2:
                showGhostData();
                break;
            case 3:
                editGhostData();
                break;
            case 4:
                deleteGhostData();
                break;
            case 5:
                analyzeGhostData();
                break; 
            case 6:
                switch_classic_modern();
                break;
            case 7:
                printf("Exiting.\n");
                close_socket(sock_fd);
                return 0;
            default:
                printf("Invalid choice. Please enter a number between 1 and 7.\n");
        }
    }
    return 0;
}
