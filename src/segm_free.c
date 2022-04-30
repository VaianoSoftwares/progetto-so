#include "../include/segm_free.h"

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