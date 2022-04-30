#pragma once

#include "../include/bool.h"

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
typedef struct cmd_args {
    int etcs;
    BOOL rbc;
    int mappa;
} cmd_args;

typedef struct segm_t {
    char *name;
    BOOL reserved;
} segm_t;

typedef struct itin {
    char *start;
    char *end;
    char *path;
} itin;

typedef struct rbc_data_t {
    BOOL segms[N_SEGM];
    int stations[N_STATIONS];
    char *paths[N_TRAINS];
} rbc_data_t;

typedef itin map_t[N_TRAINS];
