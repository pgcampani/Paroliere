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
#define MSG_LOGIN_UTENTE 'L'
#define MSG_CANCELLA_UTENTE 'D'

#define DIM_MATRIX 4
#define LINE_SIZE 128
#define MAX_CLIENT 4
#define USERNAME_LENGTH 11
#define MAX_MESSAGES 8
#define MAX_MSG_LEN 128
#define MAX_BUFFER 1024

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
    int connesso; 
    int in_gioco; 
    int disconnesso; 
    pthread_t tid; 
    int timeout; 
    struct nodoG * next; 
}Giocatore; 

typedef struct{
    char nome[USERNAME_LENGTH]; 
    int punti; 
}Punti_fine; 

typedef struct{
    int counter_punteggi; 
    Punti_fine array[MAX_CLIENT]; 
    pthread_mutex_t mutex_array; 
}Array_punteggi; 

typedef struct{
    char username[USERNAME_LENGTH]; 
    char post[MAX_MSG_LEN + 1]; 
}Post_bacheca; 

typedef struct{
    Post_bacheca post_bacheca[MAX_MESSAGES];
    int head;
    int tail; 
    int count;
    pthread_mutex_t mutex_bacheca; 
}Bacheca; 

typedef struct{
    char paroliere[DIM_MATRIX][DIM_MATRIX]; 
    Giocatore *lista_giocatori; 
    int count_giocatori;
    int utenti_attivi;  
    char * data_filename; 
    FILE * matrix_file; 
    FILE * log_file; 
    char * classifica; 
    int classifica_pronta; 
    int punteggi_pronti;
    pthread_mutex_t mutex_server_data;
    pthread_mutex_t mutex_tempo; 
    pthread_mutex_t mutex_log; 
    pthread_cond_t cond_punteggi_pronti; 
    pthread_cond_t cond_classifica_pronta; 
    pthread_cond_t inizio_partita; 
    pthread_cond_t fine_partita; 
    TrieNode * root_trie; 
    int durata_partita; 
    int partita_in_corso;
    int timer;
    int timeout;
    //int terminazione_thread; 
    Array_punteggi array_punteggi[MAX_CLIENT]; 
    Bacheca bacheca; 
}Server_data; 

typedef struct{
    int client_fd; 
    Server_data *server_data;
    Giocatore *giocatore;  
    pthread_t messaggi_tid;
    pthread_t punti_tid;  
    int termina;
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

void *handler_messaggi(void*); 

void *handler_punteggio(void*); 

void *gestione_tempo_partita(void*); 

void *scorer(void*);

void *handler_timeout(void*); 

//FILE DI LOG
void inizializza_log(Server_data*, char*); 

void log_event(Server_data*, char*, char*, char*);

//PAROLIERE 
void matrice_casuale(Server_data *); 

void genera_matrice(Server_data *); 

void stampa_matrice(char [DIM_MATRIX][DIM_MATRIX]); 

char * paroliere_in_stringa(char [DIM_MATRIX][DIM_MATRIX]); 

int dfs_rec(char [DIM_MATRIX][DIM_MATRIX], int, int, char *, int, int [DIM_MATRIX][DIM_MATRIX]);

int parola_presente(char [DIM_MATRIX][DIM_MATRIX], char*);

//Array Punteggio

void inizializza_array_punteggi(Array_punteggi*); 

int inserisci_punteggio(Array_punteggi*, char*, int); 

void reset_punteggi(Server_data*); 

int ordina_punteggi(const void*, const void*);

//Gestione utente
int cerca_giocatore(Server_data*, char*);

Giocatore * restituisci_giocatore(Server_data*, int); 

int login(Server_data*, char*, int); 

void logout_utente(Server_data*, int); 

void cancella_utente(Server_data*, int); 

//BACHECA
void inizializza_bacheca(Bacheca*);

int post_bacheca(Bacheca*, char*, char*);

void show_bacheca(Bacheca*, char*, size_t); 

//MESSAGGI
Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 

//Lista giocatori
void inserisci_giocatore(Server_data*, int, int); 

void registra_giocatore(Server_data*, int, char*);

int is_alphabetical(char *); 

void aggiorna_punti_giocatore(Server_data*, int, char*); 

void stampa_lista_giocatori(Server_data*);

void elimina_lista(Server_data*); 

void cleanup(Server_data*);