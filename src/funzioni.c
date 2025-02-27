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

    pthread_mutex_lock(&server_data->mutex_server_data); 

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

    pthread_mutex_unlock(&server_data->mutex_server_data); 
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
    if(retvalue == -1 && errno == EINTR){
        printf("VOGLIONO CHIUDERE\n"); 
        free(msg);
        return NULL; 
    }

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

void inizializza_array_punteggi(Array_punteggi *arr){
    pthread_mutex_init(&arr->mutex_array, NULL);
    
    pthread_mutex_lock(&arr->mutex_array); 

    arr->counter_punteggi = 0; 

    pthread_mutex_unlock(&arr->mutex_array); 
}

void inizializza_bacheca(Bacheca *bacheca){

    pthread_mutex_init(&bacheca->mutex_bacheca, NULL); 

    pthread_mutex_lock(&bacheca->mutex_bacheca);
    bacheca->head = 0; 
    bacheca->tail = 0; 
    bacheca->count = 0; 

    pthread_mutex_unlock(&bacheca->mutex_bacheca); 
}

void inizializza_server_data(Server_data *server_data){

    pthread_mutex_init(&server_data->mutex_server_data, NULL); 
    pthread_mutex_init(&server_data->mutex_tempo, NULL); 

    inizializza_array_punteggi(server_data->array_punteggi);
    inizializza_bacheca(&server_data->bacheca); 

    pthread_cond_init(&server_data->cond_punteggi_pronti, NULL); 
    pthread_cond_init(&server_data->cond_classifica_pronta, NULL);
    pthread_cond_init(&server_data->inizio_partita, NULL); 
    pthread_cond_init(&server_data->fine_partita, NULL);

    pthread_mutex_lock(&server_data->mutex_server_data); 
    server_data->lista_giocatori = NULL; 
    server_data->count_giocatori = 0; 
    server_data->utenti_attivi = 0; 
    server_data->matrix_file = NULL; 
    server_data->partita_in_corso = 1; 
    server_data->root_trie = crea_nodo();
    server_data->terminazione_thread = 0;  
    if(server_data->root_trie == NULL){
        perror("Nella creazione nodo trie");
        exit(EXIT_FAILURE); 
    }
    server_data->classifica_pronta = 0; 
    server_data->classifica = (char *)malloc(MAX_BUFFER * sizeof(char)); 
    if(server_data->classifica == NULL){
        perror("Errore nella malloc");
        exit(EXIT_FAILURE); 
    }
    server_data->punteggi_pronti = 0; 
    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

void inizializza_log(Server_data *server_data, char *filename){
    server_data->log_file = fopen(filename, "w");
    if(server_data->log_file == NULL){
        perror("Errore apertura file log");
        exit(EXIT_FAILURE); 
    }
}

void log_event(Server_data* server_data, char* event, char* username, char* dettagli){
    pthread_mutex_lock(&server_data->mutex_log); 

    if(server_data->log_file){
        if(dettagli && dettagli[0] != '\0'){
            fprintf(server_data->log_file, "%s - %s - Dettagli: %s\n", event, username, dettagli); 
        }
        else{
            fprintf(server_data->log_file, "%s - %s\n", event, username); 
        }
    }

    pthread_mutex_unlock(&server_data->mutex_log); 
}

void inserisci_giocatore(Server_data* server_data, int socket){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *nuovo_giocatore = (Giocatore*)malloc(sizeof(Giocatore));
    if(nuovo_giocatore == NULL){
        perror("Errore nella malloc"); 
        pthread_mutex_unlock(&server_data->mutex_server_data); 
        return;
    }

    nuovo_giocatore->socket = socket;
    nuovo_giocatore->username[0] = '\0'; 
    nuovo_giocatore->connesso = 1; 
    nuovo_giocatore->in_gioco = 0; 
    nuovo_giocatore->score = 0; 
    nuovo_giocatore->tid = pthread_self(); 

    nuovo_giocatore->next = server_data->lista_giocatori; 

    server_data->lista_giocatori = nuovo_giocatore; 

    server_data->count_giocatori++; 
    server_data->utenti_attivi++; 

    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

void registra_giocatore(Server_data *server_data, int socket, char *username){
    
    pthread_mutex_lock(&server_data->mutex_server_data);

    Giocatore *temp = server_data->lista_giocatori;

    while(temp != NULL){
        if(temp->socket == socket){
            strncpy(temp->username, username, USERNAME_LENGTH - 1); 
            temp->username[USERNAME_LENGTH] = '\0'; 
            temp->in_gioco = 1; 
            pthread_mutex_unlock(&server_data->mutex_server_data);  
            return;
        }
        temp = temp->next; 
    }

    printf("Socket utente non trovato\n"); 

    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

int is_alphabetical(char *username){

    for(int i = 0; i < strlen(username); i++){
        char c = username[i];

        if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))){
            return 0; 
        }
    }
    return 1; 
}


