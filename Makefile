all: oss user

oss: oss.o scheduler1.o 
	gcc oss.o scheduler1.o -o oss

oss.o: oss.c scheduler.h
	gcc -c oss.c

scheduler1.o: scheduler1.c scheduler.h
	gcc -c scheduler1.c

user: user.o
	gcc user.c -o child

child.o: child.c
	gcc child.c

clean:
	rm *.o oss *.txt
