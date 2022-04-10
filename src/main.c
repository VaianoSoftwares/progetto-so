#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../include/error.h"

#define N_TRAINS 5
#define N_STATIONS 8
#define N_SEGM 16
#define N_MAPS 2
#define N_ETCS 2

typedef enum bool { FALSE, TRUE } bool;

typedef struct cmd_args {
    int etcs;
    bool rbc;
    int mappa;
} cmd_args;

typedef struct itin {
    char *start;
    char *end;
    char *path;
} itin;

typedef itin map_t[N_TRAINS];

// PROTOTIPI FUNZIONI MAIN
void parse_cmd_args(const int, char* [], cmd_args*);
bool are_args_correct(const cmd_args*);
void registro_and_treni(const cmd_args);
void wait_children();
// PROTOTIPI FUNZIONI PADRE_TRENI
void padre_treni(const int);
void init_segm_files();
// PROTOTIPI FUNZIONI REGISTRO
void registro(const cmd_args);
void set_segm_status(const int, const bool, const bool);
int open_reg_pipe(const int);
void close_reg_pipe(int, int);
void send_itin_to_trains(const map_t);
void send_itin_to_train(int, const itin);
void itin_to_str(const itin, char*);
void send_map_to_rbc(const map_t);
// PROTOTIPI FUNZIONI TRENO
void treno(const int, const int);
int connect_to_reg_pipe(int);
char* get_itin(const int);
void update_train_log(int, char*, char*);
bool go_to_next_pos(const int, char*, char*);
bool allowed_to_proceed(const int, char*, const bool);
bool is_stazione(char*);
bool is_segm_free(char*);
bool allowed_by_rbc(const bool);
// PROTOTIPI FUNZIONI RBC
void rbc();

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI MAIN

// main
int main(int argc, char *argv[]) {
    // argomenti passati da cmd vengono interpretati
    cmd_args args;
    parse_cmd_args(argc, argv, &args);
    printf("MAIN\t| ETCS%d MAPPA%d RBC=%d\n", args.etcs, args.mappa, args.rbc);

    // se ETCS2 e RBC allora esecuzione processo RBC
    if(args.etcs == 2 && args.rbc) rbc();
    // esecuzione processi PADRE_TRENI e REGISTRO altrimenti
    else registro_and_treni(args);

    return 0;
}

// argomenti passati da cmd vengono interpretati
void parse_cmd_args(const int argc, char* argv[], cmd_args* result) {    
    result->etcs = 0;
    result->mappa = 0;
    result->rbc = FALSE;

    for(int i=1; i<argc; i++) {
        // parse argomento ETCS
        if(strlen(argv[i]) > 4 && !strncmp("ETCS", argv[i], 4)) sscanf(argv[i], "ETCS%d", &result->etcs);
        // parse argomento MAPPA
        else if(strlen(argv[i]) > 5 && !strncmp("MAPPA", argv[i], 5)) sscanf(argv[i], "MAPPA%d", &result->mappa);
        // parse argomento RBC
        else if(!strcmp("RBC", argv[i])) result->rbc = TRUE;
        // argomento non valido
        else throw_err("parse_cmd_args | invalid argument");
    }

    // controllo valore argomenti
    if(!are_args_correct(result)) throw_err("parse_cmd_args | invalid argument");
}

// controllo correttezza valori argomenti cmd
bool are_args_correct(const cmd_args *args) {
  return args->etcs > 0 && args->etcs <= N_ETCS && args->mappa > 0 && args->mappa <= N_MAPS;
}

// crea processi REGISTRO e PADRE_TRENI
void registro_and_treni(const cmd_args args) {
    // creazione processo REGISTRO
    pid_t curr_pid = fork();
    if(curr_pid == 0) registro(args);
    else if(curr_pid == -1) throw_err("main");
    printf("MAIN\t| Creato processo REGISTRO\n");

    // creazione processo PADRE_TRENI
    curr_pid = fork();
    if(curr_pid == 0) padre_treni(args.etcs);
    else if(curr_pid == -1) throw_err("main");
    printf("MAIN\t| Creato processo PADRE_TRENI\n");

    // processo principale in attesa di terminazione di REGISTRO e PADRE_TRENI
    wait_children();
    printf("MAIN\t| REGISTRO e PADRE_TRENI: esecuzione terminata\n");
}

