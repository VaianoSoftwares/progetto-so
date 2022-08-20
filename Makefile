# usage: make

# compilatore
CC = gcc

# nome directory principali
BIN_DIR = bin
INCL_DIR = include
SRC_DIR = src
OBJ_DIR = obj

# flag compilatore
INCL_FLAG = $(addprefix -I,$(INCL_DIR))
CFLAGS = $(INCL_FLAG) -MMD -MP -g

# path file sorgente
SRCS := $(shell find $(SRC_DIR) -name '*.c')
# path file oggetto
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
# path file dependenze
DEPS := $(OBJS:.o=.d)

# nome file eseguibili
MAIN_BIN = progetto-so
PTRENI_BIN = padre_treni
REG_BIN = registro
TRENO_BIN = treno
RBC_BIN = rbc

# lista file oggetto necessari per il linking
_MAIN_OBJS = main error wait_children
MAIN_OBJS := $(_MAIN_OBJS:%=$(OBJ_DIR)/%.o)
_PTRENI_OBJS = padre_treni error wait_children
PTRENI_OBJS := $(_PTRENI_OBJS:%=$(OBJ_DIR)/%.o)
_REG_OBJS = registro error wait_children
REG_OBJS := $(_REG_OBJS:%=$(OBJ_DIR)/%.o)
_TRENO_OBJS = treno error curr_time segm_free is_stazione connect_pipe
TRENO_OBJS := $(_TRENO_OBJS:%=$(OBJ_DIR)/%.o)
_RBC_OBJS = rbc error curr_time segm_free wait_children is_stazione connect_pipe
RBC_OBJS := $(_RBC_OBJS:%=$(OBJ_DIR)/%.o)

# target senza regole
.PHONY: clean

# usage: make
# creazione tutti file eseguibili
all: $(BIN_DIR)/$(MAIN_BIN) $(BIN_DIR)/$(PTRENI_BIN) $(BIN_DIR)/$(REG_BIN) $(BIN_DIR)/$(TRENO_BIN) $(BIN_DIR)/$(RBC_BIN)


# creazione eseguibile progetto-so
$(BIN_DIR)/$(MAIN_BIN): $(MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(MAIN_OBJS) -o $@

# creazione eseguibile padre_treni
$(BIN_DIR)/$(PTRENI_BIN): $(PTRENI_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(PTRENI_OBJS) -o $@

# creazione eseguibile registro
$(BIN_DIR)/$(REG_BIN): $(REG_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(REG_OBJS) -o $@

# creazione eseguibile treno
$(BIN_DIR)/$(TRENO_BIN): $(TRENO_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(TRENO_OBJS) -o $@

# creazione eseguibile rbc
$(BIN_DIR)/$(RBC_BIN): $(RBC_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(RBC_OBJS) -o $@


# compilazione file sorgente
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


# usage: make clean
# eliminazione file: oggetto, eseguibili, dipendenze, log e segmento
clean:
	rm -rf bin obj log /tmp/MA*.txt /tmp/rbc_server /tmp/reg_pipe*


-include $(DEPS)