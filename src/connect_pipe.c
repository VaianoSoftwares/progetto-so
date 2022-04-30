#include "../include/connect_pipe.h"

// connessione a pipe registro
int connect_to_pipe(const char* pipe_name, int num_treno) {
    char filename[16];
    sprintf(filename, "%s%d", pipe_name, num_treno);

    if(num_treno == N_RBC_PIPE) printf("RBC\t| Tentivo di connessione a %s.\n", filename);
    else printf("TRENO %d\t| Tentivo di connessione a %s.\n", num_treno, filename);

    int fd;
    do {
        fd = open(filename, O_RDONLY);

        if(fd == -1) {
            // if(num_treno == N_RBC_PIPE) printf("RBC\t| Connessione a %s fallita.\n", filename);
            // else printf("TRENO %d\t| Connessione a %s fallita.\n", num_treno, filename);
            sleep(1);
        }
    } while(fd == -1);

    if(num_treno == N_RBC_PIPE) printf("RBC\t| Connessione a %s (fd=%d) stabilita.\n", filename, fd);
    else printf("TRENO %d\t| Connessione a %s (fd=%d) stabilita.\n", num_treno, filename, fd);
    return fd;
}