// processo padre attende la terminazione dei figli
void wait_children() {
    while(waitpid(0, NULL, 0) > 0) sleep(1);
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI PADRE_TRENI

// PADRE_TRENI
void padre_treni(const int etcs) {
    printf("PADRE_TRENI\t| Inizio esecuzione.\n");

    // crea N_SEGM file, ognuno associato ad un segmento
    init_segm_files();
    printf("PADRE_TRENI\t| Inizializzati file segmento\n");

    pid_t curr_pid;

    // crea N_TRAINS processi, ognuno associato ad un treno
    for(int i=1; i<=N_TRAINS; i++) {
        curr_pid = fork();
        if(curr_pid == 0) treno(i, etcs);
        else if(curr_pid == -1) throw_err("main");
        printf("PADRE_TRENI\t| Creato processo TRENO %d\n", i);
    }

    // processo PADRE_TRENO in attesa della terminazione dei processi TRENO
    wait_children();
    printf("PADRE_TRENI\t| Processi TRENO terminati.\n");

    exit(0);
}

// crea N_SEGM file, ognuno associato ad un segmento
void init_segm_files() {
    for(int i=1; i<=N_SEGM; i++) set_segm_status(i, TRUE, TRUE);
}

// modifica lo stato di un segmento
void set_segm_status(const int num_segm, const bool empty, const bool new_file) {
    // definizione nome file
    char filename[16];
    snprintf(filename, sizeof(filename), "data/MA%d.txt", num_segm);

    // se new_file e' TRUE allora se non esiste viene creato file
    int o_flags = new_file ? O_CREAT | O_RDWR | O_TRUNC : O_WRONLY | O_TRUNC;

    // apertura file
    int fd;
    if((fd = open(filename, o_flags, 0666)) == -1) throw_err("set_segm_status");

    // 0 se segmento libero 1 altrimenti
    char file_content = empty ? '0' : '1';

    // sovrascrittura file
    if(write(fd, &file_content, sizeof(char)) == -1) throw_err("set_segm_status");

    close(fd);
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI REGISTRO

// REGISTRO
void registro(const cmd_args args) {
    printf("REGISTRO\t| Inizio esecuzione.\n");

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

    // se ETCS2 allora REGISTRO invia mappa a processo RBC
    pid_t curr_pid;
    if(args.etcs == 2) {
        curr_pid = fork();
        if(curr_pid == 0) send_map_to_rbc(maps[args.mappa - 1]);
        else if(curr_pid == -1) throw_err("registro");
    }

    // REGISTRO invia itinerari a processi TRENO
    curr_pid = fork();
    if(curr_pid == 0) send_itin_to_trains(maps[args.mappa - 1]);
    else if(curr_pid == -1) throw_err("registro");

    // REGISTRO in attesa di aver inviato itinerari a processi TRENO e mappa a RBC
    wait_children();
    printf("REGISTRO\t| Terminazione esecuzione.\n");

    exit(0);
}

// REGISTRO invia itinerari a processi TRENO
void send_itin_to_trains(const map_t map) {
    pid_t curr_pid;

    // ogni processo invia l'itinerario ad un dato TRENO
    for(int i=1; i<=N_TRAINS; i++) {
        curr_pid = fork();
        if(curr_pid == 0) send_itin_to_train(i, map[i - 1]);
        else if(curr_pid == -1) throw_err("send_itin_to_trains");
    }

    wait_children();

    exit(0);
}

// attende la richiesta di un processo TRENO ed invia l'itinerario del suddetto 
void send_itin_to_train(int num_train, const itin itin) {
    // creazione e apertura pipe registro
    int reg_pipe = open_reg_pipe(num_train);

    char buffer[128];
    // itinerario viene convertito in stringa
    itin_to_str(itin, buffer);
    //num bytes da inviare a TRENO
    int msg_len = (strlen(buffer) + 1) * sizeof(char);

    // REGISTRO invia itinerario a TRENO
    if(write(reg_pipe, buffer, msg_len) == -1) throw_err("send_itin_to_train");
    printf("REGISTRO\t| Inviato itinerario %s a TRENO %d.\n", buffer, num_train);

    // chiusura fd e eliminazione di pipe registro
    close_reg_pipe(reg_pipe, num_train);

    exit(0);
}

// conversione da itin a str
void itin_to_str(const itin src, char *dest) {
    strcpy(dest, src.start);
    strcat(dest, "-");
    strcat(dest, src.path);
    strcat(dest, "-");
    strcat(dest, src.end);
}

// creazione e apertura pipe registro
int open_reg_pipe(int num_treno) {
    // nome pipe
    char filename[16];
    snprintf(filename, sizeof(filename), "reg_pipe%d", num_treno);

    unlink(filename);
    // creazione e definizione impostazioni pipe
    mknod(filename, S_IFIFO, 0);
    chmod(filename, 0666);
    printf("REGISTRO\t| Creato %s.\n", filename);

    // apertura pipe
    int fd;
    if((fd = open(filename, O_WRONLY)) == -1) throw_err("open_reg_pipe");
    printf("REGISTRO\t| Aperto %s (fd=%d).\n", filename, fd);
    return fd;
}

// chiusura fd e eliminazione pipe registro
void close_reg_pipe(int reg_pipe, int num_treno) {
    close(reg_pipe);
    char filename[16];
    snprintf(filename, sizeof(filename),"reg_pipe%d", num_treno);
    unlink(filename);
    printf("REGISTRO\t| Chiuso %s (fd=%d).\n", filename, reg_pipe);
}

// REGISTRO invia mappa a processo RBC
void send_map_to_rbc(const map_t mappa) {
    exit(0);
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI TRENO

// TRENO
void treno(const int num_treno, const int etcs) {
    printf("TRENO %d\t| Inizio esecuzione.\n", num_treno);

    // richiesta itinerario a processo REGISTRO
    char *itin_treno = get_itin(num_treno);

    // posizione corrente del treno inizializzata al punto di partenza dell'itinerario
    char *curr_pos = strsep(&itin_treno, "-");
    char *next_pos;

    while((next_pos = strsep(&itin_treno, "-"))) {
        // TRENO tenta di procedere al segmento successivo
        do {
            // TRENO aggiorna il proprio log file ad ogni iterazione
            update_train_log(num_treno, curr_pos, next_pos);
            // processi TRENO restano in pausa per 3 sec ogni iterazione
            sleep(3);
            printf("TRENO %d\t| Locazione corrente: %s, richiesta di procedere "
                   "alla locazione successiva: %s.\n",
                   num_treno, curr_pos, next_pos);
        } while(!go_to_next_pos(etcs, curr_pos, next_pos));

        curr_pos = strdup(next_pos);

        printf("TRENO %d\t| Richiesta accolta. Nuova locazione corrente: %s.\n",
               num_treno, curr_pos);
    }

    // viene annotata ultima tappa TRENO nel log
    update_train_log(num_treno, curr_pos, "--");

    free(curr_pos);
    free(next_pos);

    printf("TRENO %d\t| Terminazione esecuzione.\n", num_treno);
    exit(0);
}

// TRENO invia richiesta per ricevere itinerario a REGISTRO
char* get_itin(const int num_treno) {
    // connessione a pipe registro
    int reg_pipe = connect_to_reg_pipe(num_treno);

    char buffer[128];
    int bytes_read;

    // TRENO riceve itinerario da REGISTRO
    if((bytes_read = read(reg_pipe, buffer, sizeof(buffer))) == -1) throw_err("get_itin");
    printf("TRENO %d\t| Ricevuto itinerario %s da REGISTRO.\n", num_treno, buffer);

    // disconnessione da pipe registro
    close(reg_pipe);
    printf("TRENO %d\t| Connessione interrotta (fd=%d).\n", num_treno, reg_pipe);

    return strdup(buffer);
}

// TRENO richiede il proprio itinerario a REGISTRO
int connect_to_reg_pipe(int num_treno) {
    char filename[16];
    snprintf(filename, sizeof(filename), "reg_pipe%d", num_treno);

    int fd;
    do {
        fd = open(filename, O_RDONLY);
        if(fd == -1) sleep(1);
    } while(fd == -1);
    printf("TRENO %d\t| Connessione a %s (fd=%d) stabilita.\n", num_treno, filename, fd);
    return fd;
}

void update_train_log(int num_treno, char *curr_pos, char *next_pos) {
    // stabilito nome file
    char filename[16];
    snprintf(filename, sizeof(filename), "log/T%d.log", num_treno);

    // apertura file
    int fd;
    if((fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0666)) == -1) throw_err("update_train_log");

    // linea di testo da scrivere su file
    char write_line[64] = { 0 };
    time_t now = time(NULL);
    struct tm *time_ptr = localtime(&now);
    snprintf(write_line, sizeof(write_line), "[Attuale: %s], [Next: %s], %s", curr_pos, next_pos, asctime(time_ptr));

    // num byte da scrivere su file
    size_t line_len = strlen(write_line) * sizeof(char);

    // scrittura file
    if((write(fd, write_line, line_len)) == -1) throw_err("update_train_log");

    close(fd);
}

// ritorna TRUE se TRENO procede verso la posizione successiva FALSE altrimenti
bool go_to_next_pos(const int etcs, char *curr_pos, char *next_pos) {
    // se prossima posizione e' stazione allora TRUE altrimenti FALSE
    const bool stazione = is_stazione(next_pos);

    // se TRENO non ottiene il permesso di procedere allora riterta all'iterazione successiva
    if(!allowed_to_proceed(etcs, next_pos, stazione)) return FALSE;
    
    int num_segm;

    // se stazione allora TRENO non deve aggiornare stato segmento successivo
    if(!stazione) {
        // ottieni numero segmento della posizione successiva
        sscanf(next_pos, "MA%d", &num_segm);
        // treno occupa il segmento successivo
        set_segm_status(num_segm, FALSE, FALSE);
    }

    // ottieni numero segmento posizione corrente
    sscanf(curr_pos, "MA%d", &num_segm);
    // treno libera il segmento corrente
    set_segm_status(num_segm, TRUE, FALSE);

    return TRUE;
}

// data una stringa stabilisce se questa e' nome di una stazione
bool is_stazione(char *str) {
    char first_ch;
    int num_stazione;
    sscanf(str, "%c%d", &first_ch, &num_stazione);
    return first_ch == 'S' && num_stazione > 0 && num_stazione <= N_STATIONS;
}

// TRENO richiede permesso per poter procedere alla posizione successiva
bool allowed_to_proceed(const int etcs, char *next_pos, const bool stazione) {
    // se ETCS2 allora TRENO chiede autorizzazione a RBC
    if(etcs == 2 && !allowed_by_rbc(stazione)) return FALSE;
    // se posizione successiva e' stazione allora TRENO ha il permesso di procedere
    if(stazione) return TRUE;
    // se posizione successiva e' un segmento allora TRENO controlla lo stato di quest'ultimo
    return is_segm_free(next_pos);
}

bool is_segm_free(char *segm) {
    // definizione nome file segmento
    char filename[16];
    snprintf(filename, sizeof(filename), "data/%s.txt", segm);

    // apertura file segmento
    int fd;
    if((fd = open(filename, O_RDONLY, 0444)) == -1) throw_err("allowed_to_proceed");

    // lettura file, se letto 0 allora TRENO puo' procedere
    // se letto 1 deve ritentare alla prossima iterazione
    char data_read;
    if((read(fd, &data_read, sizeof(data_read))) == -1) throw_err("allowed_to_proceed");

    close(fd);

    return data_read == '0';
}

bool allowed_by_rbc(const bool stazione) {
    return TRUE;
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI RBC

void rbc() {
    printf("RBC\t| Inizio esecuzione.\n");
}