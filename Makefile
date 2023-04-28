filesys: filesys.o
	gcc -o filesys filesys.o
filesys.o: filesys.c
	gcc -c filesys.c
clean:
	rm -f *.o 
