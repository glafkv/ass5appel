#ifndef STRUCTURES_H
#define STRUCTURES_H

#define resSize 20
#define available 10

//structure for the shared clcok
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


#endif
