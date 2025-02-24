#define ALPHABET_SIZE 26

//Struttura per trie
typedef struct TrieNode{
    struct TrieNode * children[ALPHABET_SIZE];
    int is_end_of_word; 
}TrieNode; 
 
TrieNode *crea_nodo(); 

void insert_word(TrieNode* , char*);

int search_word(TrieNode*, char*);

TrieNode *load_file(TrieNode *, char *);

void free_trie(TrieNode*);

void to_uppercase(char *);