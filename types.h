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

typedef struct{
    char type; 
    unsigned int length; 
    char* data; 
}Messaggio; 

//Struttura dati giocatore
typedef struct{
    int socket; 
    char username[USERNAME_LENGTH]; 
    int score; 
    int connesso; 
    int in_gioco; 
    pthread_t tid; 
}Giocatore; 

typedef struct{
    char paroliere[DIM_MATRIX][DIM_MATRIX]; 
    Giocatore lista_giocatori[MAX_CLIENT]; 
    int count_giocatori; 
    pthread_t thread_id; 
    char * data_filename; 
    FILE * matrix_file; 
}Server_data; 

typedef struct{
    int client_fd; 
    Server_data server_data; 
}ClientHandlerArgs; 

typedef struct{
    int registrato; 
    int socket_fd; 
}Client_t; 



//Prototipi funzioni

//Thread - server
void inizializza_server_data(Server_data*);

void *client_handler(void*); 

//Thread - client
void *server_handler(void*); 

//PAROLIERE 
void matrice_casuale(Server_data *); 

void genera_matrice(Server_data *); 

void stampa_matrice(Server_data); 

//Gestione utente
int username_occupato(Server_data*, char*); 

//MESSAGGI
Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 

//Lista giocatori
void inserisci_utente(Server_data*, int, char*); 

void stampa_lista_giocatori(Server_data*);