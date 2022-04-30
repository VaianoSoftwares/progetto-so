#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/curr_time.h"
#include "../include/segm_free.h"
#include "../include/is_stazione.h"
#include "../include/connect_pipe.h"

// PROTOTIPI FUNZIONI TRENO
int connect_to_pipe(const char*, int);
char* get_itin(const int);
void update_train_log(int, char*, char*);
BOOL go_to_next_pos(const int, const int, char*, char*);
BOOL allowed_to_proceed(const int, const int, char*, char*, const BOOL);
void set_segm_status(const int, const BOOL);
BOOL allowed_by_rbc(int, char*, char*);
int connect_to_rbc(int);

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI TRENO

// TRENO
int main(int argc, char *argv[]) {
    int num_treno, etcs;
    sscanf(argv[1], "%d", &num_treno);
    sscanf(argv[2], "%d", &etcs);

    printf("TRENO %d\t| Inizio esecuzione.\n", num_treno);

    // richiesta itinerario a processo REGISTRO
    char *itin_treno = get_itin(num_treno);
    printf("TRENO %d\t| Ricevuto itinerario %s da REGISTRO.\n", num_treno, itin_treno);

    const char* no_pos = "--";

    // se TRENO non dispone di itinerario allora termina
    if(!strcmp(itin_treno, no_pos)) {
        update_train_log(num_treno, (char*)no_pos, (char*)no_pos);
        free(itin_treno);
        printf("TRENO %d\t| Nessun itinerario: terminazione esecuzione.\n", num_treno);
        exit(0);
    }

    const char path_sep = '-';

    // posizione corrente del treno inizializzata al punto di partenza dell'itinerario
    char *curr_pos = strsep(&itin_treno, &path_sep);
    char *next_pos;

    while((next_pos = strsep(&itin_treno, &path_sep))) {
        // TRENO aggiorna il proprio log file ad ogni iterazione
        update_train_log(num_treno, curr_pos, next_pos);

        // TRENO tenta di procedere al segmento successivo
        do {
            // processi TRENO restano in pausa per 3 sec ogni iterazione
            sleep(3);
            printf("TRENO %d\t| Locazione corrente: %s, richiesta di procedere "
                   "alla locazione successiva: %s.\n",
                   num_treno, curr_pos, next_pos);
        } while(!go_to_next_pos(etcs, num_treno, curr_pos, next_pos));

        curr_pos = strdup(next_pos);

        printf("TRENO %d\t| Richiesta accolta. Nuova locazione corrente: %s.\n",
               num_treno, curr_pos);
    }

    // viene annotata ultima tappa TRENO nel log
    update_train_log(num_treno, curr_pos, (char*)no_pos);

    free(curr_pos);
    free(next_pos);

    printf("TRENO %d\t| Terminazione esecuzione.\n", num_treno);
    exit(0);
}

// processo invia richiesta per ricevere itinerario a REGISTRO
char* get_itin(const int num_treno) {
    // connessione a pipe registro
    const int reg_pipe = connect_to_pipe(PIPE_NAME, num_treno);

    char buffer[128];

    // TRENO riceve itinerario da REGISTRO
    if((read(reg_pipe, buffer, sizeof(buffer))) == -1) throw_err("get_itin");

    // disconnessione da pipe registro
    close(reg_pipe);

    return strdup(buffer);
}

// aggiornamento log TRENO
void update_train_log(int num_treno, char *curr_pos, char *next_pos) {
    // prima volta viene chiamata funzione il contenuto del file viene ripristinato
    static int o_flags = O_CREAT | O_WRONLY | O_TRUNC;

    // stabilito nome file
    char filename[16];
    sprintf(filename, "log/T%d.log", num_treno);

    // apertura file
    int fd;
    if((fd = open(filename, o_flags, 0666)) == -1) throw_err("update_train_log");

    // o_flags setup dopo la prima chiamata della funzione
    o_flags = O_CREAT | O_WRONLY | O_APPEND;

    // linea di testo da scrivere su file
    char write_line[64] = { 0 };
    
    sprintf(write_line, "[Attuale: %s], [Next: %s], %s", curr_pos, next_pos, get_curr_time());

    // num byte da scrivere su file
    const size_t line_len = strlen(write_line) * sizeof(char);

    // scrittura file
    if((write(fd, write_line, line_len)) == -1) throw_err("update_train_log");

    close(fd);
}

