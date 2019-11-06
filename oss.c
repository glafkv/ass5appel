#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

int main(int argc, char * argv[])
{
	int option;
	int x = 0;
	int total = 18;
	int verbFlag = 0;
	char * outfile = "output.txt";
	
	while((option = getopt(argc, argv, "hovn")) != -1)
	{
		switch(option)
		{
			case 'h':
				printf("help");
				break;
			case 'o':
				outfile = argv[x+1];
				break;
			case 'n':
				total = atoi(argv[x+1]);
				if(total > 18)
					total = 18;
				
				break;
			case 'v':
				verbFlag = 1;
				break;
			case 1:
				break;
		}
	}
	scheduler(outfile, total, verbFlag);
	
	return 0;
}
