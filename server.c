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

    int running = 1; 
    pthread_mutex_t mutex_running = PTHREAD_MUTEX_INITIALIZER;

    void sigint_handler(int signum){
        if(signum == SIGINT){
            pthread_mutex_lock(&mutex_running);
            running = 0; 
            pthread_mutex_unlock(&mutex_running); 
        }
    }

    int main(int argc, char *argv[]){
        
        //Segnali
        struct sigaction sa; 
        sa.sa_handler = sigint_handler; 
        sigemptyset(&sa.sa_mask); 
        sa.sa_flags = 0; 
        if(sigaction(SIGINT, &sa, NULL) == -1){
            perror("Errore nella configurazione di sigaction");
            exit(EXIT_FAILURE); 
        }

        //Controllo parametri
        if(argc < 3){
            perror("Numero parametri errato!\nUsage:./paroliere_srv nome_server porta_server [--matrice data_filename] [--durata durata_in_minuti] [--seed rnd_seed] [--diz dizionario]\n");
            exit(EXIT_FAILURE); 
        }

        char *nome_server = argv[1];
        int porta_server = atoi(argv[2]); 

        //Default parametri opzionali
        char *data_filename = NULL; 
        int durata_in_minuti = 3; 
        unsigned int rnd_seed = 0; 
        char * diz = NULL; 

        //Controllo parametri opzionali 
        int opt, option_index = 0; 

        struct option long_options[] = {
            {"matrice", required_argument, 0, 'm'},
            {"durata", required_argument, 0, 'd'},
            {"seed", required_argument, 0, 's'},
            {"diz", required_argument, 0, 'z'},
            {0, 0, 0, 0}
        };

        while((opt = getopt_long(argc, argv, "m:d:s:z", long_options, &option_index)) != -1){
            switch (opt){
                case 'm': 
                    data_filename = optarg;
                    break; 
                
                case 'd':
                    durata_in_minuti = atoi(optarg);
                    break; 
                
                case 's':
                    rnd_seed = atoi(optarg);
                    break; 

                case 'z':
                    diz = optarg;
                    break; 
                
                default:
                    printf("Errore in %s\n", argv[0]);
                    exit(EXIT_FAILURE);     
            }
        }

        if(rnd_seed == 0){
            srand(time(NULL));
        }
        else{
            srand(rnd_seed);
        } 

        //Creazione del socket
        int server_fd, client_fd, retvalue; 
        struct sockaddr_in server_addr, client_addr;
        socklen_t client_addr_len = sizeof(client_addr); 

        server_addr.sin_family = AF_INET; 
        server_addr.sin_port = htons(porta_server); 
        server_addr.sin_addr.s_addr = INADDR_ANY; 

        //Socket
        SYSC(server_fd, socket(AF_INET, SOCK_STREAM, 0), "Nella socket"); 

        //Bind
        SYSC(retvalue, bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)), "Nella bind"); 

        //Listen 
        SYSC(retvalue, listen(server_fd, 0), "Nella listen"); 


        //Inizializzazione dati server
        Server_data *server_data = (Server_data*)malloc(sizeof(Server_data));
        if(server_data == NULL){
            perror("Nella malloc"); 
            exit(EXIT_FAILURE); 
        } 
        inizializza_server_data(server_data); 
        server_data->data_filename = data_filename; 
        server_data->durata_partita = durata_in_minuti;
        server_data->root_trie = load_file(server_data->root_trie, "dizionario_ita.txt");

        pthread_t thread_tempo, thread_scorer; 

        pthread_create(&thread_tempo, NULL, gestione_tempo_partita, (void*)server_data);
        //pthread_create(&thread_scorer, NULL, scorer, (void*)server_data);

        while(1){

            pthread_mutex_lock(&mutex_running);
            if(!running){
                pthread_mutex_unlock(&mutex_running);
                break;
            }
            pthread_mutex_unlock(&mutex_running);

            ClientHandlerArgs *args = (ClientHandlerArgs*)malloc(sizeof(ClientHandlerArgs)); 
            if(args == NULL){
                perror("Nella malloc");
                exit(EXIT_FAILURE); 
            }

            //Accept
            client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
            if(client_fd == -1){
                pthread_mutex_lock(&mutex_running); 
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);  
                    free(args);
                    break; 
                }
                pthread_mutex_unlock(&mutex_running); 
                perror("Nella accept"); 
                free(args); 
                continue; 
            }

            args->client_fd = client_fd; 
            args->server_data = server_data; 

            pthread_t client_thread; 

            SYSC(retvalue, pthread_create(&client_thread, NULL, client_handler, args), "Nella pthread_create");

        }

        pthread_join(thread_tempo, NULL);
        cleanup(server_data);
        pthread_mutex_lock(&mutex_running);
        pthread_mutex_unlock(&mutex_running); 
        pthread_mutex_destroy(&mutex_running); 
        close(server_fd);                
    }

    void *client_handler(void *args){

        ClientHandlerArgs *client_args = (ClientHandlerArgs*)args;
        int client_fd = client_args->client_fd;
        Server_data *server_data = client_args->server_data; 
        
        int retvalue; 

        inserisci_giocatore(server_data, client_fd); 

        Messaggio *msg, *risposta; 

        risposta = (Messaggio*)malloc(sizeof(Messaggio)); 
        if(risposta == NULL){
            perror("Nella malloc"); 
            close(client_fd);
            pthread_exit(NULL); 
        }
        risposta->data = NULL; 

        while(1){

            pthread_mutex_lock(&mutex_running);
            if(!running){
                pthread_mutex_unlock(&mutex_running); 
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&mutex_running); 

            msg = (Messaggio*)malloc(sizeof(Messaggio)); 
            if(msg == NULL){
                perror("Nella malloc"); 
                close(client_fd);
                pthread_exit(NULL); 
            }

            msg->data = NULL; 
            msg->type = '\0'; 
            msg->length = 0; 

            //Lettura messaggio da client
            SYSC(retvalue, read(client_fd, &msg->length, sizeof(unsigned int)), "Nella read"); 
            //Faccio un controllo sul valore di ritorno della read per controllare se il client ha chiuso la connessione
            if(retvalue == 0){
                cancella_utente(server_data, client_fd); 
                break; 
            }
            else if(retvalue == -1){
                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    pthread_exit(NULL); 
                }
                pthread_mutex_unlock(&mutex_running); 
            }
            SYSC(retvalue, read(client_fd, &msg->type, sizeof(char)), "Nella read"); 
            if(retvalue == 0){
                cancella_utente(server_data, client_fd); 
                break; 
            }
            else if(retvalue == -1){
                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    pthread_exit(NULL); 
                }
                pthread_mutex_unlock(&mutex_running); 
            }
            
            if(msg->length > 0){
                //La lunghezza è maggiore di 0. Alloco memoria per il contenuto del messaggio
                msg->data = (char*)malloc(sizeof(char) * msg->length + 1);
                if(msg->data == NULL){
                    perror("Nella malloc");  
                    break; 
                }

                SYSC(retvalue, read(client_fd, msg->data, msg->length), "Errore nella read");
                if(retvalue == 0){
                    cancella_utente(server_data, client_fd);
                    break; 
                }
                else if(retvalue == -1){
                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    pthread_exit(NULL); 
                }
                pthread_mutex_unlock(&mutex_running); 
            }

                msg->data[msg->length] = '\0'; 
            }

            switch(msg->type){

                case MSG_REGISTRA_UTENTE: 

                    if(msg->length == 0 || msg->length > 10){
                        risposta->type = MSG_ERR; 
                        risposta->data = "La lunghezza dell'username deve essere compresa tra 1 e 10 caratteri";
                        risposta->length = strlen(risposta->data); 
                        invia_messaggio(client_fd, risposta); 
                        break; 
                    }

                    if(cerca_giocatore(server_data, msg->data)){
                        risposta->type = MSG_ERR; 
                        risposta->data = "Username già occupato"; 
                        risposta->length = strlen(risposta->data);
                        invia_messaggio(client_fd, risposta); 
                        break; 
                    }

                    pthread_mutex_lock(&server_data->mutex_server_data); 

                    if(server_data->count_giocatori > MAX_CLIENT){  
                        risposta->type = MSG_ERR; 
                        risposta->data = "Numero massimo giocatori raggiunto. Riprova più tardi";
                        risposta->length = strlen(risposta->data); 
                        invia_messaggio(client_fd, risposta); 
                        pthread_mutex_unlock(&server_data->mutex_server_data); 
                        break;
                    }

                    pthread_mutex_unlock(&server_data->mutex_server_data); 
                    
                    registra_giocatore(server_data, client_fd, msg->data);

                    risposta->type = MSG_OK;
                    risposta->data = "Registrazione avvenuta con successo"; 
                    risposta->length = strlen(risposta->data); 

                    invia_messaggio(client_fd, risposta); 

                    risposta->type = MSG_MATRICE; 
                    pthread_mutex_lock(&server_data->mutex_server_data);
                    risposta->data = paroliere_in_stringa(server_data->paroliere); 
                    pthread_mutex_unlock(&server_data->mutex_server_data); 
                    risposta->length = strlen(risposta->data); 
                    invia_messaggio(client_fd, risposta); 
                    free(risposta->data); 

                    char stringa_tempo[MAX_BUFFER]; 
                    pthread_mutex_lock(&server_data->mutex_tempo); 
                    if(server_data->partita_in_corso){
                        risposta->type = MSG_TEMPO_PARTITA;
                        snprintf(stringa_tempo, sizeof(stringa_tempo), "Tempo fine partita %d\n", server_data->timer);
                    }
                    else{
                        risposta->type = MSG_TEMPO_ATTESA; 
                        snprintf(stringa_tempo, sizeof(stringa_tempo), "Tempo a inizio partita %d\n", server_data->timer);  
                    }
                    risposta->data = stringa_tempo; 
                    risposta->length = strlen(stringa_tempo); 
                    pthread_mutex_unlock(&server_data->mutex_tempo); 

                    invia_messaggio(client_fd, risposta);

                    break; 
                
                case MSG_PAROLA: 

                    pthread_mutex_lock(&server_data->mutex_server_data);

                    to_uppercase(msg->data);

                    pthread_mutex_lock(&server_data->mutex_tempo);

                    if(server_data->partita_in_corso){
                        pthread_mutex_unlock(&server_data->mutex_tempo);

                        int parola_corretta; 

                        printf("parola inviata: %s\n", msg->data); 
                        parola_corretta = parola_presente(server_data->paroliere, msg->data);

                        if(parola_corretta && search_word(server_data->root_trie, msg->data)){
                            
                            aggiorna_punti_giocatore(server_data, client_fd, msg->data); 
                            char punteggio[MAX_BUFFER];
                            int messaggio_punti = strlen(msg->data);
        
                            risposta->type = MSG_PUNTI_PAROLA;
                            
                            //Controllo la presenza di Q, in tal caso diminuisco di 1 il valore del punteggio da stampare -- Q esiste solo con Qu ma vale 1 
                            for(int i = 0; i < strlen(msg->data); i++){
                                if(msg->data[i] == 'Q'){
                                    messaggio_punti--; 
                                    i++; 
                                }
                            }

                            snprintf(punteggio, sizeof(punteggio), "Punteggio parola: %d", messaggio_punti); 
                            risposta->data = punteggio;
                            risposta->length = strlen(risposta->data); 
                            invia_messaggio(client_fd, risposta); 
                        }
                        else{
                            risposta->type = MSG_ERR;
                            risposta->data = "Parola non corretta";
                            risposta->length = strlen(risposta->data);

                            invia_messaggio(client_fd, risposta);
                        }
                        pthread_mutex_unlock(&server_data->mutex_server_data); 
                    }
                    else{
                        pthread_mutex_unlock(&server_data->mutex_tempo);
                        
                        risposta->type = MSG_ERR; 
                        risposta->data = "Parita non ancora iniziata. Attendi."; 
                        risposta->length = strlen(risposta->data); 

                        invia_messaggio(client_fd, risposta); 

                        pthread_mutex_unlock(&server_data->mutex_server_data); 
                    }
                    break; 

                case MSG_MATRICE:

                    risposta->type = MSG_MATRICE; 
                    pthread_mutex_lock(&server_data->mutex_server_data);
                    risposta->data = paroliere_in_stringa(server_data->paroliere); 
                    pthread_mutex_unlock(&server_data->mutex_server_data); 
                    risposta->length = strlen(risposta->data); 
                    invia_messaggio(client_fd, risposta); 
                    free(risposta->data); 

                    pthread_mutex_lock(&server_data->mutex_tempo); 
                    if(server_data->partita_in_corso){
                        risposta->type = MSG_TEMPO_PARTITA;
                        snprintf(stringa_tempo, sizeof(stringa_tempo), "Tempo fine partita %d\n", server_data->timer);
                    }
                    else{
                        risposta->type = MSG_TEMPO_ATTESA; 
                        snprintf(stringa_tempo, sizeof(stringa_tempo), "Tempo a inizio partita %d\n", server_data->timer);  
                    }
                    risposta->data = stringa_tempo; 
                    risposta->length = strlen(stringa_tempo); 
                    pthread_mutex_unlock(&server_data->mutex_tempo); 

                    invia_messaggio(client_fd, risposta);

                    break;

                default: 
                    risposta->type = MSG_ERR; 
                    risposta->data = "Comando non riconosciuto"; 
                    risposta->length = strlen(risposta->data); 
                    invia_messaggio(client_fd, risposta); 
                    break; 
            } 

            free(msg->data);
            free(msg); 
            msg = NULL;

        }
        
        free(msg->data);
        free(msg); 
        
        free(risposta); 

        close(client_fd); 
        pthread_exit(NULL); 
    }

