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
#include "../include/controllerThread.h"

void* controller_thread(void* arg) {
    int* newSocketFd = (int*)arg;
    // Read Socket and prepare to insert job in buffer.
    char bytesToRead[13] = ""; 
    char currentRead;
    int bytes = 0;
    while(read(*newSocketFd, &currentRead, 1) > 0) {
        if(currentRead == ' ') {
            break;
        }
        bytesToRead[bytes] = currentRead;
        bytes++;
    }
    strcat(bytesToRead, "\0");

    char* commanderInput = malloc(atoi(bytesToRead));
    memset(commanderInput, 0, atoi(bytesToRead) + 1);
    if(read(*newSocketFd, commanderInput, atoi(bytesToRead)) < 0) {
        off_t offset = lseek(*newSocketFd, -atoi(bytesToRead), SEEK_END);
        if (offset == -1) {
            perror("lseek");
            exit(1);
        }
        perror("Read:");
        exit(1);
    }
    char* commanderJob = strtok(commanderInput, " ");
    char* commanderArgs = strtok(NULL, "\0");

    pthread_mutex_lock(&bufferSem); // lock buffer for all operations.
    if(strcmp(commanderJob, "issueJob") == 0) {
        Job* newJob = malloc(sizeof(Job));
        newJob->id = jobID;
        jobID++;
        newJob->arguments = commanderArgs;
        newJob->socketFd = *newSocketFd;
        while(jobsInBuffer == bufferSize) {
            pthread_cond_wait(&bufferSpotCond, &bufferSem);
        }
        for(int i = 0; i < bufferSize; i++) {
            if(jobsBuffer[i] == NULL) {
                jobsBuffer[i] = newJob;
                jobsInBuffer++;
                break;
            }
        }
        char* triple = getJobTriple(newJob);
        int length = snprintf(NULL, 0, "JOB %s SUBMITTED", triple);
        char* responseBuf = malloc(length + 1);
        sprintf(responseBuf, "JOB %s SUBMITTED", triple);
        char responseSize[13]; 
        sprintf(responseSize, "%ld ", strlen(responseBuf));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, responseBuf, strlen(responseBuf)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        free(triple);
        free(responseBuf);
        pthread_cond_signal(&bufferNewJobCond);
        pthread_cond_signal(&concurrencyCond);
        pthread_mutex_unlock(&bufferSem); 

    }
    else if(strcmp(commanderJob, "poll") == 0) {
        size_t totalLen = 1; 
        for(int i = 0; i < jobsInBuffer; i++) {
            Job* currJob = jobsBuffer[i];
            char* JobDouble = getJobDouble(currJob);
            totalLen += strlen(JobDouble);
            free(JobDouble);
        }
        char* response;
        if(totalLen == 1) {
            size_t size = strlen("NO JOBS WAITING");
            response = malloc(size);
            sprintf(response, "NO JOBS WAITING");
        } else {
            response = malloc(totalLen);
            if(response == NULL) {
                perror("malloc:");
                exit(1);
            }
            for(int i = 0; i < jobsInBuffer; i++) {
                Job* currJob = jobsBuffer[i];
                char* JobDouble = getJobDouble(currJob);
                strcat(response, JobDouble);
            }
        }
        char responseSize[13];
        sprintf(responseSize, "%ld ", strlen(response));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, response, strlen(response)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        free(response);
        pthread_mutex_unlock(&bufferSem); 
    }
    else if(strcmp(commanderJob, "setConcurrency") == 0) {
        pthread_mutex_lock(&concurrencySem); //acquire concurrency lock.
        int newConc = atoi(commanderArgs);
        int oldConc = concurrency;
        concurrency = newConc;
        if(newConc > oldConc) {
            pthread_cond_broadcast(&concurrencyCond); // if conc is increased, broadcast variable
        }
        pthread_mutex_unlock(&concurrencySem);

        int length = snprintf(NULL, 0, "CONCURRENCY SET AT %d", newConc);
        char responseSize[13];
        char* response = malloc(length);
        if(response == NULL) {
            perror("malloc:");
            exit(1);
        }
        sprintf(response, "CONCURRENCY SET AT %d", newConc);
        sprintf(responseSize, "%ld ", strlen(response));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, response, strlen(response)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        free(response);
        pthread_mutex_unlock(&bufferSem); 
    }
    else if(strcmp(commanderJob, "stop") == 0) {
        int removed = 0;
        int removeID = atoi(commanderArgs);
        for(int i = 0; i < jobsInBuffer; i++) {
            Job* currJob = jobsBuffer[i];
            printf("currjobid %d\n", currJob->id);
            if(currJob->id == removeID) {
                // Shift remaining jobs and enter NULL.
                for(int j = i + 1; j < jobsInBuffer; j++) {
                    jobsBuffer[j - 1] = jobsBuffer[j];
                }
                jobsBuffer[jobsInBuffer - 1] = NULL;
                removed = 1;
                jobsInBuffer--;
                free(currJob);
                break;
            }
        }
        int length = snprintf(NULL, 0, "JOB %d ", removeID);
        if(removed) {
            length += strlen("REMOVED");
        } else {
            length += strlen("NOTFOUND");
        }
        char responseSize[13];
        char* response = malloc(length);
        if(response == NULL) {
            perror("malloc:");
            exit(1);
        }
        sprintf(response, "JOB %d ", removeID);
        if(removed) {
            strcat(response, "REMOVED");
        } else {
            strcat(response, "NOTFOUND");
        }
        sprintf(responseSize, "%ld ", strlen(response));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, response, strlen(response)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        free(response);
        if(removed) {
            pthread_cond_signal(&bufferSpotCond);
        }
        pthread_mutex_unlock(&bufferSem); 
    }
    else if(strcmp(commanderJob, "exit") == 0) {
        int length = strlen("SERVER TERMINATED");
        char* responseBuf = malloc(length + 1);
        sprintf(responseBuf, "SERVER TERMINATED");
        char responseSize[13]; //int
        sprintf(responseSize, "%ld ", strlen(responseBuf));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, responseBuf, strlen(responseBuf)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        close(*newSocketFd);
        free(responseBuf);
        serverExit = 1;
        shutdown(socketFd, SHUT_RDWR);
        pthread_mutex_unlock(&bufferSem); 
    }
    else {
        int length = strlen("UNKNOWN COMMAND");
        char* responseBuf = malloc(length + 1);
        sprintf(responseBuf, "UNKNOWN COMMAND");
        char responseSize[13]; //int
        sprintf(responseSize, "%ld ", strlen(responseBuf));
        if(write(*newSocketFd, responseSize, strlen(responseSize)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        if(write(*newSocketFd, responseBuf, strlen(responseBuf)) < 0) {
            perror("Server response write:");
            exit(1);
        }
        close(*newSocketFd);
        free(responseBuf);
        pthread_mutex_unlock(&bufferSem);
    }
    free(newSocketFd);
    return NULL;
}