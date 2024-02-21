/* 
 * querier.c - CS50 'querier' module
 *
 * Sydney Savarese, February 2024
 */
 #include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "pagedir.h"
#include "webpage.h"
#include "bag.h"
#include "hashtable.h"
#include "mem.h"
#include "index.h"
#include "word.h"
#include "file.h"

// #define UNIT_TEST 
//uncomment to run unit tests


typedef struct two_counters {       //inspired by 22F activity on Pierson's website 
    counters_t *result;
    counters_t *ctrs;
} two_counters_t;

typedef struct {
    int score;
    int docID;
    char *url;
} SearchResult;

typedef struct {
    SearchResult *results;
    const char *pageDirectory;
    int *numResults;
} IterateParams;

static void query(char* pageDirectory, index_t* fileIndex);
bool validate_query(char** words, int count);
static int buildWordArray(const char *input, char** words);
counters_t* get_scores(index_t *ht, char** words, int numWords, index_t *scores);
static void intersection(counters_t *intersection, counters_t *ctrs);
static void intersection_helper(void *arg, int key, int count);
static void unions(counters_t* unionctrs, counters_t* newCtrs);
static void unions_helper(void* arg, const int key, const int item);
char* getURL(int docID, const char* pageDirectory);
void print_search_results(SearchResult *results, int numResults);
void free_search_results(SearchResult *results, int numResults);
void add_to_results(void *arg, const int key, const int count);
void sort_results(SearchResult *results, int numResults);
void merge(counters_t **andSequence, counters_t **orSequence);
void copy_helper(void *arg, const int key, const int count); 



#ifndef UNIT_TEST
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: pageDirectory index_indexFile\n");
        return false;
    }

    char* pageDirectory = argv[1];
    if (!pagedir_validate(pageDirectory)) {
        fprintf(stderr, "not a crawler directory\n");
        return false;
    }

    char* indexFile = argv[2];
    FILE *fp = fopen(indexFile, "r");
    if (fp == NULL) {
        fprintf(stderr, "file is not readable\n");
        return false;
    } 
        fclose(fp);
    
    index_t *fileIndex = index_load(indexFile);
    query(pageDirectory, fileIndex);
    index_delete(fileIndex);
    
}
#endif //UNIT_TEST