int login(Server_data* server_data, char *username, int client_fd){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *curr = server_data->lista_giocatori; 
    Giocatore *prec = NULL; 
    Giocatore *esistente = NULL; 

    while(curr != NULL){
        if(strcmp(curr->username, username) == 0){

            if(curr->connesso == 1){
                //Utente già connesso, impossibile fare la login
                pthread_mutex_unlock(&server_data->mutex_server_data);
                return 0; 
            }
            else{
                esistente = curr; //Trovato utente registrato con quell'username
            }
        }
        if(curr->socket == client_fd && curr->username[0] == '\0'){
            prec = curr;  //Trovato nodo creato alla connessione con username non registrato
        }
        curr = curr->next; 
    }

    if(esistente != NULL){
        esistente->socket = client_fd;
        esistente->connesso = 1; 
        esistente->in_gioco = 1; 
        server_data->utenti_attivi++;
        //Se esiste un nodo temporaneo lo rimuoviamo
        if(prec != NULL){
            if(server_data->lista_giocatori == prec){
                server_data->lista_giocatori = prec->next; 
            }
            else{
                Giocatore *temp = server_data->lista_giocatori;
                while(temp != NULL && temp->next != prec){
                    temp = temp->next;
                }
                if(temp != NULL){
                    temp->next = prec->next; 
                }
            }
            server_data->count_giocatori--;
            server_data->utenti_attivi--; 
            free(prec); 
            prec = NULL; 
        }
 
        pthread_mutex_unlock(&server_data->mutex_server_data);
        return 1; 
    }

    pthread_mutex_unlock(&server_data->mutex_server_data); 
    return 0; 
}

