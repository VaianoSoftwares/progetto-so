// MACROS
#define N_TRAINS 5
#define N_STATIONS 8
#define N_SEGM 16
#define N_MAPS 2
#define N_ETCS 2
#define DEFAULT_PROTOCOL 0
#define N_RBC_PIPE 0
#define SERVER_NAME "data/rbc_server"
#define PIPE_NAME "data/reg_pipe"
#define SHM_SIZE 512
#define SHM_NAME "rbc_data"
#define RBC_LOG "log/RBC.log"

// TYPEDEFS
typedef enum { FALSE, TRUE } BOOL;

typedef struct {
    int etcs;
    BOOL rbc;
    int mappa;
} cmd_args;

typedef struct {
    char *name;
    BOOL reserved;
} segm_t;

typedef struct {
    char *start;
    char *end;
    char *path;
} itin;

typedef struct {
    BOOL segms[N_SEGM];
    int stations[N_STATIONS];
    char *paths[N_TRAINS];
} rbc_data_t;

typedef itin map_t[N_TRAINS];

// PROTOTIPI FUNZIONI MAIN
void parse_cmd_args(const int, char* [], cmd_args*);
BOOL are_args_correct(const cmd_args*);
void registro_and_treni(const cmd_args);
void wait_children();
// PROTOTIPI FUNZIONI PADRE_TRENI
void padre_treni(const int);
void init_segm_files();
void create_segm_file(int);
// PROTOTIPI FUNZIONI REGISTRO
void registro(const cmd_args);
int open_pipe(const char*, const int);
void close_pipe(const char*, int, int);
void send_map_to_trains(const map_t);
void send_itin(int, const itin);
void send_map_to_rbc(const map_t map);
char* map_to_str(const map_t map);
// PROTOTIPI FUNZIONI TRENO
void treno(const int, const int);
int connect_to_pipe(const char*, int);
char* get_itin(const int);
void update_train_log(int, char*, char*);
BOOL go_to_next_pos(const int, const int, char*, char*);
BOOL allowed_to_proceed(const int, const int, char*, char*, const BOOL);
BOOL is_stazione(char*);
BOOL is_segm_free(char*);
void set_segm_status(const int, const BOOL);
BOOL allowed_by_rbc(int, char*, char*);
int connect_to_rbc();
// PROTOTIPI FUNZIONI RBC
void rbc();
void get_map(char**);
void init_rbc_data(rbc_data_t*);
void check_if_empty_paths(char**);
int create_rbc_server();
void run_server(int);
void serve_req(int);
BOOL is_segm_status_correct(rbc_data_t*, int, char*, BOOL);
void update_rbc_log(const int, char*, char*, const BOOL);
// SIGNALS HANDLERS
void usr1_handl(int);