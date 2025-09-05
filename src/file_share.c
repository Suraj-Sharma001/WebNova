#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "file_share.h"

int save_file(const char* filename, const char* data, int size){
    if(!filename || !data || size < 0) {
        printf("[FILE] Invalid parameters for save_file\n");
        return -1;
    }
    
    // Create directory if it doesn't exist
    char* dir_end = strrchr(filename, '/');
    if(dir_end) {
        char* dir_path = malloc(dir_end - filename + 1);
        if(dir_path) {
            strncpy(dir_path, filename, dir_end - filename);
            dir_path[dir_end - filename] = '\0';
            mkdir(dir_path, 0755); // Create directory (ignore errors if exists)
            free(dir_path);
        }
    }
    
    FILE* fp = fopen(filename, "wb");
    if(!fp) {
        printf("[FILE] Failed to open file for writing: %s - %s\n", filename, strerror(errno));
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    if(written != (size_t)size) {
        printf("[FILE] Failed to write complete file: %s\n", filename);
        return -1;
    }
    
    printf("[FILE] Saved file: %s, size: %d bytes\n", filename, size);
    return 0;
}

int read_file(const char* filename, char** data, int* size){
    if(!filename || !data || !size) {
        printf("[FILE] Invalid parameters for read_file\n");
        return -1;
    }
    
    FILE* fp = fopen(filename, "rb");
    if(!fp) {
        printf("[FILE] Failed to open file for reading: %s - %s\n", filename, strerror(errno));
        return -1;
    }
    
    // Get file size
    if(fseek(fp, 0, SEEK_END) != 0) {
        printf("[FILE] Failed to seek to end of file: %s\n", filename);
        fclose(fp);
        return -1;
    }
    
    long file_size = ftell(fp);
    if(file_size < 0) {
        printf("[FILE] Failed to get file size: %s\n", filename);
        fclose(fp);
        return -1;
    }
    
    rewind(fp);
    
    // Allocate memory for file contents
    *data = (char*)malloc(file_size + 1);
    if(!*data) {
        printf("[FILE] Failed to allocate memory for file: %s\n", filename);
        fclose(fp);
        return -1;
    }
    
    // Read file contents
    size_t bytes_read = fread(*data, 1, file_size, fp);
    fclose(fp);
    
    if(bytes_read != (size_t)file_size) {
        printf("[FILE] Failed to read complete file: %s\n", filename);
        free(*data);
        *data = NULL;
        return -1;
    }
    
    (*data)[file_size] = '\0'; // Null terminate for safety
    *size = (int)file_size;
    
    printf("[FILE] Read file: %s, size: %d bytes\n", filename, *size);
    return 0;
}

int file_exists(const char* filename){
    if(!filename) return 0;
    
    FILE* fp = fopen(filename, "r");
    if(fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

long get_file_size(const char* filename){

    if(!filename) return -1;
    
    struct stat st;
    if(stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

