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

void genera_matrice(){

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