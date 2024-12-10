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

void* worker_thread(void* arg) {
    while(1) {
        pthread_mutex_lock(&concurrencySem);
        while(runningJobs >= concurrency || jobsInBuffer < 1) {
            if(serverExit == 1) {
                break; // break from loop after broadcast from exit.
            }
            pthread_cond_wait(&concurrencyCond, &concurrencySem);
        }
        runningJobs++;
        pthread_mutex_unlock(&concurrencySem);

        if(serverExit == 1) {
            break; // exit before trying to pick a job from buffer.
        }
        
        pthread_mutex_lock(&bufferSem);
        while(jobsInBuffer < 1) {
            pthread_cond_wait(&bufferNewJobCond, &bufferSem);
        }
    
        Job* jobToRun = jobsBuffer[0];
        // Shift remaining jobs and enter NULL.
            for (int i = 1; i < jobsInBuffer; i++) {
                jobsBuffer[i - 1] = jobsBuffer[i];
            }
            jobsBuffer[jobsInBuffer - 1] = NULL;
            jobsInBuffer--;
        pthread_cond_signal(&bufferSpotCond);
        pthread_mutex_unlock(&bufferSem); 

        pid_t childPid = fork();
        if(childPid == -1) {
            perror("Worker fork:");
            exit(1);
        }
        if(childPid == 0) { // Child process
            pid_t filePid = getpid();
            int length = snprintf(NULL, 0, "../%d.output", filePid);
            char* fileName = malloc(length + 1);
            if(fileName == NULL) {
                perror("Malloc:");
                exit(1);
            }
            sprintf(fileName, "../%d.output", filePid);
            int outputFd;
            if((outputFd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
                perror("File creation:");
                free(fileName);
                exit(1);
            }
            if(dup2(outputFd, STDOUT_FILENO) < 0) {
                perror("Dup2:");
                close(outputFd);
                free(fileName);
                exit(1);
            }
            if (dup2(outputFd, STDERR_FILENO) < 0) {
                perror("dup2 failed");
                close(outputFd);
                free(fileName);
                exit(1);
            }

            char* args = jobToRun->arguments;
            int commandLength = strlen(args) / 2;
            char** command = malloc(commandLength * sizeof(char*)); //at worst all arguments will be 1 character. it is more than enough.
            char* token = strtok(args, " ");
            int tok_num = 0;
            // Store tokens in the array
            while (token != NULL) {
                command[tok_num] = token;
                tok_num++;
                token = strtok(NULL, " ");
            }
            command[tok_num] = NULL; // NULL terminate the string for execvp.
            execvp(command[0], command);
            perror("Execvp job execution:");
            free(fileName);
            free(command);
        }
        if(childPid > 0) { // Parent process. 
            int status;
            waitpid(childPid, &status, 0);
            int nameLength = snprintf(NULL, 0, "../%d.output", childPid);
            char* fileName = malloc(nameLength + 1);
            if(fileName == NULL) {
                perror("Malloc:");
                exit(1);
            }
            sprintf(fileName, "../%d.output", childPid);
            int outputFd;
            if((outputFd = open(fileName, O_RDONLY)) < 0) {
                perror("File open:");
                free(fileName);
                exit(1);
            }
            int jobID = jobToRun->id;
            int tempLen = snprintf(NULL, 0, "-----job%d output start-----\n", jobID);
            char* header = malloc(tempLen + 1);
            sprintf(header, "-----job%d output start-----\n", jobID);
            tempLen = snprintf(NULL, 0, "\n-----job%d output end-----", jobID);
            char* finish = malloc(tempLen + 1);
            sprintf(finish, "\n-----job%d output end-----", jobID);
            int bytesRead = 1;
            char* jobOutput = malloc(bytesRead);
            char currentRead;
            while(read(outputFd, &currentRead, 1) > 0) {
                jobOutput[bytesRead - 1] = currentRead;
                bytesRead++;
                jobOutput = realloc(jobOutput, bytesRead);
            }
            size_t totalLen = strlen(header) + strlen(jobOutput) + strlen(finish);
            char bytes[13];
            sprintf(bytes, "%ld ", totalLen);
            if(write(jobToRun->socketFd, bytes, strlen(bytes)) < 0) {
                perror("Write:");
                free(header);
                free(jobOutput);
                free(finish);
                exit(1);
            }
            if(write(jobToRun->socketFd, header, strlen(header)) < 0) {
                perror("Write:");
                free(header);
                free(jobOutput);
                free(finish);
                exit(1);
            }
            if(write(jobToRun->socketFd, jobOutput, strlen(jobOutput)) < 0) {
                perror("Write:");
                free(header);
                free(jobOutput);
                free(finish);
                exit(1);
            }
            if(write(jobToRun->socketFd, finish, strlen(finish)) < 0) {
                perror("Write:");
                free(header);
                free(jobOutput);
                free(finish);
                exit(1);
            }
            free(jobToRun);
            free(header);
            free(jobOutput);
            free(finish);
            shutdown(jobToRun->socketFd, SHUT_WR);
            close(jobToRun->socketFd);
            pthread_mutex_lock(&concurrencySem);
            runningJobs--;
            pthread_cond_signal(&concurrencyCond);
            pthread_mutex_unlock(&concurrencySem);
            if(serverExit == 1) {
                break; // exit without trying to find new job.
            }
        }
    }
    return NULL;
}