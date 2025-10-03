#include <stdio.h>      // Standard I/O functions (fopen, fread, fwrite, printf, etc.)
#include <stdlib.h>     // Standard library (malloc, free, exit, etc.)
#include <string.h>     // String handling (strncpy, strerror, strlen, etc.)
#include <errno.h>      // For error reporting (errno variable, strerror)
#include <sys/stat.h>   // For file statistics and mkdir()
#include "file_share.h" // Header file with function prototypes


// Function: save_file
// Purpose : Saves raw data to a file on disk. Creates directory if needed.
// Params  : filename - path to file where data should be written
//           data     - pointer to raw file data buffer
//           size     - size of the data buffer in bytes
// Returns : 0 on success, -1 on failure

int save_file(const char* filename, const char* data, int size){
    // Validate input parameters
    if(!filename || !data || size < 0) {
        printf("[FILE] Invalid parameters for save_file\n");
        return -1;  // Return error
    }


    // Ensure the target directory exists before writing the file
 
    char* dir_end = strrchr(filename, '/'); // Find last '/' (end of directory path)
    if(dir_end) { // If there is a directory in the path
        // Allocate memory to store the directory path
        char* dir_path = malloc(dir_end - filename + 1);
        if(dir_path) {
            // Copy only the directory portion of the filename
            strncpy(dir_path, filename, dir_end - filename);
            dir_path[dir_end - filename] = '\0'; // Null-terminate string

            // Create directory (ignore error if already exists)
            mkdir(dir_path, 0755);

            // Free allocated memory
            free(dir_path);
        }
    }

   
    // Open the file in binary write mode ("wb")  
    // If file doesn't exist, it will be created. If it exists, it will be overwritten.
   
    FILE* fp = fopen(filename, "wb");
    if(!fp) {
        // fopen failed - print error with strerror
        printf("[FILE] Failed to open file for writing: %s - %s\n", filename, strerror(errno));
        return -1;
    }

   
    // Write raw data to file

    size_t written = fwrite(data, 1, size, fp); // Write data in binary mode
    fclose(fp); // Always close the file

    // Verify if the number of written bytes matches expected size
    if(written != (size_t)size) {
        printf("[FILE] Failed to write complete file: %s\n", filename);
        return -1;
    }

    // Success message
    printf("[FILE] Saved file: %s, size: %d bytes\n", filename, size);
    return 0; // Success
}


// Function: read_file
// Purpose : Reads a file from disk into memory
// Params  : filename - path to the file to read
//           data     - pointer to char* buffer (allocated inside)
//           size     - pointer to int where file size will be stored
// Returns : 0 on success, -1 on failure
// Note    : Caller must free(*data) after use

int read_file(const char* filename, char** data, int* size){
    // Validate input parameters
    if(!filename || !data || !size) {
        printf("[FILE] Invalid parameters for read_file\n");
        return -1;
    }

    
    // Open the file in binary read mode ("rb")

    FILE* fp = fopen(filename, "rb");
    if(!fp) {
        printf("[FILE] Failed to open file for reading: %s - %s\n", filename, strerror(errno));
        return -1;
    }


    // Get file size using fseek + ftell
   
    if(fseek(fp, 0, SEEK_END) != 0) { // Move to end
        printf("[FILE] Failed to seek to end of file: %s\n", filename);
        fclose(fp);
        return -1;
    }

    long file_size = ftell(fp); // Get current position (end offset = file size)
    if(file_size < 0) {
        printf("[FILE] Failed to get file size: %s\n", filename);
        fclose(fp);
        return -1;
    }
    rewind(fp); // Reset file pointer back to start

   
    // Allocate buffer to hold the entire file
 
    *data = (char*)malloc(file_size + 1); // +1 for null terminator (safety)
    if(!*data) {
        printf("[FILE] Failed to allocate memory for file: %s\n", filename);
        fclose(fp);
        return -1;
    }

    // Read the file contents into the allocated buffer
   


    size_t bytes_read = fread(*data, 1, file_size, fp);
    fclose(fp); // Close file after reading

    if(bytes_read != (size_t)file_size) {
        printf("[FILE] Failed to read complete file: %s\n", filename);
        free(*data); // Free allocated memory on failure
        *data = NULL;
        return -1;
    }

    (*data)[file_size] = '\0';   // Null terminate buffer for safety
    *size = (int)file_size;      // Store file size

    // Success message
    printf("[FILE] Read file: %s, size: %d bytes\n", filename, *size);
    return 0; // Success
}


// Function: file_exists
// Purpose : Checks if a file exists on disk
// Params  : filename - path to file
// Returns : 1 if exists, 0 if not

int file_exists(const char* filename){
    if(!filename) return 0; // Invalid path -> doesn't exist

    FILE* fp = fopen(filename, "r"); // Try to open file for reading
    if(fp) {
        fclose(fp);
        return 1; // File exists
    }
    return 0; // File does not exist
}


// Function: get_file_size
// Purpose : Retrieves file size using stat()
// Params  : filename - path to file
// Returns : file size in bytes, -1 on failure

long get_file_size(const char* filename){
    if(!filename) return -1; // Invalid path

    struct stat st;
    if(stat(filename, &st) == 0) {
        return st.st_size; // Return file size
    }
    return -1; // Error (file not found or stat failed)
}
