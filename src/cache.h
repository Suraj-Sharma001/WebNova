#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <pthread.h>   // For thread safety



#define MAX_CACHE_SIZE (10 * 1024 * 1024)  // 10 MB maximum cache size
#define MAX_ELEMENTS   1000                // Maximum number of cache entries




typedef struct cache_element cache_element;

struct cache_element {
    char* data;              // Cached response data
    int len;                 // Length of cached data (bytes)
    char* url;               // Unique URL key
    time_t lru_time_track;   // Last access timestamp (for LRU policy)
    cache_element* next;     // Pointer to next element in linked list
};



// Head pointer for linked list cache
extern cache_element* head;  

// Current total cache size (bytes)
extern int cache_size;       

// Mutex lock for thread-safe cache operations
extern pthread_mutex_t lock;  



// Find a cache element by URL
cache_element* cache_find(char* url);

// Add a new element to cache
int cache_add(char* data, int size, char* url);

// Remove least recently used (LRU) element
void cache_remove();

// Print all cache contents (for debugging)
void cache_print();

// Get current cache size (in bytes)
int cache_get_size();

// Clear all cache entries
void cache_clear();





// Check if cache contains a given URL (returns 1 if exists, else 0)
int cache_exists(char* url);

// Update LRU timestamp of an element (mark as recently used)
void cache_update_lru(cache_element* element);

// Get the oldest (L
