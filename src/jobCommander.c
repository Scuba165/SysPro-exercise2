#include <stdio.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <stdlib.h> 
#include <string.h> 
#include <signal.h>


int main(int argc, char* argv[]) {
    int socketFd;
    struct hostent* server;
    struct in_addr** adrr_list;
    char* symbolicIP;
    int portNum;

    // Ensure calling of program is correct.
    if(argc < 4) {
        printf("Usage: ./jobCommander [serverName] [portNum] [jobCommanderInputCommand]\n");
        exit(0);
    }
    if((server = gethostbyname(argv[1])) == NULL) {
        perror("Get host by name:");
        exit(1);
    } else {
        adrr_list = (struct in_addr**)server->h_addr_list;
        symbolicIP = malloc(sizeof(inet_ntoa(*adrr_list[0])));
        strcpy(symbolicIP, inet_ntoa(*adrr_list[0]));
    }
    portNum = atoi(argv[2]);
    struct sockaddr_in serverSocket;
    struct sockaddr* serverPtr = (struct sockaddr*)&serverSocket;

    // Create socket to connect to server.
    if((socketFd = socket( AF_INET , SOCK_STREAM , 0)) == -1) {
        perror("Socket creation:");
    }
    serverSocket.sin_family = AF_INET;
    memcpy(&serverSocket.sin_addr, server->h_addr_list[0], server->h_length);
    serverSocket.sin_port = htons(portNum);

    // Connect to socket.
    if(connect(socketFd, serverPtr, sizeof(serverSocket)) < 0) {
        perror("Socket connect:");
    }


    size_t length = 0;
    for (int i = 3; i < argc; i++) {
        length += strlen(argv[i]); // arg.
        length++;  // space.
    }
    char* job = malloc(length + 1);
    sprintf(job, "%s ", argv[3]);
    for (int i = 4; i < argc; i++) {
        strcat(job, argv[i]);
        if(i < argc - 1) {
            strcat(job, " ");
        }
    }
    char bytesToRead[12]; // Enough for any 32-bit int.
    sprintf(bytesToRead, "%d ", (int)strlen(job));

    int nwrite;
    if ((nwrite = write(socketFd, bytesToRead, strlen(bytesToRead))) == -1) { 
        perror("Error in Writing");
        exit(1);
    }
    if ((nwrite = write(socketFd, job, strlen(job))) == -1) { 
        perror("Error in Writing");
        exit(1);
    }
    if((shutdown(socketFd, SHUT_WR)) < 0) {
        perror("Shutdown:");
        exit(1);
    }


    char bytesToRead2[13] = ""; // enough to store any int.(byte size of command)
    char currentRead;
    int bytes = 0;
    while (read(socketFd, &currentRead, 1) > 0) {
        if (currentRead == ' ') {
            break;
        }
        bytesToRead2[bytes] = currentRead;
        bytes++;
    }
    bytesToRead2[bytes] = '\0';

    int responseSize = atoi(bytesToRead2);
    char* serverResponse = malloc(responseSize + 1); 
    memset(serverResponse, 0, responseSize + 1);
    if(read(socketFd, serverResponse, responseSize + 1) < 0) {
        off_t offset = lseek(socketFd, -responseSize, SEEK_END);
        if (offset == -1) {
            perror("lseek");
            exit(1);
        }
        perror("Read:");
        exit(1);
    }
    serverResponse[responseSize] = '\0';  // Null-terminate the response and print.
    printf("%s\n", serverResponse);

    if(strcmp(argv[3], "issueJob") == 0) { // Wait for job output.
        char outputSizeStr[13];
        bytes = 0;
        while (read(socketFd, &currentRead, 1) > 0) {
            if (currentRead == ' ') {
                break;
            }
            outputSizeStr[bytes] = currentRead;
            bytes++;
        }
        outputSizeStr[bytes] = '\0';
        int outputSize = atoi(outputSizeStr);
        char* jobOutput = malloc(outputSize + 1); 
        memset(jobOutput, 0,outputSize + 1);
        if(read(socketFd, jobOutput, outputSize + 1) < 0) {
            off_t offset = lseek(socketFd, -outputSize, SEEK_END);
            if (offset == -1) {
                perror("lseek");
                exit(1);
            }
            perror("Read:");
            exit(1);
        }
    jobOutput[outputSize] = '\0';  // Null-terminate the response
    printf("%s\n", jobOutput);
    free(jobOutput);
    }
    free(job);
    free(serverResponse);
    close(socketFd);
    exit(0);
}

