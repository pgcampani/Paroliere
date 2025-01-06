    #define _XOPEN_SOURCE 700 // Per sigaction
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

    volatile sig_atomic_t exit_signal = 0; 
    pthread_mutex_t mutex_running = PTHREAD_MUTEX_INITIALIZER; 
    pthread_mutex_t mutex_client = PTHREAD_MUTEX_INITIALIZER; 
    pthread_cond_t cond_client = PTHREAD_COND_INITIALIZER; 

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

        Client_t *client = (Client_t*)malloc(sizeof(Client_t));
        
        if(client == NULL){
            perror("Nella malloc"); 
            exit(EXIT_FAILURE); 
        }

        client->registrato = 0; 
        client->socket_fd = socket_fd; 
        client->lista_parole = NULL; 
 
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

            pthread_mutex_lock(&mutex_client);
            printf("[PROMPT PAROLIERE]-->");
            fflush(stdout);
            pthread_mutex_unlock(&mutex_client);

            if(fgets(input, sizeof(input), stdin) == NULL){
                printf("Errore lettura comando\n");
                continue;
            }            
            
            pthread_mutex_lock(&mutex_running);
            if(exit_signal){
                pthread_mutex_unlock(&mutex_running); 
                break; 
            }
            pthread_mutex_unlock(&mutex_running); 

            size_t len = strlen(input); //Prende la lunghezza del comando letto in input

            //Rimuovo carattere '\n'
            if(len > 0 && input[len - 1] == '\n'){
                input[len - 1] = '\0'; 
            }

            //Token
            char * comando = strtok(input, " "); 
            char * argomento = strtok(NULL, " "); 

            if(comando == NULL){
                printf("Nessun comando inserito\n"); 
                continue; 
            }

            if(!client->registrato){
                //Se l'utente non è registrato  gli unici comandi validi sono: registra_utente, aiuto, fine

                if(strcmp(comando, "registra_utente") == 0){
                    if(argomento == NULL){
                        printf("Nessuno username inserito. Usage: registra_utente <nome_utente>\n"); 
                    }
                    else{
                        pthread_mutex_lock(&mutex_client); 
                        msg->type = MSG_REGISTRA_UTENTE; 
                        msg->data = argomento;
                        msg->length = strlen(argomento); 
                        invia_messaggio(socket_fd, msg);
                        pthread_cond_wait(&cond_client, &mutex_client); 
                        pthread_mutex_unlock(&mutex_client); 
                    }
                }

                else if(strcmp(comando, "aiuto") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: aiuto\n"); 
                    }
                    else{
                        printf("COMANDI:\n");
                        printf("- registra_utente nome_utente\n");
                        printf("- matrice - per richiedere la matrice del gioco in corso\n");
                        printf("- parola p - per inviare una parola nel paroliere\n");
                        printf("- fine - per uscire dal gioco\n"); 
                    }
                }

                else if(strcmp(comando, "fine") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: fine\n"); 
                    }
                    else{
                        break; 
                    }
                }
                
                else{
                    printf("Comando non valido. Digita aiuto per vedere i comandi disponibili\n"); 
                }
            }
            else{
                if(strcmp(comando, "registra_utente") == 0){
                    if(argomento == NULL){
                        printf("Comando non valido. Forse cercavi: registra_utente\n"); 
                    }
                    else{
                        printf("Utente già registrato.\n"); 
                    }
                }

                else if(strcmp(comando, "aiuto") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: aiuto\n"); 
                    }
                    else{
                        printf("COMANDI:\n");
                        printf("- registra_utente nome_utente\n");
                        printf("- matrice - per richiedere la matrice del gioco in corso\n");
                        printf("- parola p - per inviare una parola nel paroliere\n");
                        printf("- fine - per uscire dal gioco\n"); 
                    }
                }
                
                else if(strcmp(comando, "parola") == 0){
                    if(argomento == NULL){
                        printf("Nessuna parola inserita\n");
                    }
                    else{
                        pthread_mutex_lock(&mutex_client);
                        msg->type = MSG_PAROLA;
                        msg->data = argomento; 
                        msg->length = strlen(msg->data); 

                        int parola_inviata; 
                        parola_inviata = inserisci_parola_in_lista(client, msg->data);

                        if(parola_inviata){
                            invia_messaggio(socket_fd, msg);
                            pthread_cond_wait(&cond_client, &mutex_client); 
                            pthread_mutex_unlock(&mutex_client);
                        }
                        else{
                            printf("Parola già inviata\n");
                            pthread_mutex_unlock(&mutex_client); 
                        }
                    }
                }

                else if(strcmp(comando, "fine") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: fine\n");
                    }
                    else{
                        break; 
                    }
                }
            }
        }

        pthread_cancel(risposta_handler); 
        pthread_join(risposta_handler, NULL);
        close(client->socket_fd); 
        free(client); 
        free(msg); 
        return 0;
    }

    
    void *server_handler(void* args){

        Client_t *client = (Client_t*)args; 

        int socket_fd = client->socket_fd; 
        int registrato = client->registrato; 

        while(1){

            Messaggio *msg; 

            msg = leggi_messaggio(socket_fd); 

            pthread_mutex_lock(&mutex_client); 

            switch(msg->type){

                case MSG_OK: 
                    client->registrato = 1; 
                    printf("%s\n", msg->data); 
                    break; 
                
                case MSG_ERR:
                    printf("%s\n", msg->data); 
                    if(strcmp(msg->data, "Parita non ancora iniziata. Attendi.") == 0){
                        rimuovi_parole(client); 
                    }
                    pthread_cond_signal(&cond_client); 
                    break;

                case MSG_MATRICE: 
                    stringa_in_paroliere(msg->data, client);
                    stampa_matrice(client->paroliere_client); 
                    break;  

                case MSG_TEMPO_PARTITA:
                    printf("%s\n", msg->data); 
                    pthread_cond_signal(&cond_client);
                    break; 
                
                case MSG_TEMPO_ATTESA: 
                    printf("%s\n", msg->data); 
                    pthread_cond_signal(&cond_client);
                    break; 

                case MSG_PUNTI_PAROLA: 
                    printf("Punti parola: %s\n", msg->data); 
                    pthread_cond_signal(&cond_client); 
                    break; 
                
                case MSG_SERVER_SHUTDONW:

                    printf("%s\n", msg->data); 
                    printf("Digita un tasto qualsiasi per terminare-->"); 
                    fflush(stdout);
                    pthread_mutex_unlock(&mutex_client); 

                    pthread_mutex_lock(&mutex_running); 
                    exit_signal = 1; 
                    pthread_mutex_unlock(&mutex_running);
                    
                    free(msg->data);
                    free(msg); 

                    close(socket_fd);
                    pthread_exit(NULL); 
                    break; 

            }
            pthread_mutex_unlock(&mutex_client); 
            free(msg->data); 
            free(msg); 
        }
    }