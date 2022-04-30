#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/wait_children.h"

const char* treno_exec = "./bin/treno";

// PROTOTIPI FUNZIONI PADRE_TRENI
void init_segm_files();
void create_segm_file(int);

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI PADRE_TRENI

// PADRE_TRENI
int main(int argc, char *argv[]) {
    printf("PADRE_TRENI\t| Inizio esecuzione.\n");

    // crea N_SEGM file, ognuno associato ad un segmento
    init_segm_files();
    printf("PADRE_TRENI\t| Inizializzati file segmento\n");

    char tr_id_str[4];

    pid_t pid;

    // crea N_TRAINS processi, ognuno associato ad un treno
    for(int i=1; i<=N_TRAINS; i++) {
        if((pid = fork()) == 0) {
            sprintf(tr_id_str, "%d", i);
            execl(treno_exec, treno_exec, tr_id_str, argv[1], NULL);
            exit(0);
        }
        else if(pid == -1) throw_err("main");
        printf("PADRE_TRENI\t| Creato processo TRENO %d\n", i);
    }

    // processo PADRE_TRENO in attesa della terminazione dei processi TRENO
    wait_children();
    printf("PADRE_TRENI\t| Processi TRENO terminati.\n");

    exit(0);
}

// crea N_SEGM file, ognuno associato ad un segmento
void init_segm_files() {
    for(int i=1; i<=N_SEGM; i++) create_segm_file(i);
}

// modifica lo stato di un segmento
void create_segm_file(const int num_segm) {
    // definizione nome file
    char filename[16];
    sprintf(filename, "data/MA%d.txt", num_segm);

    // apertura file
    int fd;
    if((fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666)) == -1) throw_err("create_segm_file");

    // sovrascrittura file
    if(write(fd, "0", sizeof(char)) == -1) throw_err("create_segm_file");
    if(write(fd, "", sizeof(char)) == -1) throw_err("create_segm_file");

    close(fd);
}