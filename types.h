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

typedef struct{
    char type; 
    unsigned int length; 
    char* data; 
}Messaggio; 

typedef struct{
    char paroliere[DIM_MATRIX][DIM_MATRIX]; 
    int count_giocatori; 
    pthread_t thread_id; 
    char * data_filename; 
    FILE * matrix_file; 
}Server_data; 

typedef struct{
    int client_fd; 
    Server_data server_data; 
}ClientHandlerArgs; 



//Prototipi funzioni

//Thread
void *client_handler(void*); 

//PAROLIERE 
void matrice_casuale(Server_data *); 

void genera_matrice(Server_data *); 

void stampa_matrice(Server_data); 

//MESSAGGI
Messaggio * leggi_messaggio(int); 

void invia_messaggio(int, Messaggio*); 