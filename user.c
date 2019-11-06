#include <stdio.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>

#define resSize 20
#define available 8

static volatile int keepRunning = 1;

void intHandler(int dummy)
{
	printf("Process %d caught sigint.\n", getpid());
	keepRunning = 0;
}

struct clock
{
	unsigned long nano;
	unsigned long sec;
	int initialResource[resSize];
};

struct msgBuf
{
	long msgType;
	unsigned long processNum;
	int request[resSize];
	int granted[resSize];
} message;

int main(int argc, char * argv[])
{
	srand(getpid()*time(0));
	int msgid, msgid1, msgid2, resourcesHeld[resSize];
	int k = 0, i = 0, m = 0;
	for(k = 0; k < resSize; k++)
	{
		resourcesHeld[k] = 0;
	}
	
	unsigned int terminates, checkSpanSec = 0;
	char* ptr;
	pid_t pid = getpid();
	struct clock * shmPTR;
	signal(SIGINT, intHandler);
	unsigned long shmID, checkSpanNano;
	unsigned long key = strtoul(argv[0], &ptr, 10);
	unsigned long msgKey = strtoul(argv[1], &ptr, 10);
	unsigned long msgKey1 = strtoul(argv[2], &ptr, 10);
	unsigned long msgKey2 = strtoul(argv[3], &ptr, 10);
	unsigned long logicalNum = strtoul(argv[4], &ptr, 10);
	shmID = shmget(key, sizeof(struct clock), 0);
	shmPTR = (struct clock *) shmat(shmID, (void *)0, 0);
	msgid = msgget(msgKey, 0777 | IPC_CREAT);
	msgid1 = msgget(msgKey1, 0777 | IPC_CREAT);
	msgid2 = msgget(msgKey2, 0777 | IPC_CREAT);
	checkSpanNano = rand() % 250000;
	terminates = rand() % 100;
	message.msgType = 1;
	while(terminates > 20 && keepRunning == 1)
	{
		for(m = 0; m < resSize; m++)
		{
			message.request[m] = 0;
		}
		message.processNum = logicalNum;
		for(i = 0; i < resSize; i++)
		{
			if(terminates > 30)
			{
				message.request[i] = (rand() % shmPTR[0].initialResource[i]);
				while(message.request[i] + resourcesHeld[i] > shmPTR[0].initialResource[i])
				{
					message.request[i] = (rand() % shmPTR[0].initialResource[i]);
				}
				if(terminates > 98)
				{
					message.request[i] = 10;
				}
			}
			else
			{
				if(resourcesHeld[i] > 0)
				{
					message.request[i] = -(rand() % shmPTR[0].initialResource[i]);
					while(message.request[i] < -(resourcesHeld[i]))
					{
						message.request[i] = -(rand() % shmPTR[0].initialResource[i]);
					}
				}
				else
				{
					message.request[i] = 0;
				}
			}
		}
		message.msgType = 1;
		for(k = 0; k < resSize; k++)
		{
			printf("Child %li requesting %d resources, holds %d.\n", logicalNum, message.request[k], resourcesHeld[k]);
		}
		msgsnd(msgid, &message, sizeof(message), 0);
		msgrcv(msgid1, &message, sizeof(message), logicalNum, 0);
		printf("Child %li receiving message type %li.\n", logicalNum, message.msgType);
		for(k = 0; k < resSize; k++)
		{
			resourcesHeld[k] = resourcesHeld[k] + message.granted[k];
			printf("Child %li granted %d resources, holds %d out of a possible %d.\n", logicalNum, message.granted[k], resourcesHeld[k], shmPTR[0].initialResource[k]);
		}
		if(shmPTR[0].nano + checkSpanNano > 1000000000)
		{
			checkSpanNano = (checkSpanNano + shmPTR[0].nano) - 1000000000;
			checkSpanSec = shmPTR[0].sec + 1;
		}
		else
		{
			checkSpanNano = (checkSpanNano + shmPTR[0].nano);
			checkSpanSec = shmPTR[0].sec;
		}
		if(shmPTR[0].sec > checkSpanSec || (shmPTR[0].sec == checkSpanSec && shmPTR[0].nano >= checkSpanNano))
			terminates = rand() % 100;
	}
	message.processNum = logicalNum;
	for(k = 0; k < resSize; k++)
	{
		message.request[k] = 0 - resourcesHeld[k];
	}
	msgsnd(msgid2, &message, sizeof(message), 0);
	shmdt(shmPTR);
	printf("Child %li dying.\n", logicalNum);
	return 0;
}




