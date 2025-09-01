#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_SIZE (200 * (1 << 20))       // 200 MB total cache
#define MAX_ELEMENT_SIZE (10 * (1 << 20)) // 10 MB per element

static cache_element* head = NULL;
static int cache_size = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Find a cached element by URL
cache_element* cache_find(char* url){
    if(!url) return NULL;
    
    pthread_mutex_lock(&lock);
    cache_element* site = head;
    while(site){
        if(strcmp(site->url, url) == 0){
            site->lru_time_track = time(NULL); // Update LRU timestamp
            printf("[CACHE] Found URL: %s, updated LRU time\n", url);
            pthread_mutex_unlock(&lock);
            return site;
        }
        site = site->next;
    }
    printf("[CACHE] URL not found in cache: %s\n", url);
    pthread_mutex_unlock(&lock);
    return NULL;
}

// Remove the least recently used element
void cache_remove(){
    pthread_mutex_lock(&lock);
    if(!head){
        pthread_mutex_unlock(&lock);
        return;
    }

    cache_element *prev = NULL, *curr = head;
    cache_element *lru_prev = NULL, *lru = head;

    // Find LRU element
    while(curr){
        if(curr->lru_time_track < lru->lru_time_track){
            lru = curr;
            lru_prev = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    // Remove lru element from linked list
    if(lru_prev) {
        lru_prev->next = lru->next;
    } else {
        head = lru->next;
    }

    int element_size = lru->len + sizeof(cache_element) + strlen(lru->url) + 1;
    cache_size -= element_size;
    
    printf("[CACHE] Removing URL from cache: %s, freed %d bytes\n", lru->url, element_size);
    
    free(lru->data);
    free(lru->url);
    free(lru);

    pthread_mutex_unlock(&lock);
}

// Add a new element to cache
int cache_add(char* data, int size, char* url){
    if(!data || !url || size <= 0) return 0;
    
    pthread_mutex_lock(&lock);

    // Check if URL already exists in cache
    cache_element* existing = head;
    while(existing) {
        if(strcmp(existing->url, url) == 0) {
            // Update existing entry
            free(existing->data);
            existing->data = malloc(size + 1);
            if(!existing->data) {
                pthread_mutex_unlock(&lock);
                return 0;
            }
            memcpy(existing->data, data, size);
            existing->data[size] = '\0';
            existing->len = size;
            existing->lru_time_track = time(NULL);
            printf("[CACHE] Updated existing URL in cache: %s\n", url);
            pthread_mutex_unlock(&lock);
            return 1;
        }
        existing = existing->next;
    }

    int element_size = size + strlen(url) + 1 + sizeof(cache_element);
    if(element_size > MAX_ELEMENT_SIZE){
        printf("[CACHE] Element size exceeds maximum (%d bytes), skipping cache: %s\n", element_size, url);
        pthread_mutex_unlock(&lock);
        return 0;
    }

    // Remove old elements until there's enough space
    while(cache_size + element_size > MAX_SIZE && head){
        pthread_mutex_unlock(&lock);
        cache_remove();
        pthread_mutex_lock(&lock);
    }

    cache_element* element = malloc(sizeof(cache_element));
    if(!element){
        perror("[CACHE] Failed to allocate memory for cache element");
        pthread_mutex_unlock(&lock);
        return 0;
    }

    element->data = malloc(size + 1);
    if(!element->data){
        perror("[CACHE] Failed to allocate memory for data");
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    memcpy(element->data, data, size);
    element->data[size] = '\0';

    element->url = malloc(strlen(url) + 1);
    if(!element->url){
        perror("[CACHE] Failed to allocate memory for URL");
        free(element->data);
        free(element);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    strcpy(element->url, url);

    element->lru_time_track = time(NULL);
    element->len = size;
    element->next = head;
    head = element;

    cache_size += element_size;
    printf("[CACHE] Added URL to cache: %s, size: %d bytes, total cache: %d bytes\n", url, size, cache_size);

    pthread_mutex_unlock(&lock);
    return 1;
}

// Print all cache contents
void cache_print(){
    pthread_mutex_lock(&lock);
    cache_element* site = head;
    printf("-----CACHE CONTENTS-----\n");
    printf("Total cache size: %d bytes\n", cache_size);
    int count = 0;
    while(site){
        printf("%d. URL: %s, Size: %d, LRU: %ld\n", ++count, site->url, site->len, site->lru_time_track);
        site = site->next;
    }
    printf("------------------------\n");
    pthread_mutex_unlock(&lock);
}

// Get current cache size
int cache_get_size(){
    pthread_mutex_lock(&lock);
    int size = cache_size;
    pthread_mutex_unlock(&lock);
    return size;
}

// Clear all cache
void cache_clear(){
    pthread_mutex_lock(&lock);
    while(head){
        cache_element* temp = head;
        head = head->next;
        free(temp->data);
        free(temp->url);
        free(temp);
    }
    cache_size = 0;
    printf("[CACHE] Cache cleared\n");
    pthread_mutex_unlock(&lock);
}