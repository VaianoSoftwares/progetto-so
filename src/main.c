#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/protocol.h"
#include "../include/error.h"
#include "../include/wait_children.h"

// COSTANTI GLOBALI
const char *padre_tr_exec = "./bin/padre_treni";
const char *reg_exec = "./bin/registro";
const char *rbc_exec = "./bin/rbc";

const char *log_dir = "log";

// PROTOTIPI FUNZIONI MAIN
void parse_cmd_args(const int, char* [], cmd_args*);
bool are_args_correct(const cmd_args*);
void registro_and_treni(const cmd_args);

/*---------------------------------------------------------------------------------------------------------------------------*/
// FUNZIONI MAIN

// main
int main(int argc, char *argv[]) {
    // argomenti passati da cmd vengono interpretati
    cmd_args args;
    parse_cmd_args(argc, argv, &args);
    printf("MAIN\t| ETCS%d MAPPA%d RBC=%d\n", args.etcs, args.mappa, args.rbc);

    // creazione directory log
    mkdir(log_dir, 0777);

    // se ETCS2 e RBC allora esecuzione processo RBC
    if(args.etcs == 2 && args.rbc) {
        execl(rbc_exec, rbc_exec, NULL);
        throw_err("main | execl");
    }
    // esecuzione processi PADRE_TRENI e REGISTRO altrimenti
    else registro_and_treni(args);

    return EXIT_SUCCESS;
}

// argomenti passati da cmd vengono interpretati
void parse_cmd_args(const int argc, char* argv[], cmd_args *dest) {
    dest->etcs = dest->mappa = dest->rbc = 0;    
    
    for(int i=1; i<argc; i++) {
        // parse argomento ETCS
        if(strlen(argv[i]) > 4 && !strncmp("ETCS", argv[i], 4)) sscanf(argv[i], "ETCS%d", &dest->etcs);
        // parse argomento MAPPA
        else if(strlen(argv[i]) > 5 && !strncmp("MAPPA", argv[i], 5)) sscanf(argv[i], "MAPPA%d", &dest->mappa);
        // parse argomento RBC
        else if(!strcmp("RBC", argv[i])) dest->rbc = true;
        // argomento non valido
        else throw_err("parse_cmd_args | invalid argument");
    }

    // controllo valore argomenti
    if(!are_args_correct(dest)) throw_err("parse_cmd_args | invalid arguments");
}

// controllo correttezza valori argomenti cmd
bool are_args_correct(const cmd_args *args) {
  return args->etcs > 0 && args->etcs <= N_ETCS && args->mappa > 0 && args->mappa <= N_MAPS;
}

// crea processi REGISTRO e PADRE_TRENI
void registro_and_treni(const cmd_args args) {
    // eliminazione RBC log se esiste e processo RBC non contemplato in data istanza 
    if(args.etcs == 1) unlink(RBC_LOG);

    char etcs_str[4], mappa_str[4];
    sprintf(etcs_str, "%d", args.etcs);
    sprintf(mappa_str, "%d", args.mappa);

    // creazione processo REGISTRO
    pid_t pid;
    if((pid = fork()) == 0) {
        execl(reg_exec, reg_exec, etcs_str, mappa_str, NULL);
        throw_err("registro_and_treni | execl");
    }
    else if(pid == -1) throw_err("registro_and_treni | fork");
    printf("MAIN\t| Creato processo REGISTRO\n");

    // creazione processo PADRE_TRENI
    if((pid = fork()) == 0) {
        execl(padre_tr_exec, padre_tr_exec, etcs_str, NULL);
        throw_err("registro_and_treni | execl");
    }
    else if(pid == -1) throw_err("registro_and_treni | fork");
    printf("MAIN\t| Creato processo PADRE_TRENI\n");

    // processo principale in attesa di terminazione di REGISTRO e PADRE_TRENI
    wait_children();
    printf("MAIN\t| REGISTRO e PADRE_TRENI: esecuzione terminata\n");
}