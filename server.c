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

    pthread_mutex_t mutex_array_giocatori = PTHREAD_MUTEX_INITIALIZER; 
    pthread_mutex_t mutex_paroliere = PTHREAD_MUTEX_INITIALIZER; 
    pthread_mutex_t mutex_counter_giocatori = PTHREAD_MUTEX_INITIALIZER; 

    int main(int argc, char *argv[]){

        //Controllo parametri
        if(argc < 3){
            perror("Numero parametri errato!\nUsage:./paroliere_srv nome_server porta_server [--matrici data_filename] [--durata durata_in_minuti] [--seed rnd_seed] [--diz dizionario]\n");
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
        Server_data server_data; 
        server_data.count_giocatori = 0; 
        server_data.thread_id = 0; 
        server_data.data_filename = data_filename; 
        server_data.matrix_file = NULL; 

        genera_matrice(&server_data); 

        stampa_matrice(server_data); 

        while(1){
            //Accept
            SYSC(client_fd, accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len), "Nella accept"); 

            ClientHandlerArgs *args = (ClientHandlerArgs*)malloc(sizeof(ClientHandlerArgs)); 
            if(args == NULL){
                perror("Nella malloc"); 
                exit(EXIT_FAILURE); 
            }

            args->client_fd = client_fd; 
            args->server_data = server_data; 

            pthread_t client_tid; 

            SYSC(retvalue, pthread_create(&client_tid, NULL, client_handler, args), "Nella pthread_create");

            SYSC(retvalue, pthread_detach(client_tid), "Nella detach");     //Libera risorse automaticamente 
        }

    }

    void *client_handler(void *args){

        ClientHandlerArgs *client_args = (ClientHandlerArgs*)args;
        int client_fd = client_args->client_fd;
        Server_data *server_data = &client_args->server_data; 
        
        int retvalue; 

        Messaggio *msg, *risposta; 

        risposta = (Messaggio*)malloc(sizeof(Messaggio)); 
        if(risposta == NULL){
            perror("Nella malloc"); 
            close(client_fd);
            pthread_exit(NULL); 
        }

        while(1){
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
                printf("Il client ha chiuso al connessione\n"); 
                break; 
            }

            SYSC(retvalue, read(client_fd, &msg->type, sizeof(char)), "Nella read"); 
            if(retvalue == 0){
                printf("Il client ha chiuso la connessione\n");
                break; 
            }

            if(msg->length > 0){
                //La lunghezza è maggiore di 0. Alloco memoria per il contenuto del messaggio
                msg->data = (char*)malloc(sizeof(char) * msg->length + 1);
                if(msg->data == NULL){
                    perror("Nella malloc"); 
                    close(client_fd); 
                    pthread_exit(NULL); 
                }

                SYSC(retvalue, read(client_fd, msg->data, msg->length), "Errore nella read");
                if(retvalue == 0){
                    printf("Il client ha chiuso la connessione");
                    break; 
                }

                msg->data[msg->length] = '\0'; 
            }

            switch(msg->type){
                case MSG_REGISTRA_UTENTE: 
                    if(msg->length == 0 || msg->length > 10){
                        risposta->type = MSG_ERR; 
                        risposta->data = "La lunghezza dell'username deve essere compresa tra 1 e 10 caratteri\n";
                        risposta->length = strlen(risposta->data); 
                        invia_messaggio(client_fd, risposta); 
                        continue; 
                    }

                    pthread_mutex_lock(&mutex_array_giocatori); 

                    if(username_occupato(server_data, msg->data) == 1){
                        risposta->type = MSG_ERR; 
                        risposta->data = "Username già occupato\n"; 
                        risposta->length = strlen(risposta->data);
                        invia_messaggio(client_fd, risposta); 
                        pthread_mutex_unlock(&mutex_array_giocatori);
                        continue;
                    }

                    pthread_mutex_unlock(&mutex_array_giocatori);

                    pthread_mutex_lock(&mutex_counter_giocatori); 

                    if(server_data->count_giocatori > MAX_CLIENT){  
                        risposta->type = MSG_ERR; 
                        risposta->data = "Numero massimo giocatori raggiunto. Riprova più tardi\n";
                        risposta->length = strlen(risposta->data); 
                        invia_messaggio(client_fd, risposta); 
                        pthread_mutex_unlock(&mutex_counter_giocatori); 
                        close(client_fd); 
                        free(msg->data); 
                        free(msg); 
                        pthread_exit(NULL);
                    }

                    pthread_mutex_unlock(&mutex_counter_giocatori); 

                    //inserisci_utente(server_data, client_fd, msg->data);
                    //server_data->count_giocatori++; 

                    risposta->type = MSG_OK;
                    risposta->data = "Registrazione avvenuta con successo\n"; 
                    risposta->length = strlen(risposta->data); 
                    invia_messaggio(client_fd, risposta); 

                    free(msg->data); 
                    break; 

                default: 
                    risposta->type = MSG_ERR; 
                    risposta->data = "Comando non riconosciuto\n"; 
                    risposta->length = strlen(risposta->data); 
                    invia_messaggio(client_fd, risposta); 
                    free(msg->data); 
                    break; 
            }
            free(msg->data); 
            free(msg); 
        }

        close(client_fd); 
        free(risposta); 
        pthread_exit(NULL); 
        
    }