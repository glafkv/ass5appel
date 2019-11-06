#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/msg.h>

#define resSize 20
#define available 10

static volatile int keepRunning = 1;
static int deadlockDetector(int, int[resSize][2], int[][resSize], int[], FILE *);

void intHandler(int dummy)
{
	keepRunning = 0;
}

struct clock
{
	unsigned long nano;
	unsigned int sec;
	int initialResource[resSize];
};

struct msgBuf
{
	long msgType;
	unsigned long processNum;
	int request[resSize];
	int granted[resSize];
} message;

void scheduler(char* outfile, int total, int verbFlag)
{
	unsigned int quantum = 500000;
	int alive = 0;
	int totalSpawn = 0;
	int msgid, msgid1, msgid2, shmID;
	int timeFlag = 0;
	int totalFlag = 0;
	int pid[total], status, request[total][resSize];
	int grantFlag = 0; 
	int deadlockFlag = 0;
	int processStuck = 0;
	int lastCheck = 0;
	int fileFlag = 0;
	int resource[resSize][2], resOut[total][resSize];
	int i = 0, j = 0, m = 0, k = 0;
	long g = 0;
	unsigned long increment, timeBetween, cycles = 0;
	char * parameter[32], parameter1[32], parameter2[32], parameter3[32], parameter4[32], parameter5[32];
	//pointer for the shared memory timer
	struct clock * shmPTR;
	struct clock launchTime;
	//time variables for the timeout function
	time_t when, when2;
	//file pointers for output
	FILE * output;
	//key variable for shared memory access
	unsigned long key, msgKey, msgKey1, msgKey2;
	srand(time(0));
	timeBetween = (rand() % 100000000) + 1000000;
	key = rand();
	msgKey = ftok("user.c", 65);
	msgKey1 = ftok("user.c", 67);
	msgKey2 = ftok("user.c", 69);
	msgid = msgget(msgKey, 0777 | IPC_CREAT);
	msgid1 = msgget(msgKey1, 0777 | IPC_CREAT);
	msgid2 = msgget(msgKey2, 0777 | IPC_CREAT);
	message.msgType = 1;
	//setting initial time for later check
	time(&when);
	output = fopen(outfile, "w");
	//check for file error
	if(output == NULL)
	{
		perror("Error: ");
		printf("output file could not be created.\n");
		exit(EXIT_SUCCESS);
	}
	printf("\n");
	
	for(k = 0; k < total; k++)
	{
		for(j = 0; j < resSize; j++)
		{
			resOut[k][j] = 0;
			request[k][j] = 0;
		}
	}
	//get and access shared memory, setting initial timer to 0
	shmID = shmget(key, sizeof(struct clock), IPC_CREAT | IPC_EXCL | 0777);
	shmPTR = (struct clock *) shmat(shmID, (void *)0, 0);
	shmPTR[0].sec = 0;
	shmPTR[0].nano = 0;
	//initializing resource arrays
	for(k = 0; k < resSize; k++)
	{
		resource[k][0] = (rand() % available) + 1;
		shmPTR[0].initialResource[k] = resource[k][0];
		if(rand() % 100 < 20)
		{
			resource[k][1] = 1;
		}
		else
		{
			resource[k][1] = 0;
		}
		message.request[k] = 0;
		message.granted[k] = 0;
	}
	launchTime.sec = 0;
	launchTime.nano = timeBetween;
	//initializing pids to -1
	for(k = 0; k < total; k++)
	{
		pid[k] = -1;
	}
	//call to signal handler for ctrlc
	signal(SIGINT, intHandler);
	increment = (rand() % 5000000) + 25000000;
	//while loop keeps running until all children are spawned, strlc, or time is reached
	while((i < total) && (keepRunning == 1) && (timeFlag == 0) && fileFlag == 0)
	{
		time(&when2);
		if(when2 - when > 10)
		{
			timeFlag = 1;
		}
		if(ftell(output) > 10000000)
		{
			fileFlag = 1;	
		}
		//incrementing the timer
		shmPTR[0].nano += increment;
		if(shmPTR[0].nano >= 1000000000)
		{
			shmPTR[0].sec += 1;
			shmPTR[0].nano -= 1000000000;
		}
		if(shmPTR[0].sec > lastCheck)
		{
			lastCheck = shmPTR[0].sec;
			printf("Checking for deadlock, %d alive.\n", alive);
			deadlockFlag = deadlockDetector(total, resource, request, pid, output);
		}
		//if statement to spawn child if timer has passed its birth time
		if((shmPTR[0].sec > launchTime.sec) || ((shmPTR[0].sec == launchTime.sec) && (shmPTR[0].nano > launchTime.nano)))
		{
			if((pid[i] = fork()) == 0)
			{
				//converting key, shmid, and life to char* for passing to exec
				sprintf(parameter1, "%li", key);
				sprintf(parameter2, "%li", msgKey);
				sprintf(parameter3, "%li", msgKey1);
				sprintf(parameter4, "%li", msgKey2);
				sprintf(parameter5, "%d", i+1);
				srand(getpid() * (time(0) / 3));
				char * args[] = {parameter1, parameter2, parameter3, parameter4, parameter5, NULL};
				execvp("./user\0", args);
			}
			//updating launch time for next child
			launchTime.sec = shmPTR[0].sec;
			launchTime.nano = shmPTR[0].nano;
			launchTime.nano += timeBetween;
			if(launchTime.nano >= 1000000000)
			{
				launchTime.sec += 1;
				launchTime.nano -= 1000000000;
			}
			alive++;
			totalSpawn++;
			i++;
			cycles++;
		}
		//checking for message from exiting child
		if(msgrcv(msgid2, &message, sizeof(message), 0, IPC_NOWAIT) != -1)
		{
			
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			if(verbFlag)
			{
				fprintf(output, "Parent received dead child message from %li, %d.\n", message.processNum, pid[message.processNum-1]);
			}
			
			for(k = 0; k < resSize; k++)
			{
				//adding childs resources back to pool if not shareable
				if(resource[k][1] == 0)
				{
					resource[k][0] = resource[k][0] - message.request[k];
				}
				request[message.processNum-1][k] = 0;
			}
			alive--;
			pid[message.processNum-1] = -1;
			 
			for(g = 0; g < totalSpawn; g++)
			{
				
				for(j = 0; j < resSize; j++)
				{
					if((request[g][j] > 0) && (request[g][j] < resource[j][0]))
					{
						grantFlag = 1;
						message.granted[j] = request[g][j];
						if(resource[g][1] == 0)
						{
							resource[j][0] = resource[j][0] - request[g][j];
						}
						resOut[g][j] = message.granted[j];
					}
				}
				if(grantFlag)
				{
					grantFlag = 0;
					message.msgType = g + 1;
					if(verbFlag)
					{
						fprintf(output, "Parent granting child %li resources from dead child.\n", message.msgType);
					}
					msgsnd(msgid1, &message, sizeof(message), 0);
				}
			}
		}
		if(msgrcv(msgid, &message, sizeof(message), 0, IPC_NOWAIT) != -1)
		{
			
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			
			for(k = 0; k < resSize; k++)
			{
				if(resource[k][0] - message.request[k] >= 0)
				{
					grantFlag = 1;
					if(resource[k][1] == 0)
					{
						resource[k][0] = resource[k][0] - message.request[k];
					}	
					message.granted[k] = message.request[k];
					message.msgType = message.processNum;
					request[message.processNum-1][k] = message.granted[k];
				}
				else
				{
					request[message.processNum-1][k] = message.request[k];
				}	
			}
			if(grantFlag == 1)
			{
				if(verbFlag)
				{
					fprintf(output, "Parent granting child %li resources.\n", message.msgType);
				}
				msgsnd(msgid1, &message, sizeof(message), 0);
				grantFlag = 0;
			}
		}
	}
	while(alive > 0 && keepRunning == 1 && deadlockFlag == 0 && timeFlag == 0 && fileFlag == 0)
	{
		time(&when2);
		if(when2 - when > 10)
		{
			timeFlag = 1;
		}
		if(ftell(output) > 10000000)
		{
			fileFlag = 1;
		}
		shmPTR[0].nano += increment;
		if(shmPTR[0].nano >= 1000000000)
		{
			shmPTR[0].sec += 1;
			shmPTR[0].nano -= 1000000000;
		}
		if(shmPTR[0].sec > lastCheck)
		{
			lastCheck = shmPTR[0].sec;
			printf("Checking for deadlock, %d alive.\n", alive);
			deadlockFlag = deadlockDetector(total, resource, request, pid, output);
		}
		if(msgrcv(msgid2, &message, sizeof(message), 0, IPC_NOWAIT) != -1)
		{
			
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			if(verbFlag)
			{
				fprintf(output, "Parent received dead child message from %li, %d.\n", message.processNum, pid[message.processNum-1]);
			}
			
			for(k = 0; k < resSize; k++)
			{
				if(resource[k][1] == 0)
				{
					resource[k][0] = resource[k][0] - message.request[k];
				}
				request[message.processNum-1][k] = 0;
			}
			alive--;
			pid[message.processNum-1] = -1;
			
			for(g = 0; g < totalSpawn; g++)
			{
				
				for(j = 0; j < resSize; j++)
				{
					if((request[g][j] > 0) && (request[g][j] < resource[j][0]))
					{
						grantFlag = 1;
						message.msgType = g + 1;
						message.granted[j] = request[g][j];
						if(resource[g][1] == 0)
						{
							resource[j][0] = resource[j][0] - request[g][j];
						}
						resOut[g][j] = message.granted[j];
					}
				}
				if(grantFlag)
				{
					grantFlag = 0;
					if(verbFlag)
					{
						fprintf(output, "Parent granting child %li resources from dead child.\n", message.msgType);
					}
					msgsnd(msgid1, &message, sizeof(message), 0);
				}
			}
		}
		if(msgrcv(msgid, &message, sizeof(message), 0, IPC_NOWAIT) != -1)
		{
			
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			
			for(k = 0; k < resSize; k++)
			{
				if(resource[k][0] - message.request[k] >= 0)
				{
					grantFlag = 1;
					if(resource[k][1] == 0)
					{
						resource[k][0] = resource[k][0] - message.request[k];
					}
					message.granted[k] = message.request[k];
					message.msgType = message.processNum;
					resOut[message.processNum][k] = message.granted[k];
				}
				else
				{
					request[message.processNum-1][k] = message.request[k];
				}
			}
			if(grantFlag == 1)
			{
				if(verbFlag)
				{
					fprintf(output, "Parent granting child %li resources.\n", message.msgType);
				}
				msgsnd(msgid1, &message, sizeof(message), 0);
				grantFlag = 0;
			}
		}
	}
	if(timeFlag == 1)
	{
		printf("Program has reached its allotted time, exiting.\n");
	}
	if(totalFlag == 1)
	{
		printf("Program has reached its allotted children, exiting.\n");
	}
	if(keepRunning == 0)
	{
		printf("Terminated due to ctrl-c signal.\n");
	}
	if(fileFlag == 1)
	{
		printf("Terminated due to log file length.\n");
	}
	
	for(i = 0; i < total; i++)
	{
		if(pid[i] > -1)
		{
			fprintf(output, "Killing process %d, %d\n", i+1, pid[i]);
			if(kill(pid[i], SIGINT) == 0)
			{
				usleep(100000);
				kill(pid[i], SIGKILL);
			}
		}
	}
	shmdt(shmPTR);
	shmctl(shmID, IPC_RMID, NULL);
	msgctl(msgid, IPC_RMID, NULL);
	msgctl(msgid1, IPC_RMID, NULL);
	msgctl(msgid2, IPC_RMID, NULL);
	wait(NULL);
	fclose(output);
}

