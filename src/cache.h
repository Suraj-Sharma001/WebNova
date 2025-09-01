#ifndef CACHE_H
#define CACHE_H

#include <time.h>

typedef struct cache_element cache_element;

struct cache_element{
    char* data;              // Response data
    int len;                 // Length of data
    char* url;               // URL key
    time_t lru_time_track;   // LRU timestamp
    cache_element* next;     // Next element in linked list
};

// Cache functions
cache_element* cache_find(char* url);
int cache_add(char* data, int size, char* url);
void cache_remove();
void cache_print();     // For debugging
int cache_get_size();   // Get current cache size
void cache_clear();     // Clear all cache

#endif