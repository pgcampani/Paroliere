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

void matrice_casuale(Server_data * server_data){

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            char carattere_random = 65 + rand() % 26; 
            server_data->paroliere[i][j] = carattere_random;
        }
    }
}

/*void genera_matrice(Server_data * server_data){
    int curr_position = 0;   //Posizione corrente nel file matrix.txt

    FILE * matrix_file = fopen(server_data->data_filename, "r"); 

    if(matrix_file != NULL){
        fseek(matrix_file, curr_position, SEEK_SET);    //Mi posiziono nel file
        char linea[MAX_BUFFER]; 

        if(fgets(linea, sizeof(linea), matrix_file) != NULL){
            //Prendo la riga del file e tokenizzo i caratteri
            char *tok = strtok(linea, " "); 

            for(int i = 0; i < DIM_MATRIX && tok != NULL; i++){
                for(int j = 0; j < DIM_MATRIX && tok != NULL; j++){
                    if(strcmp(tok, "Qu") == 0){
                        //Se nella riga trovo Qu la tratto come un carattere speciale nella matrice
                        server_data->paroliere[i][j] = 'Q'; 
                    }
                    else{
                        //Per tutti gli altri caratteri che leggo inserisco nel paroliere
                        server_data->paroliere[i][j] = tok[0]; 
                    }
                    tok = strtok(NULL, " "); 
                }
            }
            //Aggiorno la posizione corrente 
            curr_position = ftell(matrix_file); 
        }
        else{
            //Genero una matrice di caratteri casuali
            curr_position = 0; 
            matrice_casuale(server_data); 
        }
        fclose(matrix_file); 
    }
    else{
        //Non ho passato nessun file, genero paroliere casualmente
        matrice_casuale(server_data); 
    }
}*/

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

void stampa_matrice(Server_data server_data){
    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            printf("%c ", server_data.paroliere[i][j]); 
        }
        printf("\n"); 
        printf("\n"); 
    }
}



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

    SYSC(retvalue, read(file_descriptor, &msg->data, sizeof(unsigned int)), "Nella read"); 

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

int username_occupato(Server_data *server_data, char *username){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(server_data->lista_giocatori[i].socket != -1 && strcmp(username, server_data->lista_giocatori[i].username) == 0){
            return 1; 
        }
    }
    return 0; 
}

int inserisci_utente(Server_data *server_data, int client_fd, char *username){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(server_data->lista_giocatori[i].socket == -1){
            server_data->lista_giocatori[i].socket = client_fd; 
            strncpy(server_data->lista_giocatori[i].username, username, USERNAME_LENGTH); 
            server_data->lista_giocatori[i].username[USERNAME_LENGTH] = '\0'; 
            server_data->lista_giocatori[i].in_gioco = 1; 
            server_data->lista_giocatori[i].connesso = 1; 
            server_data->lista_giocatori[i].tid = pthread_self(); 
            return 1; 
        }
    }
    return 0; 
}

void stampa_lista_giocatori(Server_data *server_data){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(server_data->lista_giocatori[i].connesso != 0){
            printf("%s socket: %d connesso: %d tid: %ld\n", server_data->lista_giocatori[i].username,server_data->lista_giocatori[i].socket, server_data->lista_giocatori[i].connesso, server_data->lista_giocatori[i].tid);
        }
    }
}