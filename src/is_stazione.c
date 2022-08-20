#include "../include/is_stazione.h"

// data una stringa stabilisce se questa e' nome di una stazione
bool is_stazione(char *str) {
    char id_str[8] = { 0 };
    int num_stazione;

    if(str[0] != 'S') return false;

    strcpy(id_str, str + 1);
    sscanf(id_str, "%d", &num_stazione);

    // identificativo stazione non valido
    if(num_stazione <= 0 || num_stazione > N_STATIONS) throw_err("is_stazione");

    return true;
}