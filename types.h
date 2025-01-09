#include <stdio.h>
#include <pthread.h>


#define MSG_OK 'K'
#define MSG_ERR 'E'
#define MSG_REGISTRA_UTENTE 'R'
#define MSG_MATRICE 'M'
#define MSG_TEMPO_PARTITA 'T'
#define MSG_TEMPO_ATTESA 'A'
#define MSG_PAROLA 'W'
#define MSG_PUNTI_FINALI 'F'
#define MSG_PUNTI_PAROLA 'P'
#define MSG_SERVER_SHUTDONW 'B'
#define MSG_POST_BACHECA 'H'
#define MSG_SHOW_BACHECA 'S'

#define DIM_MATRIX 4
#define MAX_BUFFER 1024
#define MAX_CLIENT 32
#define USERNAME_LENGTH 11

#include "trie.h"

typedef struct{
    char type; 
    unsigned int length; 
    char* data; 
}Messaggio; 

//Struttura dati giocatore
typedef struct nodoG{
    int socket; 
    char username[USERNAME_LENGTH]; 
    int score; 
    //unsigned int stato; //0 non connesso, 1 connesso, 2 connesso ma non in gioco, 3 in gioco
    int connesso;
    int in_gioco; 
    pthread_t tid; 
    struct nodoG * next; 
}Giocatore; 

typedef struct{
    char nome[USERNAME_LENGTH]; 
    int punti; 
}Punti_fine; 

typedef struct{
    Punti_fine buffer[MAX_CLIENT]; 
    int head; 
    int tail; 
    int counter; 
    pthread_mutex_t mutex_buffer_c; 
    pthread_cond_t buffer_not_full;
    pthread_cond_t buffer_not_empty; 
    pthread_cond_t ready_buffer; 
}Buffer_circolare;

typedef struct{
    char paroliere[DIM_MATRIX][DIM_MATRIX]; 
    Giocatore *lista_giocatori; 
    int count_giocatori; 
    char * data_filename; 
    FILE * matrix_file; 
    char * classifica; 
    pthread_mutex_t mutex_server_data;
    pthread_mutex_t mutex_tempo; 
    pthread_cond_t cond_classifica; 
    pthread_cond_t fine_partita; 
    TrieNode * root_trie; 
    int durata_partita; 
    int partita_in_corso;
    int prima_partita;
    int timer;
    Buffer_circolare buffer_punteggi;
}Server_data; 

typedef struct{
    int client_fd; 
    Server_data *server_data; 
}ClientHandlerArgs;

typedef struct Parola{
    char * parola;
    struct Parola * next; 
}Parola; 

typedef struct{
    int registrato; 
    int socket_fd; 
    char paroliere_client[DIM_MATRIX][DIM_MATRIX]; 
    Parola *lista_parole; 
}Client_t; 


//Thread - client
void *server_handler(void*); 

int inserisci_parola_in_lista(Client_t*, char*);

void stampa_matrice(char [DIM_MATRIX][DIM_MATRIX]);

void rimuovi_parole(Client_t*);

void stringa_in_paroliere(char *, Client_t*); 

Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 

//Prototipi funzioni

//Thread - server
void inizializza_server_data(Server_data*);

void *client_handler(void*); 

void *gestione_tempo_partita(void*); 

void *scorer(void*);

//PAROLIERE 
void matrice_casuale(Server_data *); 

void genera_matrice(Server_data *); 

void stampa_matrice(char [DIM_MATRIX][DIM_MATRIX]); 

char * paroliere_in_stringa(char [DIM_MATRIX][DIM_MATRIX]); 

int dfs_rec(char [DIM_MATRIX][DIM_MATRIX], int, int, char *, int, int [DIM_MATRIX][DIM_MATRIX]);

int parola_presente(char [DIM_MATRIX][DIM_MATRIX], char*);

//BufferCircolare

void inizializza_buffer_circolare(Buffer_circolare*); 

void produttore(Buffer_circolare *, int, char*);

Punti_fine consumatore(Buffer_circolare*);

//Gestione utente
int cerca_giocatore(Server_data*, char*);

Giocatore * restituisci_giocatore(Server_data*, int); 

void cancella_utente(Server_data*, int); 

//MESSAGGI
Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 

//Lista giocatori
void inserisci_giocatore(Server_data*, int); 

void registra_giocatore(Server_data*, int, char*);

void aggiorna_punti_giocatore(Server_data*, int, char*); 

void stampa_lista_giocatori(Server_data*);

void elimina_lista(Server_data*); 

void cleanup(Server_data*);
