#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "types.h"

#define MAX_WORD 256 //Massima lunghezza di una parola nel dizionario

TrieNode* crea_nodo(){

    //Allocazione memoria nuovo nodo 
    TrieNode * node = (TrieNode*)malloc(sizeof(TrieNode));

    if(node == NULL){
        perror("Nella malloc nodo trie");
        exit(EXIT_FAILURE);
    }

    node->is_end_of_word = 0; 

    for(int i = 0; i < ALPHABET_SIZE; i++){
        node->children[i] = NULL;
    }

    return node; 
}


void insert_word(struct TrieNode *root, char *word){
    struct TrieNode * curr = root; 

    to_uppercase(word);

    while(*word){
        int index = *word - 'A'; 
        if(!curr->children[index]){
            curr->children[index] = crea_nodo();
        }
        curr = curr->children[index];
        word++;
    }
    curr->is_end_of_word = 1; 
}

int search_word(TrieNode *root, char *word){
    TrieNode * curr = root; 

    to_uppercase(word);

    while(*word){
        int index = *word - 'A'; 
        if(!curr->children[index]){
            return 0; 
        } 
        curr = curr->children[index];
        word++;
    }
    return curr && curr->is_end_of_word; 
}


TrieNode *load_file(TrieNode *root, char *filename){
    FILE *file = fopen(filename, "r");

    if(!file){
        perror("Errore apertura file");
        return NULL;
    }

    char word[MAX_WORD]; 

    while(fscanf(file, "%s", word) != EOF){
        to_uppercase(word); 
        insert_word(root, word);
    }

    fclose(file);
    return root; 
}

void free_trie(TrieNode *node){
    if(node == NULL){
        return;
    }

    for(int i = 0; i < ALPHABET_SIZE; i++){
        free_trie(node->children[i]);
    }
    free(node); 
}