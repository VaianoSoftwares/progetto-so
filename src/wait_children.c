#include "../include/wait_children.h"

// processo padre attende la terminazione dei figli
void wait_children() {
    while(waitpid(0, NULL, 0) > 0) sleep(1);
}