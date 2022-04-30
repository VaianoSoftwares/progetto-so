.PHONY: all install clean progetto-so padre_treni registro treno rbc

all: clean install ;

install: progetto-so padre_treni registro treno rbc ; mkdir bin data log ; mv *.o bin ; mv progetto-so bin ; mv padre_treni bin ; mv registro bin ; mv treno bin ; mv rbc bin

progetto-so: main.o error.o wait_children.o ; gcc -o progetto-so main.o error.o wait_children.o
padre_treni: padre_treni.o error.o wait_children.o ; gcc -o padre_treni padre_treni.o error.o wait_children.o
registro: registro.o error.o wait_children.o ; gcc -o registro registro.o error.o wait_children.o
treno: treno.o error.o curr_time.o segm_free.o is_stazione.o connect_pipe.o ; gcc -o treno treno.o error.o curr_time.o segm_free.o is_stazione.o connect_pipe.o
rbc: rbc.o error.o curr_time.o segm_free.o wait_children.o is_stazione.o connect_pipe.o ; gcc -o rbc rbc.o error.o curr_time.o wait_children.o segm_free.o is_stazione.o connect_pipe.o

main.o: include/protocol.h include/error.h include/curr_time.h ; gcc -c src/main.c
padre_treni.o: include/protocol.h include/error.h include/wait_children.h ; gcc -c src/padre_treni.c
registro.o: include/protocol.h include/error.h include/wait_children.h ; gcc -c src/registro.c
treno.o: include/protocol.h include/error.h include/curr_time.h include/segm_free.h include/is_stazione.h include/connect_pipe.h ; gcc -c src/treno.c
rbc.o: include/protocol.h include/error.h include/wait_children.h include/curr_time.h include/segm_free.h include/is_stazione.h include/connect_pipe.h ; gcc -c src/rbc.c
error.o: include/error.h ; gcc -c src/error.c
curr_time.o: include/curr_time.h ; gcc -c src/curr_time.c
segm_free.o: include/segm_free.h include/bool.h include/error.h ; gcc -c src/segm_free.c
wait_children.o: include/wait_children.h ; gcc -c src/wait_children.c
is_stazione.o: include/is_stazione.h include/protocol.h include/error.h ; gcc -c src/is_stazione.c
connect_pipe.o: include/connect_pipe.h include/protocol.h ; gcc -c src/connect_pipe.c

clean: ; rm -Rf bin data log