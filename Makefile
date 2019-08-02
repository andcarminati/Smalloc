all: tests

tests: allocation.o tests.o
	gcc -g tests.o allocation.o -o tests

tests.o: tests.c
	gcc -g -c tests.c

allocation.o: allocation.c allocation.h
	gcc -g -c allocation.c


clean:
	/bin/rm -f tests allocation.o tests.o