#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/wait_children.h"

// PROTOTIPI FUNZIONI REGISTRO
int open_pipe(const char*, const int);
void close_pipe(const char*, const int, const int);
void send_map_to_trains(const map_t);
void send_itin(int, const itin);
void send_map_to_rbc(const map_t map);
char* map_to_str(const map_t map);

// mappe
const map_t maps[N_MAPS] = {
    {
        { "S1", "S6", "MA1-MA2-MA3-MA8" },
        { "S2", "S6", "MA5-MA6-MA7-MA3-MA8" },
        { "S7", "S3", "MA13-MA12-MA11-MA10-MA9" },
        { "S4", "S8", "MA14-MA15-MA16-MA12" },
        { "", "", "" }
    },
    {
        { "S2", "S6", "MA5-MA6-MA7-MA3-MA8" },
        { "S3", "S8", "MA9-MA10-MA11-MA12" },
        { "S4", "S8", "MA14-MA15-MA16-MA12" },
        { "S6", "S1", "MA8-MA3-MA2-MA1" },
        { "S5", "S1", "MA4-MA3-MA2-MA1" }
    }
};


/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI REGISTRO

// REGISTRO
int main(int argc, char *argv[]) {
    printf("REGISTRO\t| Inizio esecuzione.\n");

    if(argc != 3) throw_err("registro | invalid arguments");

    int etcs, n_map;
    sscanf(argv[1], "%d", &etcs);
    sscanf(argv[2], "%d", &n_map);

    // se ETCS2 allora REGISTRO invia mappa a processo RBC
    pid_t pid;
    if(etcs == 2) {
        if((pid = fork()) == 0) send_map_to_rbc(maps[n_map - 1]);
        else if(pid == -1) throw_err("registro | fork");
    }

    // REGISTRO invia itinerari a processi TRENO
    if((pid = fork()) == 0) send_map_to_trains(maps[n_map - 1]);
    else if(pid == -1) throw_err("registro | fork");

    // REGISTRO in attesa di aver inviato itinerari a processi TRENO e mappa a RBC
    wait_children();
    printf("REGISTRO\t| Terminazione esecuzione.\n");

    return EXIT_SUCCESS;
}

// REGISTRO invia itinerari a processi TRENO
void send_map_to_trains(const map_t map) {
    pid_t pid;

    // ogni processo invia l'itinerario ad un dato TRENO
    for(int i=1; i<=N_TRAINS; i++) {
        if((pid = fork()) == 0) send_itin(i, map[i - 1]);
        else if(pid == -1) throw_err("send_map_to_trains | fork");
    }

    wait_children();

    exit(EXIT_SUCCESS);
}

// attende la richiesta di un processo TRENO ed invia l'itinerario del suddetto 
void send_itin(int num_train, const itin itin) {
    char buffer[128] = { 0 };
    // itinerario viene convertito in stringa
    sprintf(buffer, "%s-%s-%s", itin.start, itin.path, itin.end);
    //num bytes da inviare a TRENO
    const int msg_len = (strlen(buffer) + 1) * sizeof(char);

    // creazione e apertura pipe registro
    const int reg_pipe = open_pipe(PIPE_FORMAT, num_train);

    // REGISTRO invia itinerario a TRENO
    if(write(reg_pipe, buffer, msg_len) == -1) throw_err("send_itin | write");
    printf("REGISTRO\t| Inviato itinerario %s di TRENO %d.\n", buffer, num_train);

    // chiusura fd e eliminazione di pipe registro
    close_pipe(PIPE_FORMAT, reg_pipe, num_train);

    exit(EXIT_SUCCESS);
}

void send_map_to_rbc(const map_t map) {
    // msg da inviare ad RBC
    char *msg_to_send = map_to_str(map);
    const int msg_len = (strlen(msg_to_send) + 1) * sizeof(char); 

    // creazione e apertura pipe registro
    const int reg_pipe = open_pipe(PIPE_FORMAT, N_RBC_PIPE);

    // REGISTRO invia mappa a RBC
    if(write(reg_pipe, msg_to_send, msg_len) == -1) throw_err("send_map_to_rbc | write");
    printf("REGISTRO\t| Inviata mappa %s ad RBC.\n", msg_to_send);

    // chiusura fd e eliminazione di pipe registro
    close_pipe(PIPE_FORMAT, reg_pipe, N_RBC_PIPE);

    free(msg_to_send);

    exit(EXIT_SUCCESS);
}

char* map_to_str(const map_t map) {
    const char *sep = "-";
    char buffer[512] = { 0 };
    for(int i=0; i<N_TRAINS; i++) {
        strcat(buffer, map[i].start);
        strcat(buffer, sep);
        strcat(buffer, map[i].path);
        strcat(buffer, sep);
        strcat(buffer, map[i].end);
        if(i != (N_TRAINS - 1)) strcat(buffer, "~");
    }

    return strdup(buffer);
}

// creazione e apertura pipe registro
int open_pipe(const char *pipe_format, const int num_pipe) {
    // nome pipe
    char filename[16];
    sprintf(filename, pipe_format, num_pipe);

    //eliminazione pipe
    unlink(filename);

    // creazione e definizione impostazioni pipe
    // mknod(filename, S_IFIFO, 0);
    // chmod(filename, 0666);
    mkfifo(filename, 0666);
    printf("REGISTRO\t| Creato %s.\n", filename);

    // apertura pipe
    int fd;
    if((fd = open(filename, O_WRONLY)) == -1) throw_err("open_pipe | open");
    printf("REGISTRO\t| Aperto %s (fd=%d).\n", filename, fd);
    return fd;
}

// chiusura fd e eliminazione pipe registro
void close_pipe(const char *pipe_format, const int fd_pipe, const int num_pipe) {
    // chiusura pipe
    close(fd_pipe);

    // nome pipe
    char filename[16];
    sprintf(filename, pipe_format, num_pipe);

    // eliminazione pipe
    unlink(filename);
    printf("REGISTRO\t| Chiuso %s (fd=%d).\n", filename, fd_pipe);
}