static int deadlockDetector(int total, int resources[resSize][2], int requests[total][resSize], int pid[total], FILE * output)
{	
	int procReq[resSize], deadlocked[total];
	int sysLock = 0;
	int status = 1;
	int processStuck = 0;
	int i, j;
	for(i = 0; i < total; i++)
	{
		if(pid[i] > 0)
		{
			fprintf(output, "Process %d, %d unfulfilled requests, available resources: ", i+1, pid[i]);
			for(j = 0; j < resSize; j++)
			{
				procReq[j] = requests[i][j];
				fprintf(output, "%d, %d ||", procReq[j], resources[j][0]);
			}
			fprintf(output, "\n");
		}
		for(j = 0; j < resSize; j++)
		{
			if((procReq[j] <= resources[j][0]) && (pid[i] > 0))
			{
				deadlocked[i] = 0;
			}
			else
			{
				deadlocked[i] = 1;
				break;
			}	
		}
	}
	for(i = 0; i < total; i++)
	{
		if((deadlocked[i] == 1) && (pid[i] > 0))
		{
			sysLock = 1;
		}
		else if(deadlocked[i] == 0)
		{
			sysLock = 0;
			fprintf(output, "No deadlock, process %d, %d can complete.\n", i+1, pid[i]);
			break;
		}
	}
	if(sysLock)
	{
		fprintf(output, "System deadlocked\n");
		printf("System deadlocked\n");
		for(i = 0; i < total; i++)
		{
			if(pid[i] > -1)
			{
				fprintf(output, "Killing process %d, %d\n", i+1, pid[i]);
				printf("Killing process %d, %d\n", i+1, pid[i]);
				if(kill(pid[i], SIGINT) == 0)
				{
					usleep(100000);
					kill(pid[i], SIGKILL);
				}
			}
		}
	}
	return sysLock;

}	