// ritorna TRUE se TRENO procede verso la posizione successiva FALSE altrimenti
BOOL go_to_next_pos(const int etcs, int num_treno, char *curr_pos, char *next_pos) {
    // se prossima posizione e' stazione allora TRUE altrimenti FALSE
    const BOOL curr_stazione = is_stazione(curr_pos);
    const BOOL next_stazione = is_stazione(next_pos);

    // se TRENO non ottiene il permesso di procedere allora riterta all'iterazione successiva
    if(!allowed_to_proceed(etcs, num_treno, curr_pos, next_pos, next_stazione)) return FALSE;
    
    int num_segm;

    // se stazione allora TRENO non deve aggiornare stato segmento
    if(!next_stazione) {
        // ottieni numero segmento della posizione successiva
        sscanf(next_pos, "MA%d", &num_segm);
        // treno occupa il segmento successivo
        set_segm_status(num_segm, FALSE);
    }

    // se stazione allora TRENO non deve aggiornare stato segmento
    if(!curr_stazione) {
        // ottieni numero segmento posizione corrente
        sscanf(curr_pos, "MA%d", &num_segm);
        // treno libera il segmento corrente
        set_segm_status(num_segm, TRUE);   
    }

    return TRUE;
}

// modifica lo stato di un segmento
void set_segm_status(const int num_segm, const BOOL empty) {
    // definizione nome file
    char filename[16];
    sprintf(filename, "data/MA%d.txt", num_segm);

    // apertura file
    int fd;
    if((fd = open(filename, O_RDWR, 0666)) == -1) throw_err("set_segm_status | open");

    // informazioni file
    struct stat fs;
    if((fstat(fd, &fs)) == -1) throw_err("set_segm_status | fstat");

    // mappatura file in memoria
    char *mapped_file = (char *)mmap(NULL, fs.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(mapped_file == MAP_FAILED) throw_err("set_segm_status | mmap");

    // 0 se segmento libero 1 altrimenti
    const char file_content = empty ? '0' : '1';
    memcpy(mapped_file, &file_content, sizeof(char));

    // chiusura file
    if((munmap(mapped_file, fs.st_size) == -1)) throw_err("set_segm_status | munmap");
    close(fd);
}

// TRENO richiede permesso per poter procedere alla posizione successiva
BOOL allowed_to_proceed(const int etcs, int num_treno, char *curr_pos, char *next_pos, const BOOL stazione) {
    // se ETCS2 allora TRENO chiede autorizzazione a RBC
    if(etcs == 2 && !allowed_by_rbc(num_treno, curr_pos, next_pos)) return FALSE;
    // se posizione successiva e' stazione allora TRENO ha il permesso di procedere
    if(stazione) return TRUE;
    // se posizione successiva e' un segmento allora TRENO controlla lo stato di quest'ultimo
    return is_segm_free(next_pos);
}

// TRENO chiede permesso ad RBC di procedere
BOOL allowed_by_rbc(int num_train, char *curr_pos, char *next_pos) {
    // connessione ad RBC
    const int client_fd = connect_to_rbc(num_train);

    // messaggio da inviare a RBC
    const int msg_len =
        sizeof(num_train) + ((strlen(curr_pos) + 1) * sizeof(char)) +
        ((strlen(next_pos) + 1) * sizeof(char)) + (2 * sizeof(char));
    char *msg_to_send = (char *)malloc(msg_len);
    snprintf(msg_to_send, msg_len, "%d~%s~%s", num_train, curr_pos, next_pos);

    // TRENO invia numero identificativo, posizione corrente e successiva
    if((write(client_fd, msg_to_send, msg_len)) == -1) throw_err("allowed_by_rbc");
    printf("TRENO %d\t| Inviato messaggio %s identificativo ad RBC.\n", num_train, msg_to_send);

    // lettura verdetto da parte di RBC
    BOOL auth;
    if((read(client_fd, &auth, sizeof(auth))) == -1) throw_err("allowed_by_rbc");
    printf("TRENO %d\t| Ricevuta autorizzazione %d da RBC.\n", num_train, auth);

    close(client_fd);

    free(msg_to_send);

    return auth;
}

// connessione ad RBC da parte di TRENO 
int connect_to_rbc(int num_treno) {
    // indirizzo server
    struct sockaddr_un server_addr;
    struct sockaddr* server_addr_ptr = (struct sockaddr*) &server_addr;
    socklen_t server_len = sizeof(server_addr);

    // creazione socket
    int client_fd;
    if((client_fd = socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1) throw_err("connect_to_rbc");

    // opzioni socket
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SERVER_NAME);

    //  TRENO tenta connessione ad RBC
    printf("TRENO %d\t| Tentivo di connessione ad RBC (fd=%d).\n", num_treno, client_fd);
    int connected;
    do {
        connected = connect(client_fd, server_addr_ptr, server_len);
        if (connected == -1) {
            // printf("TRENO %d\t| Connessione ad RBC fallita (fd=%d).\n", num_treno, client_fd);
	        sleep(1);
        }
    } while(connected == -1);
    printf("TRENO %d\t| Connessione ad RBC stabilita (fd=%d).\n", num_treno, client_fd);

    return client_fd;
}