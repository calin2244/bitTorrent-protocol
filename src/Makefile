build:
	mpicc -o bitprot -Wall main.c -pthread
clean:
	rm -rf bitprot
run:
	mpirun --oversubscribe -np 4 bitprot
chleak:
	mpirun --oversubscribe -np 6 valgrind ./bitprot
rm:
	rm -rf in* && rm -rf client*
