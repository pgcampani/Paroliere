# Il paroliere 

## 1. Introduzione

Il progetto consiste nello sviluppo del gioco **Il Paroliere** utilizzando un'architettura client-server in **C**, sfruttando le librerie **POSIX standard** per la gestione di thread, socket e segnali.

---

## 2. Strutture dati

### 2.1 Messaggi

La struct `Messaggio` gestisce la comunicazione client-server. 
Contiene:
- `type`: tipo del messaggio
- `length`: lunghezza del messaggio
- `data`: contenuto del messaggio 

### 2.2 Server

La struct `Server_data` raccoglie tutte le info lato server:
- `Paroliere`: matrice 4x4 generata casualmente o letta da file (`matrix.txt`)
- `Lista dei giocatori`: lista dinamica che permette inserimento/rimozione in tempo O(1)
- `Array dei punteggi`: accesso diretto per ordinare e stilare classifiche
- `Bacheca dei messaggi` coda circolare per messaggi tra client e server
- `Trie` struttura dati per ricerca efficiente nel vocabolario (tempo O(m), con m lunghezza parola)

### 2.3 ClientHandlerArgs

Struttura contenente i dati utilizzati dai thread che gestiscono i singoli client. 

### 2.4 Client

Ogni client mantiene:
- Stato di registrazione
- File descriptor del socket
- Matrice 4x4 per il paroliere
- Lista delle parole inviate (per evitare duplicati)

---

## 3. Struttura del Programma e Algoritmi 

### 3.1 Il Server

- **Main**: Inizializza il server, configura la gestione dei segnali e gestisce le connessioni con i client. Il ciclo di attesa termina con l'arrivo del segnale SIGINT, che attiva l'esecuzione della `cleanup` per una chiusura ordinata.
- **Client_thread**: Thread creato per ogni client. Registra il client nella lista dei giocatori e crea i thread `handler_messaggi` e `handler_punteggi` per gestire rispettivamente la comunicazione e il punteggio.
- **Handler_messaggi**: Responsabile della ricezione e gestione dei messaggi dal client.
- **Handler_punteggio**: Gestisce i punteggi di fine partita, sincronizzandosi con gli altri thread per aggiornare la classifica.
- **Thread Tempo**: Mantiene il tempo delle partite e gestisce la pausa tra una partita e l'altra, generando una nuova matrice alla fine di ogni partita.
- **Thread Scorer**: Ordinamento e stilatura della classifica tramite qsort.
- **Thread Timeout**: Monitora l'inattività dei giocatori ed espelle quelli inattivi tramite logout automatico.

### 3.2 Il Client

- **Main**: Inizializza il client, gestisce i segnali (`SIGINT` e `SIGUSR1`) e si connette al server. Crea un thread per la ricezione dei messaggi e si occupa dell'invio diretto dei comandi.
- **Risposta_handler**: Thread che riceve i messaggi dal server e sincronizza la comunicazione con il main, segnalando eventuali chiusure o errori.

### 3.3 Algoritmi Principali

- **DFS_rec**: Algoritmo che esplora la matrice in profondità per verificare la validità di una parola inviata dal client, esaminando tutte le possibili direzioni (ortogonali e diagonali).
- **Genera_matrice**: Funzione che gestisce la creazione della matrice; legge da un file se specificato o genera una matrice casuale in caso di file non disponibile o file terminato.

---

## 4. Organizzazione dei File

La struttura del progetto è la seguente: 

```plaintext

.
├── include
│   ├── macros.h
│   ├── trie.h
│   └── types.h
├── src
│   ├── funzioni.c
│   ├── server.c
│   └── trie.c
├── makefile
├── matrix.txt
├── dizionario_ita.txt
├── file_log.txt
└── README.md

```

## 5. Testing del programma 

### 5.1 Debugging con Valgrind 

Utilizzo di **Valgrind** per: 
- Verificare errori di accesso alla memoria.
- Assicurarsi che tutta la memoria venga liberata correttamente alla terminazione.

### 5.2 Test sulla Concorrenza

Simulazione del programma con più client connessi contemporaneamente per verificare la corretta sincronizzazione tra thread e la gestione delle risorse condivise.

Utilizzo di **Helgrind** per rilevare race condition. 

**Esempio di utilizzo**

```bash
valgrind --tool=helgrind ./paroliere_srv localhost 3500
```
In fase di compilazione sono stati usati i **sanitizer** di GCC per individuare errori legati ai thread. 

**Esempio di utilizzo** 

```bash
gcc -fsanitize=thread -g -o paroliere_srv server.c funzioni.c trie.c
```

### 5.3 Test su Segnali e Disconnessioni

Verifica del corretto funzionamento in caso di:
- Disconnessione volontaria (comandi `fine` e `cancella_registrazione`).
- Disconnessione per inattività.
- Chiusura del server.

### 5.4 Test delle Funzionalità del Gioco

- Verifica della corretta generazione e gestione della matrice, con particolare attenzione alla gestione della coppia "Qu".
- Test della validità delle parole presenti nella matrice e nel dizionario, con aggiornamento coerente del punteggio.

---

## 6. Compilazione del Codice

Utilizzo di **Makefile** per automatizzare la compilazione. 

Per compilare il progetto:
```bash
make
```
Per pulire i file oggetto e gli eseguibili
```bash
make clean
```

Il **Makefile** include opzioni di compilazione:
- -Wall per abilitare tutti gli avvisi
- -pthread per supporto thread
- -g per info sul debugging
- -Iinclude per includere la directory **include**

## 7. Esecuzione del codice

Per l'avvio del **Server**:

```plaintext
./paroliere_srv nome_server porta_server [--matrici data_filename] [--durata durata_in_minuti] [--seed rnd_seed] [--diz dizionario]
```
dove:
- paroliere_srv é il nome dell’eseguibile.
- nome_server é il nomer del server sul quale sarà fatto partire il server (ad esempio: 127.0.0.1 o localhost).
- porta_server é il numero della porta sulla quale far partire il processo server.
- il parametro opzionale --matrici `e seguito dal nome del file dal quale caricare le matrici.
- il parametro opzionale --durata permette di indicare la durata del gioco in minuti.
- il parametro opzionale --seed permette di indicare il seed da usare per la generazione dei numeri pseudocasuali.
- il parametro opzionale --diz permette di indicare il dizionario da usare per la verifica della leicità delle parole ricevute dal client.

Per l'avvio dei **Client**: 

```plaintext
./paroliere_cl nome_server porta_server
```

dove:
- paroliere_cl `e il nome dell’eseguibile;
- nome_server `e il nome del server al quale collegarsi;
- porta_server `e il numero della porta alla quale collegarsi;