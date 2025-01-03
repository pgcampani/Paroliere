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
    char paroliere[DIM_MATRIX][DIM_MATRIX]; 
    Giocatore *lista_giocatori; 
    int count_giocatori; 
    char * data_filename; 
    FILE * matrix_file; 
    pthread_mutex_t mutex_server_data; 
    pthread_mutex_t mutex_tempo; 
    int durata_partita; 
    int partita_in_corso;
}Server_data; 

typedef struct{
    int client_fd; 
    Server_data *server_data; 
}ClientHandlerArgs; 

typedef struct{
    int registrato; 
    int socket_fd; 
    char paroliere_client[DIM_MATRIX][DIM_MATRIX]; 
}Client_t; 


//Prototipi funzioni

//Thread - server
void inizializza_server_data(Server_data*);

void *client_handler(void*); 

void *gestione_tempo_partita(void*); 

//Thread - client
void *server_handler(void*); 

//void *commands(void*);

//PAROLIERE 
void matrice_casuale(Server_data *); 

void genera_matrice(Server_data *); 

void stampa_matrice(char [DIM_MATRIX][DIM_MATRIX]); 

char * paroliere_in_stringa(char [DIM_MATRIX][DIM_MATRIX]); 

void stringa_in_paroliere(char *, Client_t*); 

//Gestione utente
int cerca_giocatore(Server_data*, char*); 

//MESSAGGI
Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 

//Lista giocatori
void inserisci_giocatore(Server_data*, int); 

void registra_giocatore(Server_data*, int, char*);

void stampa_lista_giocatori(Server_data*);

void elimina_lista(Server_data*); 


void cleanup(Server_data*);