void *gestione_tempo_partita(void* args){
    Server_data *server_data = (Server_data*)args; 
    int timer;  

    while(1){

        pthread_mutex_lock(&server_data->mutex_server_data); 

        pthread_mutex_lock(&server_data->mutex_tempo);

        if(server_data->partita_in_corso){
            server_data->partita_in_corso = 0;

            pthread_mutex_unlock(&server_data->mutex_tempo); 

            pthread_mutex_unlock(&server_data->mutex_server_data); 

            pthread_mutex_lock(&server_data->mutex_tempo);
            server_data->timer = 10;  
            timer = server_data->timer; 
            pthread_mutex_unlock(&server_data->mutex_tempo); 
            genera_matrice(server_data); 
            stampa_matrice(server_data->paroliere); 
        }
        else{
            server_data->prima_partita = 0; 
            server_data->partita_in_corso = 1; 
            server_data->timer = server_data->durata_partita * 60;
            timer = server_data->timer; 
            pthread_mutex_unlock(&server_data->mutex_tempo); 
            pthread_mutex_unlock(&server_data->mutex_server_data);
        }

        while(timer){

            pthread_mutex_lock(&mutex_running);

            if(running){
                pthread_mutex_unlock(&mutex_running); 
                pthread_mutex_lock(&server_data->mutex_tempo); 
                server_data->timer--;
                pthread_mutex_unlock(&server_data->mutex_tempo); 
                timer--;
                sleep(1);
            }
            else{
                pthread_mutex_unlock(&mutex_running); 
                pthread_exit(NULL); 
            }
        }
    }
}

