#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/curr_time.h"

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
BOOL are_args_correct(const cmd_args *args) {
  return args->etcs > 0 && args->etcs <= N_ETCS && args->mappa > 0 && args->mappa <= N_MAPS;
}

// crea processi REGISTRO e PADRE_TRENI
void registro_and_treni(const cmd_args args) {
    // eliminazione RBC log se esiste e processo RBC non contemplato in data istanza 
    if(args.etcs == 1) unlink(RBC_LOG);

    // creazione processo REGISTRO
    pid_t pid;
    if((pid = fork()) == 0) registro(args);
    else if(pid == -1) throw_err("main");
    printf("MAIN\t| Creato processo REGISTRO\n");

    // creazione processo PADRE_TRENI
    if((pid = fork()) == 0) padre_treni(args.etcs);
    else if(pid == -1) throw_err("main");
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

    pid_t pid;

    // crea N_TRAINS processi, ognuno associato ad un treno
    for(int i=1; i<=N_TRAINS; i++) {
        if((pid = fork()) == 0) treno(i, etcs);
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
    pid_t pid;
    if(args.etcs == 2) {
        if((pid = fork()) == 0) send_map_to_rbc(maps[args.mappa - 1]);
        else if(pid == -1) throw_err("registro");
    }

    // REGISTRO invia itinerari a processi TRENO
    if((pid = fork()) == 0) send_map_to_trains(maps[args.mappa - 1]);
    else if(pid == -1) throw_err("registro");

    // REGISTRO in attesa di aver inviato itinerari a processi TRENO e mappa a RBC
    wait_children();
    printf("REGISTRO\t| Terminazione esecuzione.\n");

    exit(0);
}

// REGISTRO invia itinerari a processi TRENO
void send_map_to_trains(const map_t map) {
    pid_t pid;

    // ogni processo invia l'itinerario ad un dato TRENO
    for(int i=1; i<=N_TRAINS; i++) {
        if((pid = fork()) == 0) send_itin(i, map[i - 1]);
        else if(pid == -1) throw_err("send_itins");
    }

    wait_children();

    exit(0);
}

// attende la richiesta di un processo TRENO ed invia l'itinerario del suddetto 
void send_itin(int num_train, const itin itin) {
    char buffer[128] = { 0 };
    // itinerario viene convertito in stringa
    sprintf(buffer, "%s-%s-%s", itin.start, itin.path, itin.end);
    //num bytes da inviare a TRENO
    const int msg_len = (strlen(buffer) + 1) * sizeof(char);

    // creazione e apertura pipe registro
    const int reg_pipe = open_pipe(PIPE_NAME, num_train);

    // REGISTRO invia itinerario a TRENO
    if(write(reg_pipe, buffer, msg_len) == -1) throw_err("send_itin_to_train");
    printf("REGISTRO\t| Inviato itinerario %s di TRENO %d.\n", buffer, num_train);

    // chiusura fd e eliminazione di pipe registro
    close_pipe(PIPE_NAME, reg_pipe, num_train);

    exit(0);
}

void send_map_to_rbc(const map_t map) {
    // msg da inviare ad RBC
    char *msg_to_send = map_to_str(map);
    const int msg_len = (strlen(msg_to_send) + 1) * sizeof(char); 

    // creazione e apertura pipe registro
    const int reg_pipe = open_pipe(PIPE_NAME, N_RBC_PIPE);

    // REGISTRO invia mappa a RBC
    if(write(reg_pipe, msg_to_send, msg_len) == -1) throw_err("send_itin_to_train");
    printf("REGISTRO\t| Inviata mappa %s ad RBC.\n", msg_to_send);

    // chiusura fd e eliminazione di pipe registro
    close_pipe(PIPE_NAME, reg_pipe, N_RBC_PIPE);

    free(msg_to_send);

    exit(0);
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
int open_pipe(const char* pipe_name, int num_pipe) {
    // nome pipe
    char filename[16];
    sprintf(filename, "%s%d", pipe_name, num_pipe);

    //eliminazione pipe
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
void close_pipe(const char* pipe_name, int fd_pipe, int num_pipe) {
    // chiusura pipe
    close(fd_pipe);

    // nome pipe
    char filename[16];
    sprintf(filename, "%s%d", pipe_name, num_pipe);

    // eliminazione pipe
    unlink(filename);
    printf("REGISTRO\t| Chiuso %s (fd=%d).\n", filename, fd_pipe);
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI TRENO

// TRENO
void treno(const int num_treno, const int etcs) {
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

// data una stringa stabilisce se questa e' nome di una stazione
BOOL is_stazione(char *str) {
    char id_str[8] = { 0 };
    int num_stazione;

    if(str[0] != 'S') return FALSE;

    strcpy(id_str, str + 1);
    sscanf(id_str, "%d", &num_stazione);

    // identificativo stazione non valido
    if(num_stazione <= 0 || num_stazione > N_STATIONS) throw_err("is_stazione");

    return TRUE;
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

// lettura file segmento, se contiene '0' allora segmento e' libero
BOOL is_segm_free(char *segm) {
    // definizione nome file segmento
    char filename[16];
    sprintf(filename, "data/%s.txt", segm);
    // printf("is_segm_free | filename: %s segm: %s\n", filename, segm);

    // apertura file segmento
    int fd;
    if((fd = open(filename, O_RDONLY, 0444)) == -1) throw_err("is_segm_free | open");

    // informazioni file
    struct stat fs;
    if((fstat(fd, &fs)) == -1) throw_err("is_segm_free | fstat");

    // mappatura file
    char *mapped_file = (char *)mmap(NULL, fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(mapped_file == MAP_FAILED) throw_err("is_segm_free | MAP_FAILED");

    // lettura file, se letto 0 allora TRENO puo' procedere
    // se letto 1 deve ritentare alla prossima iterazione
    const char file_content = *mapped_file;

    // chiusura file
    if((munmap(mapped_file, fs.st_size) == -1)) throw_err("is_segm_free | munmap");
    close(fd);

    switch(file_content) {
        case '0':
        case '1':
            break;
        default:
            // valore non valido
            throw_err("is_segm_free | invalid file content");
    }

    return file_content == '0';
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

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI RBC

// RBC
void rbc() {
    printf("RBC\t| Inizio esecuzione.\n");

    // creazione shm
    const int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    printf("RBC\t| Creata SHM %s (fd=%d).\n", SHM_NAME, shm_fd);
    ftruncate(shm_fd, SHM_SIZE);

    // inizializzazione array segmenti e stazioni
    rbc_data_t *rbc_data = (rbc_data_t*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(rbc_data == MAP_FAILED) throw_err("rbc | mmap");
    init_rbc_data(rbc_data);

    // segnale interrompe RBC se tutti processi TRENO hanno concluso esecuzione
    signal(SIGUSR1, usr1_handl);
    // viene inviato un SIGUSR1 ogni path vuoto
    check_if_empty_paths(rbc_data->paths);

    // elimina RBC log se esiste
    unlink(RBC_LOG);

    // creazione server
    const int server_fd = create_rbc_server();
    printf("RBC\t| Server creato (fd=%d).\n", server_fd);

    // server RBC attende ed esaurisce richieste processi TRENO
    run_server(server_fd);
}

// inizializzazione array segmenti e stazioni
void init_rbc_data(rbc_data_t *rbc_data) {
    int num_station;
    char *str_ptr, *station_name;

    // inizializzazione array segmento
    for(int i=0; i<N_SEGM; i++) rbc_data->segms[i] = FALSE;
    // inizializzazione array paths
    get_map(rbc_data->paths);

    // inizializzazione array stazione
    for(int i=0; i<N_STATIONS; i++) rbc_data->stations[i] = 0;
    // conta numero di treni presenti nelle stazioni di inizio
    for(int i=0; i<N_TRAINS; i++) {
        str_ptr = strdup(rbc_data->paths[i]);
        station_name = strsep(&str_ptr, "-");

        if(is_stazione(station_name)) {
            sscanf(station_name, "S%d", &num_station);
            rbc_data->stations[num_station - 1]++;
        }
    }

    free(station_name);
}

// RBC richiede mappa a REGISTRO
void get_map(char *dest[N_TRAINS]) {
    char buffer[512] = { 0 };
    // Connessione a registro pipe
    const int reg_pipe = connect_to_pipe(PIPE_NAME, N_RBC_PIPE);

    // RBC riceve mappa da REGISTRO
    if((read(reg_pipe, buffer, sizeof(buffer))) == -1) throw_err("get_map");
    printf("RBC\t| Ricevuta mappa %s da REGISTRO.\n", buffer);

    // Chiusura registro pipe
    close(reg_pipe);
    printf("RBC\t| Connessione a registro pipe (fd=%d) interrotta.\n", reg_pipe);

    int i = 0;
    char *map = strdup(buffer), *path;
    // itinerari vengono copiati in rbc_data
    while((path = strsep(&map, "~"))) dest[i++] = strdup(path);

    free(map);
    free(path);
}

// viene inviato un segnale SIGUSR1 per ogni path vuoto
void check_if_empty_paths(char *paths[N_TRAINS]) {
    for(int i=0; i<N_TRAINS; i++) {
        if(!strcmp(paths[i], "--")) kill(getpid(), SIGUSR1);
    }
}

// creazione server RBC
int create_rbc_server() {
    struct sockaddr_un server_addr;
    struct sockaddr *server_addr_ptr = (struct sockaddr *)&server_addr;
    socklen_t server_len = sizeof(server_addr);

    // creazione socket
    int server_fd;
    if((server_fd = socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1) throw_err("create_rbc_server");

    // opzioni socket
    server_addr.sun_family = AF_UNIX;     
    strcpy(server_addr.sun_path, SERVER_NAME); 

    unlink(SERVER_NAME);

    // associazione socket ad indirizzo locale server
    if((bind(server_fd, server_addr_ptr, server_len)) == -1) throw_err("create_rbc_server");

    // server abilitato a concedere richieste
    if((listen(server_fd, N_TRAINS)) == -1) throw_err("create_rbc_server");

    return server_fd;
}

void run_server(int server_fd) {
    // indirizzo client
    struct sockaddr_un client_addr;
    struct sockaddr *client_addr_ptr = (struct sockaddr*)&client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd;
    pid_t pid;

    while(1) {
        // RBC in attessa di richieste da processi TRENO
        printf("RBC\t| Server in ascolto. In attesa di richieste TRENO.\n");
        if((client_fd = accept(server_fd, client_addr_ptr, &client_len)) == -1) throw_err("run_server");

        // richiesta accolta, TRENO viene servito da un processo figlio di RBC
        printf("RBC\t| Richiesta TRENO accolta (client_fd=%d).\n", client_fd);
        if((pid = fork()) == 0) serve_req(client_fd);
        else if(pid == -1) throw_err("run_server");
        else close(client_fd);
    }
}

// RBC serve richiesta TRENO
void serve_req(int client_fd) {
    // creazione SHM
    const int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    printf("RBC\t| Creata SHM %s (fd=%d).\n", SHM_NAME, shm_fd);

    // rbc_data, SHM tra RBC e suoi processi figli
    rbc_data_t *rbc_data = (rbc_data_t*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(rbc_data == MAP_FAILED) throw_err("serve_req | mmap");

    // RBC riceve informazioni di TRENO
    char buffer[32] = { 0 };
    if((read(client_fd, buffer, sizeof(buffer))) == -1) throw_err("serve_req");
    printf("RBC\t| Ricevuto messaggio %s da TRENO.\n", buffer);
    
    char *msg_read = strdup(buffer);

    const char str_sep = '~';

    // id TRENO
    int num_train;
    char *tmp_str = strsep(&msg_read, &str_sep);
    sscanf(tmp_str, "%d", &num_train);

    // posizione corrente TRENO
    char *curr_pos = strsep(&msg_read, &str_sep);
    // posizione successiva TRENO
    char *next_pos = strsep(&msg_read, &str_sep);

    // stabilito se posizioni sono stazioni o segmenti
    const BOOL curr_stazione = is_stazione(curr_pos);
    const BOOL next_stazione = is_stazione(next_pos);

    // id posizioni
    int curr_id, next_id;
    if(curr_stazione) sscanf(curr_pos, "S%d", &curr_id);
    else sscanf(curr_pos, "MA%d", &curr_id);
    if(next_stazione) sscanf(next_pos, "S%d", &next_id);
    else sscanf(next_pos, "MA%d", &next_id);

    // RBC stabilisce se TRENO può progredire o meno
    const BOOL next_st_or_rbc_data_next_free = (next_stazione || !rbc_data->segms[next_id - 1]);
    const BOOL next_st_or_next_segm_corr = is_segm_status_correct(rbc_data, next_id, next_pos, next_stazione);
    const BOOL curr_st_or_curr_segm_corr = is_segm_status_correct(rbc_data, curr_id, curr_pos, curr_stazione);

    const BOOL auth = next_st_or_rbc_data_next_free && next_st_or_next_segm_corr && curr_st_or_curr_segm_corr;

    // printf("%d&%d&%d=%d\n", next_st_or_rbc_data_next_free, next_st_or_next_segm_corr, curr_st_or_curr_segm_corr, auth);

    // RBC invia autorizzazione a TRENO
    if((write(client_fd, &auth, sizeof(auth))) == -1) throw_err("serve_req");
    printf("RBC\t| Inviata autorizzazione %d a TRENO %d.\n", auth, num_train);

    // TRENO e' stato servito  
    close(client_fd);

    // aggiornamento di rbc_data se richiesta e' accolta
    if(auth) {
        if(next_stazione) {
            rbc_data->stations[next_id - 1]++;

            // TRENO ha raggiunto destinazione, allora TRENO prossimo alla sua terminazione
            kill(getppid(), SIGUSR1);
        }
        else rbc_data->segms[next_id - 1] = TRUE;

        if(curr_stazione) rbc_data->stations[curr_id - 1]--;
        else rbc_data->segms[curr_id - 1] = FALSE;

        // printf("RBC\t| valori aggiornati: curr_id=%d curr_val=%d next_id=%d next_val=%d\n", curr_id, rbc_data->segms[curr_id - 1], next_id, rbc_data->segms[next_id - 1]);
    }

    // RBC aggiorna il log
    update_rbc_log(num_train, curr_pos, next_pos, auth);

    // accesso a shm rimosso
    if((munmap(rbc_data, SHM_SIZE)) == -1) throw_err("serve_req | munmap");
    close(shm_fd);
    printf("RBC\t| Accesso a SHM %s (fd=%d) rimosso.\n", SHM_NAME, shm_fd);

    free(tmp_str);
    free(msg_read);

    exit(0); 
}

// se valore in file segmento non corrisponde a quello in strutt dati RBC allora FALSE, TRUE altrimenti
BOOL is_segm_status_correct(rbc_data_t *rbc_data, int segm_id, char *segm_name, BOOL stazione) {
    // se posizione non e' segmento allora TRUE
    if(stazione) return TRUE;

    const BOOL segm_file_val = !is_segm_free(segm_name);
    const BOOL rbc_segm_val = rbc_data->segms[segm_id - 1];
    // printf("RBC\t| segm_id=%d segm_file_val=%d rbc_segm_val=%d\n", segm_id, segm_file_val, rbc_segm_val);

    return segm_file_val == rbc_segm_val;
}

// aggiornamento log RBC
void update_rbc_log(const int num_train, char *curr_pos, char* next_pos, const BOOL auth) {
    // apertura file
    int fd;
    if((fd = open(RBC_LOG, O_CREAT | O_WRONLY | O_APPEND, 0666)) == -1) throw_err("update_train_log");

    // linea di testo da scrivere su file
    char write_line[128] = { 0 };
    
    // se autorizzato SI altrimenti NO
    char auth_str[3];
    if(auth) strcpy(auth_str, "SI");
    else strcpy(auth_str, "NO");

    sprintf(write_line,
            "[TRENO RICHIEDE AUTORIZZAZIONE: T%d], [Attuale: %s], [Next: %s], [AUTORIZZATO: %s], %s",
            num_train, curr_pos, next_pos, auth_str, get_curr_time());

    // num byte da scrivere su file
    const size_t line_len = strlen(write_line) * sizeof(char);

    // scrittura file
    if((write(fd, write_line, line_len)) == -1) throw_err("update_train_log");

    close(fd);
}

/*---------------------------------------------------------------------------------------------------------------------------*/
// SIGNAL HANDLERS

// invocata ogni volta un treno arriva a destinazione
// se tutti processi TRENO terminano allora RBC termina
void usr1_handl(int sign) {
    // se 0 allora tutti i processi TRENO hanno raggiunto la loro destinazione
    static int proc_train_left = N_TRAINS;

    proc_train_left--;
    printf("RBC\t| Treni rimanenti: %d.\n", proc_train_left);

    if(proc_train_left > 0) return;

    wait_children();

    // eliminazione SHM
    shm_unlink(SHM_NAME);
    printf("RBC\t| Eliminata SHM %s.\n", SHM_NAME);

    // eliminazione server
    unlink(SERVER_NAME);
    printf("RBC\t| Chiuso server %s.\n", SERVER_NAME);

    printf("RBC\t| Terminazione esecuzione.\n");
    exit(0);
}