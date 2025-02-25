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

    int exit_signal = 0; 
    Messaggio * msg_server;
    pthread_mutex_t mutex_running = PTHREAD_MUTEX_INITIALIZER; 
    pthread_mutex_t mutex_client = PTHREAD_MUTEX_INITIALIZER; 
    pthread_cond_t cond_client = PTHREAD_COND_INITIALIZER; 

    void sigint_handler(int signum){
        if(signum == SIGINT){
            pthread_mutex_lock(&mutex_running); 
            exit_signal = 1; 
            pthread_mutex_unlock(&mutex_running); 

            pthread_mutex_lock(&mutex_client);

            pthread_cond_broadcast(&cond_client);

            pthread_mutex_unlock(&mutex_client); 
        }
    }


    int main(int argc, char *argv[]){

        struct sigaction sa; 
        sa.sa_handler = sigint_handler; 
        sigemptyset(&sa.sa_mask); 
        sa.sa_flags = 0; 
        if(sigaction(SIGINT, &sa, NULL) == -1){
            perror("Errore nella configurazione di sigaction");
            exit(EXIT_FAILURE); 
        }

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

        pthread_mutex_lock(&mutex_client);
        client->registrato = 0; 
        client->socket_fd = socket_fd; 
        client->lista_parole = NULL; 
        pthread_mutex_unlock(&mutex_client); 
 
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
        printf("- login_utente nome utente - per connettersi con uno username già registrato\n"); 
        printf("- matrice - per richiedere il paroliere ed il tempo rimanente alla fine del gioco/pausa\n");
        printf("- parola p - per inviare una parola trovata nel paroliere\n"); 
        printf("- fine - per uscire dal gioco\n"); 

        while(1){
            pthread_mutex_lock(&mutex_running);
            if(exit_signal){
                printf("Esco\n"); 
                pthread_mutex_unlock(&mutex_running); 
                break; 
            }
            pthread_mutex_unlock(&mutex_running); 

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
                printf("Esco\n"); 
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

            pthread_mutex_lock(&mutex_client);
            if(!client->registrato){
                pthread_mutex_unlock(&mutex_client); 
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

                else if(strcmp(comando, "login_utente") == 0){
                    if(argomento == NULL){
                        printf("Nessun username inserito. Usage: login_utente <nome_utente>\n"); 
                    }
                    else{
                        pthread_mutex_lock(&mutex_client);
                        msg->type = MSG_LOGIN_UTENTE; 
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
                        printf("- login_utente nome utente - per connettersi con uno username già registrato\n"); 
                        printf("- matrice - per richiedere il paroliere ed il tempo rimanente alla fine del gioco/pausa\n");
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

                else if(strcmp(comando, "matrice") == 0 || strcmp(comando, "parola") == 0){
                    printf("Comando non valido. Si prega di registrarsi\n");
                }
                
                else{
                    printf("Comando non valido. Digita aiuto per vedere i comandi disponibili\n"); 
                }
            }
            else{
                pthread_mutex_unlock(&mutex_client); 

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
                        printf("- login_utente nome utente - per connettersi con uno username già registrato\n"); 
                        printf("- matrice - per richiedere il paroliere ed il tempo rimanente alla fine del gioco/pausa\n");
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

                else if(strcmp(comando, "matrice") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: matrice\n"); 
                    }
                    else{
                        pthread_mutex_lock(&mutex_client); 
                        msg->type = MSG_MATRICE; 
                        msg->data = NULL; 
                        msg->length = 0; 

                        invia_messaggio(socket_fd, msg);
                        
                        pthread_cond_wait(&cond_client, &mutex_client); 
                        pthread_mutex_unlock(&mutex_client); 
                    }
                }

                else if(strcmp(comando, "msg") == 0){
                    if(argomento == NULL){
                        printf("Comando non valido. Inserire un messaggio da postare sulla bacheca\n"); 
                    }
                    else{
                        msg->type = MSG_POST_BACHECA;
                        msg->data = argomento;
                        msg->length = strlen(msg->data);
                        
                        invia_messaggio(socket_fd, msg);
                        pthread_mutex_lock(&mutex_client);
                        printf("Attendo risposta da server\n"); 
                        pthread_cond_wait(&cond_client, &mutex_client);
                        printf("Svegliato\n"); 
                        pthread_mutex_unlock(&mutex_client); 
                    }
                }

                else if(strcmp(comando, "show_msg") == 0){
                    if(argomento != NULL){
                        printf("Comando non valido. Forse cercavi: show_msg\n");
                    }
                    else{
                        msg->type = MSG_SHOW_BACHECA;
                        msg->data = NULL; 
                        msg->length = 0; 

                        invia_messaggio(socket_fd, msg); 

                        pthread_mutex_lock(&mutex_client); 
                        pthread_cond_wait(&cond_client, &mutex_client);
                        pthread_mutex_unlock(&mutex_client); 
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

        close(client->socket_fd); 
        pthread_cancel(risposta_handler); 
        pthread_join(risposta_handler, NULL);
        if(msg_server){
            free(msg_server); 
        }
        free(client); 
        free(msg); 
        return 0;
    }
    
    void *server_handler(void* args){

        Client_t *client = (Client_t*)args; 

        int socket_fd = client->socket_fd; 
        while(1){

            int retvalue; 
            msg_server = (Messaggio*)malloc(sizeof(Messaggio)); 
            if(msg_server == NULL){
                perror("Errore nella malloc"); 
                exit(EXIT_FAILURE); 
            }

            //Inizializzo i valori default
            msg_server->data = NULL; 
            msg_server->type = '\0'; 
            msg_server->length = 0;


            SYSC(retvalue, read(socket_fd, &msg_server->length, sizeof(unsigned int)), "Nella read"); 
            if(retvalue == -1){
                pthread_mutex_lock(&mutex_client);
                if(exit_signal == 1){
                    pthread_mutex_unlock(&mutex_client); 
                    free(msg_server);
                    break; 
                }
                free(msg_server);
                break; 
            }
            SYSC(retvalue, read(socket_fd, &msg_server->type, sizeof(char)), "Nella read"); 
            //Alloco memoria per contenuto messaggio 
            msg_server->data = (char*)malloc(sizeof(char) * msg_server->length + 1); 
            if(msg_server->data == NULL){
                perror("Nella malloc"); 
                exit(EXIT_FAILURE); 
            }

            SYSC(retvalue, read(socket_fd, msg_server->data, msg_server->length), "Nella read");
             
            msg_server->data[msg_server->length] = '\0';  //Terminatore stringa

            pthread_mutex_lock(&mutex_running);
                if(exit_signal){ 
                    pthread_mutex_unlock(&mutex_running);
                    free(msg_server->data);
                    free(msg_server);
                    break; 
                }

            pthread_mutex_unlock(&mutex_running); 

            pthread_mutex_lock(&mutex_client); 

            switch(msg_server->type){

                case MSG_OK: 
                    if(!client->registrato){
                        client->registrato = 1; 
                    }
                    else{
                        printf("ARRIVATO ok\n"); 
                        pthread_cond_signal(&cond_client); 
                        break; 
                    }
                    printf("%s\n", msg_server->data); 
                    break; 
                
                case MSG_ERR:
                    printf("%s\n", msg_server->data); 
                    if(strcmp(msg_server->data, "Parita non ancora iniziata. Attendi.") == 0){
                        rimuovi_parole(client); 
                    }
                    pthread_cond_signal(&cond_client); 
                    break;

                case MSG_MATRICE: 
                    stringa_in_paroliere(msg_server->data, client);
                    stampa_matrice(client->paroliere_client); 
                    break;  

                case MSG_TEMPO_PARTITA:
                    printf("%s\n", msg_server->data); 
                    pthread_cond_signal(&cond_client);
                    break; 
                
                case MSG_TEMPO_ATTESA: 
                    printf("%s\n", msg_server->data); 
                    pthread_cond_signal(&cond_client);
                    break; 

                case MSG_PUNTI_PAROLA: 
                    printf("Punti parola: %s\n", msg_server->data); 
                    pthread_cond_signal(&cond_client); 
                    break; 

                case MSG_PUNTI_FINALI:
                    printf("\nClassifica finale:\n%s\n", msg_server->data);
                    rimuovi_parole(client); 
                    printf("Digita un comando qualsiasi per continuare\n");
                    break;
                
                case MSG_SHOW_BACHECA:
                    printf("*****BACHECA*****\n");
                    printf("%s", msg_server->data);
                    printf("*****************\n"); 
                    pthread_cond_signal(&cond_client); 
                    break; 

                case MSG_SERVER_SHUTDONW:

                    printf("%s\n", msg_server->data); 
                    printf("Digita un tasto qualsiasi per terminare-->"); 
                    fflush(stdout);
                    pthread_mutex_unlock(&mutex_client); 

                    pthread_mutex_lock(&mutex_running); 
                    exit_signal = 1; 
                    pthread_mutex_unlock(&mutex_running);
                    
                    free(msg_server->data);
                    free(msg_server); 

                    close(socket_fd);
                    pthread_exit(NULL); 
                    break; 

            }
            pthread_mutex_unlock(&mutex_client); 
            free(msg_server->data); 
            free(msg_server); 
        } 
        pthread_exit(NULL); 
    }