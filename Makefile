all: jobCom jobServer

workerThread: 
	gcc -Wall -c src/workerThread.c -lrt -lpthread 

controllerThread: 
	gcc -Wall -c src/controllerThread.c -lrt -lpthread 

jobCom: src/jobCommander.c 
	gcc -Wall -o bin/jobCommander src/jobCommander.c 

jobServer: src/jobExecutorServer.c
	gcc -Wall -o bin/jobExecutorServer src/jobExecutorServer.c src/workerThread.c src/controllerThread.c -lrt -lpthread 

clean:
	rm -f bin/jobCommander bin/jobExecutorServer bin/progDelay