static void query(char* pageDirectory, index_t* fileIndex) {
    char* line;
    printf("Query?\n");
    counters_t* scores = NULL; // Initialize scores pointer to NULL
    while ((line = file_readLine(stdin)) != NULL) {
        char** words = mem_malloc(sizeof(char*) * 20); 
        index_t* and_scores = index_new(200);
        int numResults = 100;       //max allowed results
        SearchResult *results = mem_malloc(numResults * sizeof(SearchResult));
        if (results == NULL) {
            // Handle error
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

        // Initialize each SearchResult struct in the array
        for (int i = 0; i < numResults; i++) {
            results[i].score = 0; // Assuming score is an integer
            results[i].docID = 0; // Assuming docID is an integer
            results[i].url = NULL; // Assuming url is a pointer
        }

        // Check if memory allocation was successful
        if (results == NULL) {
            // Handle error
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

        int numWords;
        // Find the position of the newline character
        int newline_pos = strcspn(line, "\n");
        // Replace the newline character with the null terminator
        line[newline_pos] = '\0';
        numWords = buildWordArray(line, words);
        if (line != NULL) {  
            if (!validate_query(words, numWords)) {
                mem_free(line);
                index_delete(and_scores);
                for (int i = 0; i < numWords; i++) {
                    free(words[i]);
                }
                mem_free(words);
                 if (scores != NULL) {
                    counters_delete(scores);
                    scores = NULL;
                }   
                mem_free(results);
                printf("results deleted\n");
                printf("Query?\n");
                continue;  
            }
            
            // Free the previously allocated memory for scores
            if (scores != NULL) {
                counters_delete(scores);
                scores = NULL;
            }
            scores = get_scores(fileIndex, words, numWords, and_scores);
            
            if (scores == NULL) {
                mem_free(results);
                if(words != NULL) {
                    printf("No documents match\n");
                }
            } else {
                printf("Query: ");
                for(int i = 0; i < numWords; i++) {
                    printf("%s ", words[i]);
                }
                // Iterate over the idcounts counter set
                IterateParams params = { .results = results, .pageDirectory = pageDirectory };
                counters_iterate(scores, &params, add_to_results);               
                sort_results(params.results, params.numResults);
                print_search_results(params.results, params.numResults);
                free_search_results(params.results, params.numResults);
            }
            mem_free(line);
            index_delete(and_scores);
            if(scores != NULL) {
                counters_delete(scores); // Free scores before the loop ends
                scores = NULL;
            }
            for (int i = 0; i < numWords; i++) {
                free(words[i]);
            }
            mem_free(words);
            printf("Query?\n");
        }
    }
}


bool validate_query(char** words, int count)
{
	
	char* word = words[0];
	char* prev = word;
	
	for (int i = 0; i < count; i++) {
		word = words[i];
		
		// check syntax
		if ((i == 0) && (strcmp(word, "or") == 0  ||strcmp(word, "and")==0)) {
			fprintf(stderr, "Error: '%s' cannot be first\n", word);
			return false;
		} else if ((i == count - 1) && (strcmp(word, "or") == 0  ||strcmp(word, "and")==0)) {
			fprintf(stderr, "Error: '%s' cannot be last\n", word);
			return false;
		} else if ((i != 0) && (strcmp(prev, "or") == 0  ||strcmp(prev, "and")==0) && (strcmp(word, "or") == 0  ||strcmp(word, "and")==0)) {
			fprintf(stderr, 
			 "Error: '%s' and '%s' cannot be adjacent\n", prev, word);
			return false;
		}

		prev = word;
	}
	return true;
}

   void add_to_results(void *arg, const int key, const int count) {
        IterateParams *params = (IterateParams *)arg;
        SearchResult *results = params->results;
        const char *pageDirectory = params->pageDirectory;
        params->numResults++;
    
        // Extract score, docID, and URL from key and count
        results[key].score = count;
        results[key].docID = key;
        results[key].url = getURL(key, pageDirectory); 
   }
    
/**
 * Searches the index for each word in the query, computes scores 
 * for documents matching each word, and returns the final scores 
 * per document after applying AND/OR logic.
 *
 * Takes the index hash table, array of query words, number of words,
 * and empty score counters to populate. For each word, looks up matching
 * docs and scores in the index. Maintains a current "andSequence" and 
 * "orSequence" of scores. On AND, intersects the scores. On OR, merges.
 * Returns the final scores per doc after processing all words.
 */
counters_t* get_scores(index_t *ht, char** words, int numWords, index_t *scores) {
    counters_t *andSequence = NULL;
    counters_t *orSequence = NULL;
    bool shortCircuit = false;
    
    for (int i = 0; i < numWords; i++) {
        char* word = words[i];

        if (strcmp(word, "or") == 0) {
            // Merge andSequence into orSequence
            merge(&andSequence, &orSequence);
            shortCircuit = false;
            continue;
        }

        if (shortCircuit) {
            // Skip this word and continue to the next one
            continue;
        }

        if (strcmp(word, "and") == 0) {
            // Continue to the next word
            continue;
        }

        // Regular word
        counters_t *match = hashtable_find(ht, word);
        
        if (match == NULL) {
            // No match found, set shortCircuit flag
            shortCircuit = true;
            if (andSequence != NULL) {
                // Drop the andSequence
                counters_delete(andSequence);
                andSequence = NULL;
            }
        } else {
            if (andSequence == NULL) {
                // Initialize andSequence
                andSequence = counters_new();
                unions(andSequence, match);
            } else {
                // Intersect with match
                intersection(andSequence, match);
            }
        }
    }

    // Merge andSequence into orSequence
    merge(&andSequence, &orSequence);

    // Free resources
    if (andSequence != NULL) {
        counters_delete(andSequence);
    }

    // Return the final orSequence
    return orSequence;
}


// Merges the andSequence into the orSequence
void merge(counters_t **andSequence, counters_t **orSequence) {
    if (*andSequence != NULL) {
        if (*orSequence == NULL) {
            // If orSequence is NULL, set it equal to andSequence
            *orSequence = counters_new();
            // Copy the content of andSequence into orSequence
            counters_iterate(*andSequence, *orSequence, copy_helper);
        } else {
            // Union andSequence into orSequence
            unions(*orSequence, *andSequence);
        }
        // Free the memory allocated for andSequence and set it to NULL
        counters_delete(*andSequence);
        *andSequence = NULL; 
    }

    if (*orSequence == NULL) {
        return;
    }
    // return orSequence;
}


// Helper function to copy each element from one counter to another
void copy_helper(void *arg, const int key, const int count) {
    counters_t *destination = arg;
    counters_set(destination, key, count);
}





static void intersection(counters_t *result, counters_t *ctrs)
/**
 * Intersects two counter objects by iterating over the counters in result 
 * Modifies result in place to contain the intersection.
*/
{
        mem_assert(result, "Counters intersect error\n");
        mem_assert(ctrs, "Counters intersect error\n");
	
        two_counters_t counters = { result, ctrs };
        counters_iterate(result, &counters, intersection_helper);
}

/* intersection_helper() asisists counters_intersect()
 */
static void intersection_helper(void *arg, int key, int count)
/**
 * Helper function for counters_intersect(). Compares the counts for a given 
 * key in two counter objects and sets the count for that key in the result 
 * counter to the lesser of the two. This has the effect of intersecting the
 * counter sets by keeping only keys present in both sets and using the 
 * lesser count.
*/
{
        two_counters_t* counters = arg;

        int num = 0;
        if (count > counters_get(counters->ctrs, key)) {
                num = counters_get(counters->ctrs, key);
        } else {
                num = count;
        }

        counters_set(counters->result, key, num);
}




static void unions(counters_t* unionctrs, counters_t* newCtrs)
{
    counters_iterate(newCtrs, unionctrs, unions_helper);
}

/* Consider one item for insertion into the other set.
 * If the other set does not contain the item, insert it;
 * otherwise, update the other set's item with sum of item values.
 */
static void unions_helper(void* arg, const int key, const int item)
{
    counters_t* unionctrs = arg;
    int itemB = item;
    
    // find the same key in unionctrs
    int itemA = counters_get(unionctrs, key);
    if (itemA == 0) {
        // not found: insert it
        counters_set(unionctrs, key, itemB);
        
    } else {
        // add to the existing value
        itemA += itemB;
        counters_set(unionctrs, key, itemA);
    }
}

// gets URL from a docID file
char* getURL(int docID, const char* pageDirectory) {
    char webfile[200];
    sprintf(webfile, "%s/%d", pageDirectory, docID);
    FILE* fp = fopen(webfile, "r");
    if (fp != NULL) {
            char* URL = file_readLine(fp);
            fclose(fp);
            return URL;
      }
      else {
          fprintf(stderr, "error: failed to retrieve docID URL\n");
          exit(1);
      }
 }

// Function to sort search results by score (descending order)
void sort_results(SearchResult *results, int numResults) {
    SearchResult temp;

// Assuming ptr points to some memory address
    for (int i = 0; i < numResults - 1; i++) {
        for (int j = 0; j < numResults - i - 1; j++) {
            if (results[j].score < results[j + 1].score) {
                // Swap elements if they are in the wrong order
                temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }
}

// Function to print search results
void print_search_results(SearchResult *results, int numResults) {
    int count = 0;
    for (int i = 0; i < numResults; i++) {
        if (results[i].score != 0 && results[i].url != NULL) {
                count++;
        }
    }
    printf("\n%d matching documents (ranked)\n", count);
    for (int i = 0; i < numResults; i++) {
        if (results[i].score != 0 && results[i].url != NULL) {
            printf("Score: %d     DocID: %d     %s\n", results[i].score, results[i].docID, results[i].url);
        }
    }
    if (count == 0) {
        printf("No documents match\n");
    }
    printf("\n");
}

// Function to free memory allocated for search results
void free_search_results(SearchResult *results, int numResults) {
    for (int i = 0; i < numResults; i++) {
        if(results[i].url != NULL) {
            mem_free(results[i].url);
            results[i].url = NULL;
        }
    }
    mem_free(results);
}

// Function to build an array of words from input string
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static int buildWordArray(const char *input, char **words) {
    int num = 0;
    const char *start;
    const char *end;

    // Loop through the input string
    while (*input != '\0') {
        // Skip non-alphabetic characters
        while (*input != '\0' && !isalpha(*input)) {
            input++;
        }

        // If we found an alphabetic character
        if (*input != '\0') {
            start = input; // Mark the start of the word

            // Find the end of the word
            while (*input != '\0' && isalpha(*input)) {
                input++;
            }
            end = input; // Points to the character after the word

            // Allocate memory for the word and copy it
            int wordLength = end - start;
            words[num] = mem_malloc((wordLength + 1) * sizeof(char));
            if (words[num] == NULL) {
                // Handle memory allocation failure
                fprintf(stderr, "Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }
            strncpy(words[num], start, wordLength);
            words[num][wordLength] = '\0'; // Null-terminate the word
            num++;
        }
    }

    return num;
}




/*****************************************************
 ******************** unit testing *******************
 *****************************************************/

#ifdef UNIT_TEST
// #include "unittest.h"

////////////////////////////////////////

int test_buildWordArray()
{
    char first[] = " this is a test for building the word array ";
    char** testarray  = mem_malloc(9 * sizeof(char*));
    int count1 = buildWordArray(first, testarray);

        for (int i = 0; i < count1; i++) {
            printf("printing words: ");
            printf("%s\n", testarray[i]);
        }   
    
    mem_free(testarray);

    return 0;  // Assuming success, change if you have specific tests
}

////////////////////////////////////////
int test_get_scores()
{
    char line0[] = "home playground ";
    char** words0 = mem_malloc(6 * sizeof(char*)); // Allocate space for all words
    int count0 = buildWordArray(line0, words0);
    index_t *ht = index_load("/thayerfs/home/f0055j2/cs50-dev/shared/tse/output/letters-2.index");
    index_t *scores = index_new(100);
    get_scores(ht, words0, count0, scores);
    index_save(scores, "/thayerfs/home/f0055j2/cs50-dev/tse-sydneysavarese/querier/getScorestest"); 

    mem_free(words0);
    for(int i = 0; i<count0; i++){
        mem_free(words0[i]);
    }
    index_delete(ht);
    index_delete(scores);

    return 0; // Indicate success
}


////////////////////////////////////////
// test main()

int main(const int argc, const char *argv[])
{
    printf("test main\n");

    test_buildWordArray();
    test_get_scores();

}

#endif // UNIT_TEST