#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>

#include "types.h"
#include "macros.h"


//FUNZIONI PER PAROLIERE

void matrice_casuale(Server_data * server_data){

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            char carattere_random = 65 + rand() % 26; 
            server_data->paroliere[i][j] = carattere_random;
        }
    }
}

void genera_matrice(Server_data * server_data){

    if(server_data->matrix_file == NULL && server_data->data_filename != NULL){
        //Apri il file solo se il file non è già aperto
        server_data->matrix_file = fopen(server_data->data_filename, "r");
        if(server_data->matrix_file == NULL){
            perror("Errore apertura file"); 
            exit(EXIT_FAILURE); 
        }    
    }

    if(server_data->matrix_file != NULL){
        char linea[MAX_BUFFER]; 
        if(fgets(linea, sizeof(linea), server_data->matrix_file) != NULL){
            //Tokenizzo i caratteri e inserisco nella matrice
            char *tok = strtok(linea, " ");
            for(int i = 0; i < DIM_MATRIX && tok != NULL; i++){
                for(int j = 0; j < DIM_MATRIX && tok != NULL; j++){
                    if(strcmp(tok, "Qu") == 0){
                        //Se nella riga trovo Qu lo tratto come carattere singolo nella matrice
                        server_data->paroliere[i][j] = 'Q'; 
                    }
                    else{
                        //Scrivo direttamente il carattere nella matrice
                        server_data->paroliere[i][j] = tok[0]; 
                    }
                    tok = strtok(NULL, " "); 
                }
            }
        }
        else{
            //Ho raggiunto la fine del file 
            fclose(server_data->matrix_file);
            server_data->matrix_file = NULL; 
            matrice_casuale(server_data); 
        }
    }
    else{
        matrice_casuale(server_data); 
    }
}

void stampa_matrice(char paroliere[DIM_MATRIX][DIM_MATRIX]){

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            if(paroliere[i][j] == 'Q'){
                printf("Qu  "); 
            }
            else{
                printf("%c  ", paroliere[i][j]); 
            }
        }
        printf("\n"); 
        printf("\n"); 
    }
}

//FUNZIONI PER LO SCAMBIO DI MESSAGGI

Messaggio* leggi_messaggio(int file_descriptor){

    int retvalue; 

    Messaggio * msg = (Messaggio*)malloc(sizeof(Messaggio)); 
    if(msg == NULL){
        perror("Errore nella malloc"); 
        exit(EXIT_FAILURE); 
    }

    //Inizializzo i valori default
    msg->data = NULL; 
    msg->type = '\0'; 
    msg->length = 0; 

    SYSC(retvalue, read(file_descriptor, &msg->length, sizeof(unsigned int)), "Nella read"); 

    SYSC(retvalue, read(file_descriptor, &msg->type, sizeof(char)), "Nella read"); 

    //Alloco memoria per contenuto messaggio 
    msg->data = (char*)malloc(sizeof(char) * msg->length + 1); 
    if(msg->data == NULL){
        perror("Nella malloc"); 
        exit(EXIT_FAILURE); 
    }

    SYSC(retvalue, read(file_descriptor, msg->data, msg->length), "Nella read"); 

    msg->data[msg->length] = '\0';  //Terminatore stringa

    return msg; 
}

void invia_messaggio(int file_descriptor, Messaggio *msg){

    int retvalue; 

    SYSC(retvalue, write(file_descriptor, &msg->length, sizeof(unsigned int)), "Nella write"); 

    SYSC(retvalue, write(file_descriptor, &msg->type, sizeof(char)), "Nella write");

    SYSC(retvalue, write(file_descriptor, msg->data, sizeof(char) * msg->length), "Nella write"); 

}



//FUNZIONI SERVER


void inizializza_server_data(Server_data *server_data){
    //Lista giocatori
    server_data->lista_giocatori = NULL; 
    server_data->count_giocatori = 0; 
    server_data->matrix_file = NULL; 
    pthread_mutex_init(&server_data->mutex_server_data, NULL); 
}


void inserisci_giocatore(Server_data* server_data, int socket, char *username){

    Giocatore *nuovo_giocatore = (Giocatore*)malloc(sizeof(Giocatore));
    if(nuovo_giocatore == NULL){
        perror("Errore nella malloc"); 
        return;
    }

    nuovo_giocatore->socket = socket; 
    strncpy(nuovo_giocatore->username, username, USERNAME_LENGTH); 
    nuovo_giocatore->username[USERNAME_LENGTH] = '\0'; 
    nuovo_giocatore->connesso = 1; 
    nuovo_giocatore->in_gioco = 0; 
    nuovo_giocatore->score = 0; 
    nuovo_giocatore->tid = pthread_self(); 

    pthread_mutex_lock(&server_data->mutex_server_data); 

    nuovo_giocatore->next = server_data->lista_giocatori; 

    server_data->lista_giocatori = nuovo_giocatore; 

    server_data->count_giocatori++; 

    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

void cancella_giocatore(Server_data* server_data, int socket){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *temp = server_data->lista_giocatori;
    Giocatore *prev = NULL; 

    while(temp != NULL && temp->socket != socket){
        prev = temp; 
        temp = temp->next; 
    }

    if(temp == NULL){
        printf("Giocatore non trovato\n"); 
        return; 
    }

    if(prev == NULL){
        //Rimuovo il primo nodo
        server_data->lista_giocatori = temp->next; 
    }
    else{
        prev->next = temp->next; 
    }

    server_data->count_giocatori--; 
    
    pthread_mutex_unlock(&server_data->mutex_server_data); 

    free(temp); 
}

void elimina_lista(Server_data* server_data){

    pthread_mutex_lock(&server_data->mutex_server_data); 
    
    Giocatore *temp = server_data->lista_giocatori; 

    while(temp != NULL){
        Giocatore *giocatore_da_rimuovere = temp;
        temp = temp->next; 
        free(giocatore_da_rimuovere); 
    }
    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

int cerca_giocatore(Server_data* server_data, char* username){
    
    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *temp = server_data->lista_giocatori;

    while(temp != NULL){
        if(strcmp(temp->username, username) == 0){
            pthread_mutex_unlock(&server_data->mutex_server_data); 
            return 1; 
        }
        temp = temp->next; 
    }

    pthread_mutex_unlock(&server_data->mutex_server_data); 

    return 0; 
}


void stampa_lista_giocatori(Server_data* server_data){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *temp = server_data->lista_giocatori; 

    while(temp != NULL){
        printf("Username: %s, Socket: %d, Connesso: %d\n", temp->username, temp->socket, temp->connesso);
        temp = temp->next; 
    }

    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

char * paroliere_in_stringa(char m[DIM_MATRIX][DIM_MATRIX]){

    int size_str = DIM_MATRIX * DIM_MATRIX + 1;
    int k = 0; //Mantiene l'indice della stringa
    char * str = (char*)malloc(size_str * sizeof(char)); 

    if(str == NULL){
        perror("Nella malloc"); 
        exit(EXIT_FAILURE); 
    }

    str[0] = '\0'; //Inizializzo stringa 

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            str[k++] = m[i][j]; 
        }
    }
    str[k] = '\0'; 
    return str; 
}

void stringa_in_paroliere(char * str, Client_t * client){
    int k = 0; 

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
           client->paroliere_client[i][j] = str[k]; 
           k++; 
        }
    }
}

//FUNZIONE DI CLEANUP
void cleanup(Server_data *server_data){

    if(server_data->matrix_file){
        free(server_data->matrix_file); 
    }

    elimina_lista(server_data); 
    //invia_shtudown

    pthread_mutex_destroy(&server_data->mutex_server_data); 
}