CC = gcc

# Flag
CFLAGS = -Wall -pthread -g -Iinclude

# File sorgente
SRCS_SERVER = src/server.c src/funzioni.c src/trie.c
SRCS_CLIENT = src/client.c src/funzioni.c src/trie.c

# File oggetto
OBJ_SERVER = $(SRCS_SERVER:.c=.o)
OBJ_CLIENT = $(SRCS_CLIENT:.c=.o)

# Eseguibili
TARGET_SERVER = paroliere_srv
TARGET_CLIENT = paroliere_cl

all: $(TARGET_SERVER) $(TARGET_CLIENT)

# Regola per server
$(TARGET_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(OBJ_SERVER)

# Regola per client
$(TARGET_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(OBJ_CLIENT)

# Regola per compilare i file .c in .o
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Pulizia dei file compilati
clean:
	rm -f src/*.o $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean