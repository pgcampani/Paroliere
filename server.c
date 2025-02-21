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

    int active_threads = 0;

    pthread_mutex_t active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

    Server_data *global_server_data = NULL;
    
    void increment_active_threads() {
        pthread_mutex_lock(&active_threads_mutex);
        active_threads++;
        pthread_mutex_unlock(&active_threads_mutex);
    }
    
    void decrement_active_threads() {
        pthread_mutex_lock(&active_threads_mutex);
        active_threads--;
        pthread_mutex_unlock(&active_threads_mutex);
    }
    
    int global_server_fd = -1; 
    
    void sigint_handler(int signum){

        //invia_shtudown
        Messaggio msg; 
        msg.type = MSG_SERVER_SHUTDONW; 
        msg.data = "Chiusura server"; 
        msg.length = strlen(msg.data);

        if(signum == SIGINT){
            pthread_mutex_lock(&mutex_running);
            running = 0; 
            pthread_mutex_unlock(&mutex_running); 

            pthread_mutex_lock(&global_server_data->mutex_tempo);
            global_server_data->partita_in_corso = 0;
            
            pthread_cond_broadcast(&global_server_data->inizio_partita);
            pthread_cond_broadcast(&global_server_data->fine_partita);

            pthread_mutex_unlock(&global_server_data->mutex_tempo); 

            pthread_mutex_lock(&global_server_data->mutex_server_data);

            pthread_cond_broadcast(&global_server_data->cond_punteggi_pronti);
            pthread_cond_broadcast(&global_server_data->cond_classifica_pronta); 

            pthread_mutex_unlock(&global_server_data->mutex_server_data); 

            shutdown(global_server_fd, SHUT_RDWR); 

            if(global_server_fd != -1){
                close(global_server_fd); 
            }

            pthread_mutex_lock(&global_server_data->mutex_server_data);

            Giocatore *curr = global_server_data->lista_giocatori; 

            while(curr != NULL){

                invia_messaggio(curr->socket, &msg);

                if(close(curr->socket) == -1){
                    perror("Errore in chiusura socket"); 
                } 

                curr = curr->next;
            }
            pthread_mutex_unlock(&global_server_data->mutex_server_data); 
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

        global_server_fd = server_fd;

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

        global_server_data = server_data; 

        pthread_t thread_tempo, thread_scorer; 

        pthread_create(&thread_tempo, NULL, gestione_tempo_partita, (void*)server_data);
        increment_active_threads();
        pthread_create(&thread_scorer, NULL, scorer, (void*)server_data);
        increment_active_threads();

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
                
                pthread_cancel(thread_tempo); 

                perror("Nella accept"); 
                free(args); 
                continue; 
            }

            args->client_fd = client_fd; 
            args->server_data = server_data; 

            pthread_t client_thread; 
            increment_active_threads();
            SYSC(retvalue, pthread_create(&client_thread, NULL, client_handler, (void*)args), "Nella pthread_create");

            pthread_detach(client_thread);   
        }

        pthread_join(thread_scorer, NULL); 

        pthread_join(thread_tempo, NULL);

        pthread_mutex_lock(&server_data->mutex_tempo);
        pthread_cond_broadcast(&server_data->fine_partita); 
        pthread_mutex_unlock(&server_data->mutex_tempo); 

        cleanup(server_data);

        printf("THREAD ATTIVI: %d\n", active_threads); 
        
        pthread_mutex_destroy(&mutex_running); 

        return 0; 
    }

    void *client_handler(void *args){

        ClientHandlerArgs *client_args = (ClientHandlerArgs*)args; 

        int client_fd = client_args->client_fd;
        Server_data *server_data = client_args->server_data;
        int retvalue; 
        
        inserisci_giocatore(server_data, client_fd); 
    
        SYSC(retvalue, pthread_create(&client_args->messaggi_tid, NULL, handler_messaggi, (void*)client_args), "Nella pthread_create");
        increment_active_threads(); 
        SYSC(retvalue, pthread_create(&client_args->punti_tid, NULL, handler_punteggio, (void*)client_args), "Nella pthread_create");
        increment_active_threads(); 

        pthread_join(client_args->messaggi_tid, NULL);
        pthread_join(client_args->punti_tid, NULL); 

        free(client_args); 
        decrement_active_threads();
        pthread_exit(NULL); 
    }

    void *handler_messaggi(void *args){

        ClientHandlerArgs *client_args = (ClientHandlerArgs*)args; 

        int client_fd = client_args->client_fd;
        Server_data *server_data = client_args->server_data;
        int retvalue; 

        Messaggio *msg, *risposta; 

        risposta = (Messaggio*)malloc(sizeof(Messaggio)); 
        if(risposta == NULL){
            perror("Nella malloc"); 
            close(client_fd);
            pthread_exit(NULL); 
        }
        risposta->type = '\0';
        risposta->length = 0; 
        risposta->data = NULL; 


        while(1){

            pthread_mutex_lock(&mutex_running);
            if(running == 0){
                pthread_mutex_unlock(&mutex_running); 
                decrement_active_threads();
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

                pthread_mutex_lock(&server_data->mutex_server_data);
                server_data->count_giocatori--; 
                server_data->terminazione_thread = 1; 
                pthread_mutex_unlock(&server_data->mutex_server_data);

                pthread_mutex_lock(&server_data->mutex_tempo);
                pthread_cond_broadcast(&server_data->fine_partita); 
                pthread_mutex_unlock(&server_data->mutex_tempo); 

                shutdown(client_fd, SHUT_RDWR); 

                break; 
            }
            else if(retvalue == -1){
                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    free(risposta);
                    free(msg->data); 
                    free(msg);
                    decrement_active_threads(); 
                    pthread_exit(NULL); 
                }
                else{
                    pthread_mutex_unlock(&mutex_running); 
                }
            }

            SYSC(retvalue, read(client_fd, &msg->type, sizeof(char)), "Nella read"); 
            
            if(msg->length > 0){
                //La lunghezza è maggiore di 0. Alloco memoria per il contenuto del messaggio
                msg->data = (char*)malloc(sizeof(char) * msg->length + 1);
                if(msg->data == NULL){
                    perror("Nella malloc");  
                    break; 
                }

                SYSC(retvalue, read(client_fd, msg->data, msg->length), "Errore nella read");


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

                    pthread_mutex_unlock(&server_data->mutex_server_data); 

                    pthread_mutex_lock(&server_data->mutex_tempo);

                    if(server_data->partita_in_corso){
                        
                        pthread_mutex_unlock(&server_data->mutex_tempo); 

                        int parola_corretta; 

                        printf("parola inviata: %s\n", msg->data); 

                        pthread_mutex_lock(&server_data->mutex_server_data); 

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
                
                    case MSG_LOGIN_UTENTE: 
                        int log = 0; 
                        log = login(server_data, msg->data, client_fd); 
                        if(log){

                            risposta->type = MSG_OK;
                            risposta->data = "Bentornato\n"; 
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
                        }
                        else{
                            risposta->type = MSG_ERR; 
                            risposta->data = "Errore login. L'utente non è registrato.\nPer registrarsi utilizzare comando registra_utente\n";
                            risposta->length = strlen(risposta->data); 
                        }
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
        
        free(risposta); 
        decrement_active_threads(); 
        pthread_exit(NULL);
    }

    void *handler_punteggio(void *args){
        ClientHandlerArgs * client_args = (ClientHandlerArgs*)args; 
        int client_fd = client_args->client_fd;
        Server_data *server_data = client_args->server_data;

        
        while(1){

            pthread_mutex_lock(&server_data->mutex_tempo); 

            while(server_data->partita_in_corso){
                pthread_cond_wait(&server_data->fine_partita, &server_data->mutex_tempo); 
                
                pthread_mutex_lock(&server_data->mutex_server_data);
                if(server_data->terminazione_thread){
                    server_data->terminazione_thread = 0; 
                    pthread_mutex_unlock(&server_data->mutex_server_data);
                    pthread_mutex_unlock(&server_data->mutex_tempo); 
                    decrement_active_threads();
                    pthread_exit(NULL); 
                }

                pthread_mutex_unlock(&server_data->mutex_server_data);


                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    pthread_mutex_unlock(&server_data->mutex_tempo);  
                    decrement_active_threads(); 
                    pthread_exit(NULL);
                }
                pthread_mutex_unlock(&mutex_running);
            }
            pthread_mutex_unlock(&server_data->mutex_tempo);

            pthread_mutex_lock(&mutex_running);
            if(running == 0){
                pthread_mutex_unlock(&mutex_running); 
                decrement_active_threads(); 
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&mutex_running);

            Giocatore *giocatore = restituisci_giocatore(server_data, client_fd); 
            if(giocatore != NULL){
                int punteggio_inserito = 0; 
                if(giocatore->score > 0){
                    punteggio_inserito = inserisci_punteggio(server_data->array_punteggi, giocatore->username, giocatore->score); 
                }       

                if(punteggio_inserito){
                    pthread_mutex_lock(&server_data->mutex_server_data);
                    if(server_data->array_punteggi->counter_punteggi == server_data->count_giocatori){
                        server_data->punteggi_pronti = 1; 
                        pthread_cond_signal(&server_data->cond_punteggi_pronti);
                    }

                    while(!server_data->classifica_pronta){
                        pthread_cond_wait(&server_data->cond_classifica_pronta, &server_data->mutex_server_data);

                        if(server_data->terminazione_thread){
                            server_data->terminazione_thread = 0;
                            pthread_mutex_unlock(&server_data->mutex_server_data);
                            decrement_active_threads();
                            pthread_exit(NULL); 
                        }        
        
                        pthread_mutex_lock(&mutex_running);
                        if(running == 0){
                            pthread_mutex_unlock(&mutex_running);
                            pthread_mutex_unlock(&server_data->mutex_server_data);  
                            decrement_active_threads(); 
                            pthread_exit(NULL);
                        }
                        pthread_mutex_unlock(&mutex_running);
                    }

                    Messaggio *msg_punti = (Messaggio*)malloc(sizeof(Messaggio));
                    if(msg_punti == NULL){
                        perror("Nella malloc");
                        pthread_mutex_unlock(&server_data->mutex_server_data); 
                        decrement_active_threads(); 
                        pthread_exit(NULL);
                    }
                    msg_punti->type = MSG_PUNTI_FINALI;
                    msg_punti->data = server_data->classifica;  
                    msg_punti->length = strlen(msg_punti->data);
                    invia_messaggio(client_fd, msg_punti);  
                    free(msg_punti);
                    //pthread_cond_signal(&server_data->cond_classifica_pronta); 
                    server_data->classifica_pronta = 0; 
                    pthread_mutex_unlock(&server_data->mutex_server_data);
                }
            }

            pthread_mutex_lock(&server_data->mutex_tempo);
            while(!server_data->partita_in_corso){
                pthread_cond_wait(&server_data->inizio_partita, &server_data->mutex_tempo);

                pthread_mutex_lock(&server_data->mutex_server_data);
                if(server_data->terminazione_thread){
                    server_data->terminazione_thread = 0; 
                    pthread_mutex_unlock(&server_data->mutex_server_data);
                    pthread_mutex_unlock(&server_data->mutex_tempo); 
                    decrement_active_threads();
                    pthread_exit(NULL);
                }
                pthread_mutex_unlock(&server_data->mutex_server_data); 
\

                pthread_mutex_lock(&mutex_running);
                if(running == 0){
                    pthread_mutex_unlock(&mutex_running);
                    pthread_mutex_unlock(&server_data->mutex_tempo); 
                    decrement_active_threads();
                    pthread_exit(NULL);
                }
                pthread_mutex_unlock(&mutex_running); 
            }
            pthread_mutex_unlock(&server_data->mutex_tempo); 
        }

        decrement_active_threads();
        pthread_exit(NULL); 
    }

    void *gestione_tempo_partita(void* args){
        Server_data *server_data = (Server_data*)args; 
        int timer;  

        while(1){

            pthread_mutex_lock(&mutex_running);

            if(!running){
                pthread_mutex_unlock(&mutex_running); 
                decrement_active_threads();
                pthread_exit(NULL); 
            }

            pthread_mutex_unlock(&mutex_running);

            pthread_mutex_lock(&server_data->mutex_tempo);

            if(server_data->partita_in_corso){
                pthread_mutex_lock(&mutex_running);

                if(!running){
                    pthread_mutex_unlock(&mutex_running); 
                    decrement_active_threads();
                    pthread_exit(NULL); 
                }
    
                pthread_mutex_unlock(&mutex_running);
                server_data->partita_in_corso = 0;

                pthread_cond_broadcast(&server_data->fine_partita);

                server_data->timer = 10;  
                timer = server_data->timer; 

                pthread_mutex_unlock(&server_data->mutex_tempo); 
                genera_matrice(server_data);
                pthread_mutex_lock(&server_data->mutex_server_data);
                stampa_matrice(server_data->paroliere); 
                pthread_mutex_unlock(&server_data->mutex_server_data);
            }
            else{
                pthread_mutex_lock(&mutex_running);

                if(!running){
                    pthread_mutex_unlock(&mutex_running); 
                    decrement_active_threads();
                    pthread_exit(NULL); 
                }
    
                pthread_mutex_unlock(&mutex_running);

                server_data->partita_in_corso = 1; 

                pthread_cond_broadcast(&server_data->inizio_partita);

                server_data->timer = server_data->durata_partita * 60;
                timer = server_data->timer; 
                pthread_mutex_unlock(&server_data->mutex_tempo); 
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
                    pthread_testcancel();
                }
                else{
                    pthread_mutex_unlock(&mutex_running); 
                    decrement_active_threads(); 
                    pthread_exit(NULL); 
                }
            }
        }
    }

void *scorer(void* args){

    Server_data* server_data = (Server_data*)args; 
    char csv[MAX_BUFFER];

    while(1){

        pthread_mutex_lock(&server_data->mutex_server_data);

        while(!server_data->punteggi_pronti){
            pthread_cond_wait(&server_data->cond_punteggi_pronti, &server_data->mutex_server_data);

            pthread_mutex_lock(&mutex_running);
            int should_exit = !running;
            pthread_mutex_unlock(&mutex_running); 
            
            if(should_exit){
                pthread_mutex_unlock(&server_data->mutex_server_data);
                decrement_active_threads(); 
                pthread_exit(NULL); 
            }
        }

        pthread_mutex_lock(&mutex_running);
        int should_exit = !running;
        pthread_mutex_unlock(&mutex_running); 
        
        if(should_exit){
            pthread_mutex_unlock(&server_data->mutex_server_data);
            decrement_active_threads(); 
            pthread_exit(NULL); 
        }

        pthread_mutex_unlock(&server_data->mutex_server_data); 

        pthread_mutex_lock(&server_data->array_punteggi->mutex_array);

        int n = server_data->array_punteggi->counter_punteggi;
        Punti_fine temp_buffer[MAX_CLIENT];
        for(int i = 0; i < n; i++){
            temp_buffer[i] = server_data->array_punteggi->array[i];
        }

        pthread_mutex_unlock(&server_data->array_punteggi->mutex_array);

        qsort(temp_buffer, n, sizeof(Punti_fine), ordina_punteggi); 

        csv[0] = '\0';
        for(int i = 0; i < server_data->array_punteggi->counter_punteggi; i++){
            char line[LINE_SIZE];
            snprintf(line, sizeof(line), "%s,%d\n", temp_buffer[i].nome, temp_buffer[i].punti);
            strncat(csv, line, sizeof(csv) - strlen(csv) - 1);
        }

        pthread_mutex_lock(&server_data->mutex_server_data); 

        strncpy(server_data->classifica, csv, MAX_BUFFER - 1);
        server_data->classifica[MAX_BUFFER - 1] = '\0';

        
        server_data->classifica_pronta = 1;

        pthread_cond_broadcast(&server_data->cond_classifica_pronta);

        server_data->punteggi_pronti = 0; 

        Giocatore *curr = server_data->lista_giocatori; 
        while(curr != NULL){
            curr->score = 0; 
            curr = curr->next;
        }
        pthread_mutex_unlock(&server_data->mutex_server_data);

        pthread_mutex_lock(&server_data->array_punteggi->mutex_array); 
        for(int i = 0; i < MAX_CLIENT; i++){
            server_data->array_punteggi->array[i].punti = 0; 
            server_data->array_punteggi->array[i].nome[0] = '\0'; 
            server_data->array_punteggi->counter_punteggi = 0; 
        }
        pthread_mutex_unlock(&server_data->array_punteggi->mutex_array); 
    }
    decrement_active_threads();
    pthread_exit(NULL); 
}