void logout_utente(Server_data *server_data, int socket){
    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *curr = server_data->lista_giocatori;
    Giocatore *prev = NULL;  

    while(curr != NULL){
        if(curr->socket == socket){
            if(curr->username[0] == '\0'){
                if(prev == NULL){
                    server_data->lista_giocatori = curr->next; 
                }
                else{
                    prev->next = curr->next;
                }
                free(curr); 
            }
            else{
                curr->score = 0; 
                curr->connesso = 0; 
                curr->socket = -1; 
                curr->in_gioco = 0; 
                server_data->utenti_attivi--; 
            }
            pthread_mutex_unlock(&server_data->mutex_server_data);
            close(socket); 
            return; 
        }
        prev = curr; 
        curr = curr->next; 
    }
    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

void cancella_utente(Server_data* server_data, int socket){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *temp = server_data->lista_giocatori;
    Giocatore *prev = NULL; 

    while(temp != NULL && temp->socket != socket){
        prev = temp; 
        temp = temp->next; 
    }

    if(temp == NULL){
        printf("Giocatore non trovato\n"); 
        pthread_mutex_unlock(&server_data->mutex_server_data); 
        return; 
    }
    
    if(prev == NULL){
        //Rimuovo il primo nodo
        server_data->lista_giocatori = temp->next; 
    }
    else{
        prev->next = temp->next; 
    }

    server_data->utenti_attivi--; 
    server_data->count_giocatori--; 

    free(temp); 

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

Giocatore *restituisci_giocatore(Server_data* server_data, int socket_fd){
    
    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *curr = server_data->lista_giocatori;

    while(curr != NULL){
        if(curr->socket == socket_fd){
            pthread_mutex_unlock(&server_data->mutex_server_data); 
            return curr; 
        }
        curr = curr->next; 
    }

    pthread_mutex_unlock(&server_data->mutex_server_data); 

    return NULL; 
}

void stampa_lista_giocatori(Server_data* server_data){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    if(server_data->lista_giocatori == NULL){
        printf("Nessun giocatore connesso\n"); 
    }

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


int dfs_rec(char p[DIM_MATRIX][DIM_MATRIX], int righe, int colonne, char *parola, int index, int visited[DIM_MATRIX][DIM_MATRIX]){

    // Limiti paroliere
    if (righe < 0 || colonne < 0 || righe >= DIM_MATRIX || colonne >= DIM_MATRIX) {
        return 0;
    }

    //Cella già visitata o lettera non corrisponde
    if(visited[righe][colonne] || p[righe][colonne] != parola[index]){
        return 0; 
    }
    
    if(p[righe][colonne] == 'Q'){
        index++;
    }
    // Caso base - fine parola
    if (index == strlen(parola) - 1) {
        return 1;
    }

    visited[righe][colonne] = 1;

    // Esplora in tutte le direzioni ortogonali

    if(dfs_rec(p, righe - 1, colonne, parola, index + 1, visited)){
        return 1; // Alto
    }
    if(dfs_rec(p, righe + 1, colonne, parola, index + 1, visited)){
        return 1; // Basso
    }
    if(dfs_rec(p, righe, colonne - 1, parola, index + 1, visited)){
        return 1; ; // Sinistra
    }
    if (dfs_rec(p, righe, colonne + 1, parola, index + 1, visited)){
        return 1; //Destra
    } 
    if(dfs_rec(p, righe - 1, colonne - 1, parola, index + 1, visited)){
        return 1; //Diagonale sinistra alto
    } 
    if(dfs_rec(p, righe - 1, colonne + 1, parola, index + 1, visited)){
        return 1; //Diagonale destra alto
    }
    if(dfs_rec(p, righe + 1, colonne - 1, parola, index + 1, visited)){
        return 1; //Diagonale sinistra basso
    }
    if(dfs_rec(p, righe + 1, colonne + 1, parola, index + 1, visited)){
        return 1; //Diagonale destra basso; 
    }

    visited[righe][colonne] = 0; // Deselezione della cella

    return 0;
}

int parola_presente(char matrice[DIM_MATRIX][DIM_MATRIX], char *parola){
    int visited[DIM_MATRIX][DIM_MATRIX] = {0}; 

    for(int i = 0; i < DIM_MATRIX; i++){
        for(int j = 0; j < DIM_MATRIX; j++){
            if(dfs_rec(matrice, i, j, parola, 0, visited)){
                return 1; 
            }
        }
    }

    return 0;
}

void aggiorna_punti_giocatore(Server_data* server_data, int socket, char* parola){

    Giocatore *temp = server_data->lista_giocatori;

    while(temp != NULL){
        if(temp->socket == socket){
            temp->score += strlen(parola); 

            for(int i = 0; i < strlen(parola); i++){
                if(parola[i] == 'Q'){
                    temp->score--; 
                    i++; 
                }
            }
            return;  
        }
        temp = temp->next; 
    }
    return; 
}

void to_uppercase(char *str){
    while(*str){
        if(*str >= 'a' && *str <= 'z'){
            *str = *str - 'a' + 'A';
        }
        str++;
    }
}

int inserisci_punteggio(Array_punteggi *arr, char* username, int score){
    pthread_mutex_lock(&arr->mutex_array);

    if(arr->counter_punteggi < MAX_CLIENT){
        strncpy(arr->array[arr->counter_punteggi].nome, username, strlen(username)); 
        arr->array[arr->counter_punteggi].nome[strlen(username)] = '\0'; 
        arr->array[arr->counter_punteggi].punti = score; 
        arr->counter_punteggi++;  
        pthread_mutex_unlock(&arr->mutex_array); 
        return 1;
    }
    else{
        pthread_mutex_unlock(&arr->mutex_array); 
        return 0; 
    } 
}

void reset_punteggi(Server_data *server_data){

    pthread_mutex_lock(&server_data->mutex_server_data); 

    Giocatore *curr = server_data->lista_giocatori; 
        while(curr != NULL){
            curr->score = 0; 
            curr = curr->next;
        }

    pthread_mutex_unlock(&server_data->mutex_server_data); 
}

int post_bacheca(Bacheca *bacheca, char *username, char *post){
    pthread_mutex_lock(&bacheca->mutex_bacheca);

    if(bacheca == NULL || username == NULL || post == NULL){
        pthread_mutex_unlock(&bacheca->mutex_bacheca);
        return 0; 
    }

    strncpy(bacheca->post_bacheca[bacheca->tail].username, username, sizeof(bacheca->post_bacheca[bacheca->tail].username) - 1);
    bacheca->post_bacheca[bacheca->tail].username[sizeof(bacheca->post_bacheca[bacheca->tail].username) - 1] = '\0';

    strncpy(bacheca->post_bacheca[bacheca->tail].post, post, MAX_MSG_LEN);
    bacheca->post_bacheca[bacheca->tail].post[MAX_MSG_LEN] = '\0'; 

    bacheca->tail = (bacheca->tail + 1) % MAX_MESSAGES;
    
    if(bacheca->count < MAX_MESSAGES){
        bacheca->count++; 
    }
    else{
        //Buffer pieno, sovrascrivo
        bacheca->head = (bacheca->head + 1) % MAX_MESSAGES;
    }
    pthread_mutex_unlock(&bacheca->mutex_bacheca); 
    return 1; 
}

void show_bacheca(Bacheca *bacheca, char* output, size_t output_size){
    pthread_mutex_lock(&bacheca->mutex_bacheca);

    size_t offset = 0;  //Tiene traccia della posizione corrente nel buffer di output
    int index = bacheca->head;  //Mantiene l'indice del messaggio più vecchio nel buffer
    for(int i = 0; i < bacheca->count; i++){
        int written = snprintf(output + offset, output_size - offset, "%s,%s\n", bacheca->post_bacheca[index].username, bacheca->post_bacheca[index].post);

        if(written < 0 || (size_t)written >= output_size - offset){
            //Se non c'è spazio per scrivere interrompo il ciclo. Evito buffer overflow
            break;
        }
        offset += written; //Aggiorno offset con numero di caratteri scritti
        index = (index + 1) % MAX_MESSAGES; //Scorro buffer circolare
    }
    pthread_mutex_unlock(&bacheca->mutex_bacheca); 
}

//FUNZIONE DI CLEANUP
void cleanup(Server_data *server_data){

    //Chiusura file di log
    pthread_mutex_lock(&server_data->mutex_log);
    if(server_data->log_file){
        fflush(server_data->log_file);
        fclose(server_data->log_file);
    }
    pthread_mutex_unlock(&server_data->mutex_log);

    pthread_mutex_lock(&server_data->mutex_server_data);
    //Chiusura file matrici
    if(server_data->matrix_file){
        fclose(server_data->matrix_file); 
        server_data->matrix_file = NULL; 
    }

    Giocatore *curr = server_data->lista_giocatori; 
    //Pulizia lista_giocatori
    while(curr != NULL){

        Giocatore *rimuovi = curr; 
        curr = curr->next; 
        free(rimuovi); 
    }

    free_trie(server_data->root_trie); 
    
    free(server_data->classifica); 

    pthread_mutex_destroy(&server_data->mutex_log); 

    pthread_mutex_unlock(&server_data->mutex_server_data); 

    pthread_mutex_destroy(&server_data->mutex_server_data);

    pthread_mutex_destroy(&server_data->mutex_tempo); 

    pthread_mutex_destroy(&server_data->array_punteggi->mutex_array); 

    pthread_mutex_destroy(&server_data->bacheca.mutex_bacheca); 

    free(server_data); 
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

int inserisci_parola_in_lista(Client_t* client, char* word){

    Parola * temp = client->lista_parole; 

    while(temp != NULL){
        if(strcmp(word, temp->parola) == 0){
            return 0; 
        }
        temp = temp->next;
    }

    Parola * nuova_parola = (Parola*)malloc(sizeof(Parola)); 
    if(nuova_parola == NULL){
        perror("Nella malloc");
        exit(EXIT_FAILURE);
    }

    nuova_parola->parola = (char*)malloc(strlen(word) + 1);
    if(nuova_parola->parola == NULL){
        perror("Nella malloc");
        exit(EXIT_FAILURE); 
    }

    strcpy(nuova_parola->parola, word); 
    
    nuova_parola->next = client->lista_parole; 

    client->lista_parole = nuova_parola; 

    return 1; 
}

void rimuovi_parole(Client_t *client){
    Parola *curr = client->lista_parole; 

    while(curr != NULL){
        curr->parola = NULL; 
        curr = curr->next; 
    }

    client->lista_parole = NULL; 
}

int ordina_punteggi(const void* a, const void* b){
    const Punti_fine *p1 = (const Punti_fine*) a;
    const Punti_fine *p2 = (const Punti_fine*) b;
    return p2->punti - p1->punti; 
}