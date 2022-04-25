all: clean install ;

install: progetto-so ; mkdir bin data log ; mv *.o bin ; mv progetto-so bin

progetto-so: main.o error.o curr_time.o ; gcc -o progetto-so *.o

main.o: include/protocol.h include/error.h include/curr_time.h ; gcc -c src/main.c

error.o: include/error.h ; gcc -c src/error.c

curr_time.o: include/curr_time.h ; gcc -c src/curr_time.c

clean: ; rm -Rf bin data log