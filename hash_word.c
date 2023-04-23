#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_SIZE 100000

typedef struct WordNode {
    char* word;
	int count;
    struct WordNode* next;
} WordNode;

WordNode* create_word_node(char* word) {
    WordNode* node = (WordNode*) malloc(sizeof(WordNode));
    node->word = (char*) malloc(strlen(word) + 1);
    strcpy(node->word, word);
	node->count = 1;
    node->next = NULL;
    return node;
}

void insert_word(WordNode** hash_table, char* word) {
    unsigned long hash_value = 5381;
    int c;
	char* temp = word;
    while ((c = *temp++)) {
        hash_value = (hash_value << 5) + c;
    }
    int index = hash_value % HASH_TABLE_SIZE;
	printf("The word [%s] hash value is %ld, %d\n", word, hash_value, index);
    WordNode* node = hash_table[index];

    if (node == NULL) {
        hash_table[index] = create_word_node(word);
    } else {
	
        while (node != NULL) {
			if(strcmp(node->word, word) == 0) {
				node->count++;
				return;
			}
			if(node->next == NULL) {
            	break;
        	}
			node = node->next;
		}
        node->next = create_word_node(word);

		//node->count++;
    }
}

void print_word_list(WordNode** hash_table) {
    WordNode* node;
    int i, j;
	WordNode* sorted_list[HASH_TABLE_SIZE] = {NULL};
	if (hash_table == NULL) {
		printf("Word list is empty.\n");
		return;
	}
	printf("Not empty.\n");
/*
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        node = hash_table[i];
        while (node != NULL) {
			WordNode* temp = sorted_list[node->count];
			if(temp == NULL) {
				sorted_list[node->count] = node;
			} else {
				while (temp->next != NULL) {
					temp = temp->next;
				}
				temp->next = node;
			}
			node = node->next;
        }
    }


	for (i = HASH_TABLE_SIZE - 1; i >= 0; i--) {
        node = sorted_list[i];
		while (node != NULL) {
			printf("%s, %d\n", node->word, node->count);
			node = node->next;
		}

	}
*/
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
	//printf("#####%d, Not empty.\n", i);
		node = hash_table[i];
		while(node != NULL) {
			printf("%s, %d\n", node->word, node->count);
			node = node->next;
		}
	}
}
/*
int isNull(char c) {
	return (c >=65 && c <=122) ? 1 : 0;
}

int getWord(FILE* fp, char* str) {
	char c;
	int counter = 0;
	while((c = fgetc(fp)) != EOF){
			if( isNull(c) && (counter <= 0)){
					continue;
			}
			else if(isNull(c) && (counter > 0)){
					break;
			}
			str[counter++] = c;
	}
	str[counter] = '\0';
	if(counter > 0)
		return 1;
	else
		return 0;
}
*/
int main() {
    FILE* fp;
    char word[1024];
    WordNode* hash_table[HASH_TABLE_SIZE] = { NULL };
    int word_count = 0;

    fp = fopen("./input.txt", "r");
    if (fp == NULL) {
        printf("Failed to open input file.\n");
        return 1;
    }

    while (fscanf(fp, "%s", word) != EOF) {
        insert_word(hash_table, word);
		//printf("Have a look: %s\n", word);
        word_count++;
    }


    printf("Total words: %d\n", word_count);
    printf("Word list:\n");
    print_word_list(hash_table);

    fclose(fp);
    return 0;
}

