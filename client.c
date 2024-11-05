    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>
    #include <getopt.h>
    #include <pthread.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <errno.h>
    #include <sys/wait.h>

    #include "macros.h"
    #include "types.h"

    #define LINEA_DI_COMANDO 1024

    int main(int argc, char *argv[]){
        //Riceve da linea di comando il nome del server e la porta a cui collegarsi

        if(argc < 2){
            perror("Numero parametri errato!\nUsage:./paroliere_srv nome_server porta_server");
            exit(EXIT_FAILURE); 
        }

        char *nome_server = argv[1]; 
        int porta_server = atoi(argv[2]); 

        struct sockaddr_in server_addr; 

        int socket_fd, retvalue; 

        //Socket
        SYSC(socket_fd, socket(AF_INET, SOCK_STREAM, 0), "Nella socket"); 

        server_addr.sin_family = AF_INET; 
        server_addr.sin_port = htons(porta_server); 
        server_addr.sin_addr.s_addr = INADDR_ANY; 

        //Connect
        SYSC(retvalue, connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)), "Nella connect"); 

        //Client_t - struttura contenente i dati del client da passare al thread

        pthread_t risposta_handler;

        pthread_create(&risposta_handler, NULL, server_handler, (void*)client); 

        //Gestione comandi

        char input[LINEA_DI_COMANDO]; 

        Messaggio *msg = (Messaggio*)malloc(sizeof(Messaggio));

        if(msg == NULL){
            perror("Nella malloc"); 
            exit(EXIT_FAILURE); 
        }

        printf("COMANDI:\n");
        printf("- registra_utente nome_utente - per registrarsi\n");
        printf("- matrice - per richiedere la matrice del gioco in corso\n");
        printf("- parola p - per inviare una parola trovata nel paroliere\n"); 
        printf("- fine - per uscire dal gioco\n"); 

        while(1){
            printf("[PROMPT PAROLIERE]-->"); 

            if(fgets(input, sizeof(input), stdin) == NULL){
                printf("Errore lettura comando\n");
                continue; 
            }

            size_t len = strlen(input); //Prende la lunghezza del comando letto in input

            //Rimuovo carattere '\n'
            if(len > 0 && input[len - 1] == '\n'){
                input[len - 1] = '\0'; 
            }

            //Token
            char * comando = strtok(input, " "); 
            char * argomento = strtok(NULL, " "); 

            //DA QUI VERIFICARE I COMANDI LETTI DA INPUT E AGIRE DI CONSEGUENZA 
        }
    }