#ifndef FILE_SHARE_H
#define FILE_SHARE_H

// File operations
int save_file(const char* filename, const char* data, int size);
int read_file(const char* filename, char** data, int* size);
int file_exists(const char* filename);
long get_file_size(const char* filename);

#endif