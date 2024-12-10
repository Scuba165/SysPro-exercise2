#include <stdio.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <unistd.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include "../include/jobExecutorServer.h"
#include "../include/workerThread.h"
#include "../include/controllerThread.h"

int main(int argc, char* argv[]) {
    jobID = 1;
    jobsInBuffer = 0;
    runningJobs = 0;
    concurrency = 1;
    serverExit = 0;
    // Ensure calling of program is correct.
    if(argc != 4) {
        printf("Wrong input! Correct usage: ./jobExecutorServer [portnum] [bufferSize] [threadPoolSize]\n");
        exit(1);
    }

    bufferSize = atoi(argv[2]);
    if(bufferSize <= 0) {
        printf("Wrong input! Buffer size must be a positive number.\n");
        exit(1);
    }
    threadPoolSize = atoi(argv[3]);
    if(threadPoolSize <= 0) {
        printf("Wrong input! Thread pool must be a positive number.\n");
        exit(1);
    }

    int portNum;
    struct sockaddr_in serverSocket, clientSocket;
    socklen_t clientLen;
    struct sockaddr* serverPtr = (struct sockaddr*)&serverSocket;
    struct sockaddr* clientPtr = (struct sockaddr*)&clientSocket;
    portNum = atoi(argv[1]);

    // Create socket.
    if((socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation:");
        exit(1);
    }
    serverSocket.sin_family = AF_INET; 
    serverSocket.sin_addr.s_addr = htonl(INADDR_ANY);
    serverSocket.sin_port = htons(portNum);
    if(bind(socketFd, serverPtr, sizeof(serverSocket)) < 0) {
        perror("Socket binding:");
        exit(1);
    }
    if(listen(socketFd, bufferSize) < 0)  {
        perror("Listen:");
        exit(1);
    }

    jobsBuffer = malloc(sizeof(Job*) * bufferSize);
    
    if(pthread_mutex_init(&bufferSem, NULL) < 0) {
        perror("pthread_mutex_init");
        exit(1);
    }
    if(pthread_mutex_init(&concurrencySem, NULL) < 0) {
        perror("pthread_mutex_init");
        exit(1);
    }
    if(pthread_cond_init(&bufferSpotCond, NULL) < 0) {
        perror("Condition Variable init:");
        exit(1);
    }
    if(pthread_cond_init(&bufferNewJobCond, NULL) < 0) {
        perror("Condition Variable init:");
        exit(1);
    }
    if(pthread_cond_init(&concurrencyCond, NULL) < 0) {
        perror("Condition Variable init:");
        exit(1);
    }

    // Create worker threads.
    worker_thread_id = malloc(sizeof(pthread_t) * threadPoolSize);
    for(int i = 0; i < threadPoolSize; i++) {
        if(pthread_create(&worker_thread_id[i], NULL, &worker_thread, NULL) < 0) {
            perror("Worker thread creation:");
            exit(1);
        }
    }

    while(1) {
        acceptFd = malloc(sizeof(int)); 
        if ((*acceptFd = accept(socketFd, clientPtr, &clientLen)) < 0) {
            if(serverExit == 1) {
                break;
            }
            perror("Accept:");
            exit(1);
        }
        pthread_t controller_thread_id;
        if((pthread_create(&controller_thread_id, NULL, &controller_thread, (void*)acceptFd)) < 0) {
            perror("Controller thread creation:");
            exit(1);
        }
        if((pthread_detach(controller_thread_id)) < 0) {
            perror("Controller thread detach:");
            exit(1);
        }
    }
    // Out of loop == serverExit
    pthread_cond_broadcast(&concurrencyCond);
    // Wait for workers to terminate and free memory.
    for(int i = 0; i < threadPoolSize; i++) {
        if(pthread_join(worker_thread_id[i], NULL) < 0) {
            perror("Worker thread waiting");
            exit(1);
        }
    }
    free(worker_thread_id);
    // Send termination message to waiting clients.
    int length = strlen("SERVER TERMINATED BEFORE EXECUTION");
    char* responseBuf = malloc(length + 1);
    sprintf(responseBuf, "SERVER TERMINATED BEFORE EXECUTION");
    char responseSize[13]; //int
    sprintf(responseSize, "%ld ", strlen(responseBuf));
    for(int i = 0; i < jobsInBuffer; i++) {
        Job* currJob = jobsBuffer[i];
        int currSocketFd = currJob->socketFd;
        if(write(currSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(currSocketFd, responseBuf, strlen(responseBuf)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        close(currSocketFd);
        free(currJob);
    }
    // Free last pieces and exit.
    free(responseBuf);
    free(jobsBuffer);
    exit(0);
}









char* getJobDouble(Job* job) {
    int length = snprintf(NULL, 0, "<%d, %s>\n", job->id, job->arguments);
    char* returnStr = malloc(length + 1);
    memset(returnStr, 0, length + 1);
    if (returnStr == NULL) {
        perror("malloc:");
        exit(1);
    }
    int id = job->id;
    char* args = job->arguments;
    sprintf(returnStr, "<%d, %s>\n", id, args);
    return returnStr;
}

char* getJobTriple(Job* job) {
    int length = snprintf(NULL, 0, "<%d, %s, %d>", job->id, job->arguments, job->socketFd);
    char* returnStr = malloc(length + 1);
    memset(returnStr, 0, length + 1);
    if (returnStr == NULL) {
        perror("malloc:");
        exit(1);
    }
    int id = job->id;
    char* args = job->arguments;
    int socketFd = job->socketFd;
    sprintf(returnStr, "<%d, %s, %d>", id, args, socketFd);
    return returnStr;
}