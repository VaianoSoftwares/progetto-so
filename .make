all: install ;

install: progetto-so ; mv *.o bin ; mkdir data log

progetto-so: main.o error.o clean ; gcc -o bin/progetto-so *.o

main.o: include/error.h ; gcc -c src/main.c

error.o: include/error.h ; gcc -c src/error.c

clean: ; rm -Rf bin/* data log *pipe* rbc*