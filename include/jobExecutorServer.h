#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct job {
    int id;
    char* arguments;
    int socketFd;
} Job;

int socketFd;
int jobID;
int bufferSize;
int threadPoolSize;
int jobsInBuffer;
int runningJobs;
int concurrency;
int serverExit;
pthread_mutex_t bufferSem;
pthread_mutex_t concurrencySem;
pthread_cond_t bufferSpotCond;
pthread_cond_t bufferNewJobCond;
pthread_cond_t concurrencyCond;
pthread_t* worker_thread_id;
Job** jobsBuffer;
int *acceptFd;

char* getJobDouble(Job* job);

char* getJobTriple(Job* job);