#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/wait_children.h"
#include "../include/curr_time.h"
#include "../include/segm_free.h"
#include "../include/is_stazione.h"
#include "../include/connect_pipe.h"

// PROTOTIPI FUNZIONI RBC
void get_map(char**);
void init_rbc_data(rbc_data_t*);
void check_if_empty_paths(char**);
int create_rbc_server();
void run_server(int);
void serve_req(int);
bool is_segm_status_correct(rbc_data_t*, int, char*, bool);
void update_rbc_log(const int, char*, char*, const bool);
//void print_rbc_data(rbc_data_t);
// SIGNALS HANDLERS
void usr1_handl(int);

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI RBC

// RBC
int main(void) {
    printf("RBC\t| Inizio esecuzione.\n");

    // creazione shared memory
    const int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1) throw_err("rbc | shm_open");
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

    return EXIT_SUCCESS;
}

// inizializzazione array segmenti e stazioni
void init_rbc_data(rbc_data_t *rbc_data) {
    int num_station;
    char *str_ptr, *station_name;

    // inizializzazione array segmento
    for(int i=0; i<N_SEGM; i++) rbc_data->segms[i] = false;
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
    const int reg_pipe = connect_to_pipe(PIPE_FORMAT, N_RBC_PIPE);

    // RBC riceve mappa da REGISTRO
    if((read(reg_pipe, buffer, sizeof(buffer))) == -1) throw_err("get_map | read");
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
    if((server_fd = socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1) throw_err("create_rbc_server | socket");

    // opzioni socket
    server_addr.sun_family = AF_UNIX;     
    strcpy(server_addr.sun_path, SERVER_NAME); 

    unlink(SERVER_NAME);

    // associazione socket ad indirizzo locale server
    if((bind(server_fd, server_addr_ptr, server_len)) == -1) throw_err("create_rbc_server | bind");

    // server abilitato a concedere richieste
    if((listen(server_fd, N_TRAINS)) == -1) throw_err("create_rbc_server | listen");

    return server_fd;
}

void run_server(int server_fd) {
    // indirizzo client
    struct sockaddr_un client_addr;
    struct sockaddr *client_addr_ptr = (struct sockaddr*)&client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd;
    pid_t pid;

    while(true) {
        // RBC in attessa di richieste da processi TRENO
        printf("RBC\t| Server in ascolto. In attesa di richieste TRENO.\n");
        if((client_fd = accept(server_fd, client_addr_ptr, &client_len)) == -1)
            throw_err("run_server | accept");

        // richiesta accolta, TRENO viene servito da un processo figlio di RBC
        printf("RBC\t| Richiesta TRENO accolta (client_fd=%d).\n", client_fd);
        if((pid = fork()) == 0) {
            close(server_fd);
            serve_req(client_fd);
        }
        else if(pid == -1) throw_err("run_server | fork");
        close(client_fd);
    }
}

// RBC serve richiesta TRENO
void serve_req(int client_fd) {
    // creazione SHM
    const int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if(shm_fd == -1) throw_err("serve_req | shm_open");
    printf("RBC\t| Creata SHM %s (fd=%d).\n", SHM_NAME, shm_fd);

    // rbc_data, SHM tra RBC e suoi processi figli
    rbc_data_t *rbc_data = (rbc_data_t*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(rbc_data == MAP_FAILED) throw_err("serve_req | mmap");

    // RBC riceve informazioni di TRENO
    char buffer[32] = { 0 };
    if(recv(client_fd, buffer, sizeof(buffer), 0) == -1) throw_err("serve_req | recv");
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
    const bool curr_stazione = is_stazione(curr_pos);
    const bool next_stazione = is_stazione(next_pos);

    // id posizioni
    int curr_id, next_id;
    if(curr_stazione) sscanf(curr_pos, "S%d", &curr_id);
    else sscanf(curr_pos, "MA%d", &curr_id);
    if(next_stazione) sscanf(next_pos, "S%d", &next_id);
    else sscanf(next_pos, "MA%d", &next_id);

    // RBC stabilisce se TRENO può progredire o meno
    const bool next_st_or_rbc_data_next_free = (next_stazione || !rbc_data->segms[next_id - 1]);
    const bool next_st_or_next_segm_corr = is_segm_status_correct(rbc_data, next_id, next_pos, next_stazione);
    const bool curr_st_or_curr_segm_corr = is_segm_status_correct(rbc_data, curr_id, curr_pos, curr_stazione);

    const bool auth = next_st_or_rbc_data_next_free && next_st_or_next_segm_corr && curr_st_or_curr_segm_corr;

    // printf("%d&%d&%d=%d\n", next_st_or_rbc_data_next_free, next_st_or_next_segm_corr, curr_st_or_curr_segm_corr, auth);

    // RBC invia autorizzazione a TRENO
    if(send(client_fd, &auth, sizeof(auth), 0) == -1) throw_err("serve_req | send");
    printf("RBC\t| Inviata autorizzazione %d a TRENO %d.\n", auth, num_train);

    // TRENO e' stato servito  
    close(client_fd);

    // aggiornamento di rbc_data se richiesta e' accolta
    if(auth) {
        if(next_stazione) {
            rbc_data->stations[next_id - 1]++;

            // TRENO ha raggiunto destinazione, allora TRENO prossimo alla sua terminazione
            kill(getppid(), SIGUSR1);
            //print_rbc_data(*rbc_data);
        }
        else rbc_data->segms[next_id - 1] = true;

        if(curr_stazione) rbc_data->stations[curr_id - 1]--;
        else rbc_data->segms[curr_id - 1] = false;

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

    exit(EXIT_SUCCESS); 
}

// se valore in file segmento non corrisponde a quello in strutt dati RBC allora false, true altrimenti
bool is_segm_status_correct(rbc_data_t *rbc_data, int segm_id, char *segm_name, bool stazione) {
    // se posizione non e' segmento allora true
    if(stazione) return true;

    const bool segm_file_val = !is_segm_free(segm_name);
    const bool rbc_segm_val = rbc_data->segms[segm_id - 1];
    // printf("RBC\t| segm_id=%d segm_file_val=%d rbc_segm_val=%d\n", segm_id, segm_file_val, rbc_segm_val);

    return segm_file_val == rbc_segm_val;
}

// aggiornamento log RBC
void update_rbc_log(const int num_train, char *curr_pos, char* next_pos, const bool auth) {
    // apertura file
    int fd;
    if((fd = open(RBC_LOG, O_CREAT | O_WRONLY | O_APPEND, 0666)) == -1) throw_err("update_train_log | open");

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
    if((write(fd, write_line, line_len)) == -1) throw_err("update_train_log | write");

    close(fd);
}

// void print_rbc_data(rbc_data_t data) {
//     char rbc_str[64] = "segm: ";
//     char tmp_str[8];
//     for(int i=0; i<N_SEGM; i++) {
//         sprintf(tmp_str, "%d", data.segms[i]);
//         strcat(rbc_str, tmp_str);
//     }
//     strcat(rbc_str, " | stat: ");
//     for(int i=0; i<N_STATIONS; i++) {
//         sprintf(tmp_str, "%d", data.stations[i]);
//         strcat(rbc_str, tmp_str);
//     }
//     printf("%s\n", rbc_str);
// }

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

    // eliminazione shared memory
    shm_unlink(SHM_NAME);
    printf("RBC\t| Eliminata SHM %s.\n", SHM_NAME);

    // eliminazione server
    unlink(SERVER_NAME);
    printf("RBC\t| Chiuso server %s.\n", SERVER_NAME);

    printf("RBC\t| Terminazione esecuzione.\n");
    exit(EXIT_SUCCESS);
}