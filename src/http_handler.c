#include "http_handler.h"
#include "cache.h"
#include "file_share.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "http_handler.h"
#include "cache.h"
#include "file_share.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h> 


#define MAX_BYTES 4096
#define MAX_RESPONSE_SIZE (50 * 1024 * 1024) // 50MB max response size
#define UPLOAD_DIR "./uploads"  // directory where files will be saved
#define MAX_FILE_SIZE (10 * 1024 * 1024) // 10MB


static int connect_remote_server(const char* host, int port){
    if(!host || port <= 0 || port > 65535) return -1;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("[HTTP] Socket creation failed");
        return -1;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 30; // 30 seconds
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct hostent* he = gethostbyname(host);
    if(!he) {
        printf("[HTTP] Failed to resolve host: %s\n", host);
        close(sock);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr, he->h_length);

    if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("[HTTP] Failed to connect to %s:%d - %s\n", host, port, strerror(errno));
        close(sock);
        return -1;
    }

    printf("[HTTP] Connected to %s:%d\n", host, port);
    return sock;
}

static char* create_cache_key(struct ParsedRequest* request) {
    if(!request || !request->host || !request->path) return NULL;
    
    int key_len = strlen(request->host) + strlen(request->path) + 32;
    char* key = malloc(key_len);
    if(!key) return NULL;
    
    snprintf(key, key_len, "%s:%s%s", 
             request->host, 
             request->port ? request->port : "80", 
             request->path);
    return key;
}

static int send_error_response(int clientSocket, int status_code, const char* message) {
    char response[1024];
    const char* status_text;
    
    switch(status_code) {
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 500: status_text = "Internal Server Error"; break;
        case 502: status_text = "Bad Gateway"; break;
        case 504: status_text = "Gateway Timeout"; break;
        default: status_text = "Error"; break;
    }
    
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><head><title>%d %s</title></head>"
        "<body><h1>%d %s</h1><p>%s</p></body></html>",
        status_code, status_text, (int)(strlen(message) + 100),
        status_code, status_text, status_code, status_text, message);
    
    return send(clientSocket, response, response_len, 0);
}

int handle_get(int clientSocket, struct ParsedRequest* request, char* raw_request){
    // Suppress unused parameter warning
    (void)raw_request;
    
    if(!request || !request->host || !request->path) {
        send_error_response(clientSocket, 400, "Invalid request");
        return -1;
    }
    
    printf("[HTTP] Handling GET request: %s%s\n", request->host, request->path);

    // Create cache key
    char* cache_key = create_cache_key(request);
    if(!cache_key) {
        send_error_response(clientSocket, 500, "Memory allocation failed");
        return -1;
    }

    // Check cache first
    cache_element* cached = cache_find(cache_key);
    if(cached){
        printf("[HTTP] Sending cached response (%d bytes)\n", cached->len);
        int sent = send(clientSocket, cached->data, cached->len, 0);
        free(cache_key);
        return sent > 0 ? 1 : -1;
    }

    // Connect to remote server
    int port = request->port ? atoi(request->port) : 80;
    int remoteSock = connect_remote_server(request->host, port);
    if(remoteSock < 0) {
        send_error_response(clientSocket, 502, "Failed to connect to remote server");
        free(cache_key);
        return -1;
    }

    // Reconstruct and send HTTP request
    char* http_request = malloc(8192);
    if(!http_request) {
        send_error_response(clientSocket, 500, "Memory allocation failed");
        close(remoteSock);
        free(cache_key);
        return -1;
    }
    
    int request_len = snprintf(http_request, 8192,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: ProxyServer/1.0\r\n"
        "\r\n",
        request->path, request->host);

    if(send(remoteSock, http_request, request_len, 0) < 0) {
        printf("[HTTP] Failed to send request to remote server\n");
        send_error_response(clientSocket, 502, "Failed to send request to remote server");
        close(remoteSock);
        free(http_request);
        free(cache_key);
        return -1;
    }
    free(http_request);

    // Receive and forward response
    char buffer[MAX_BYTES];
    char* full_response = malloc(1);
    int response_size = 0;
    int bytes;
    
    if(!full_response) {
        send_error_response(clientSocket, 500, "Memory allocation failed");
        close(remoteSock);
        free(cache_key);
        return -1;
    }
    full_response[0] = '\0';

    while((bytes = recv(remoteSock, buffer, MAX_BYTES, 0)) > 0) {
        // Forward data to client immediately
        if(send(clientSocket, buffer, bytes, 0) < 0) {
            printf("[HTTP] Failed to send data to client\n");
            break;
        }

        // Check if response is getting too large
        if(response_size + bytes > MAX_RESPONSE_SIZE) {
            printf("[HTTP] Response too large, not caching\n");
            // Continue forwarding but don't cache
            while((bytes = recv(remoteSock, buffer, MAX_BYTES, 0)) > 0) {
                send(clientSocket, buffer, bytes, 0);
            }
            close(remoteSock);
            free(full_response);
            free(cache_key);
            return 1;
        }

        // Store response for caching
        char* temp = realloc(full_response, response_size + bytes + 1);
        if(!temp) {
            printf("[HTTP] Memory allocation failed, continuing without caching\n");
            while((bytes = recv(remoteSock, buffer, MAX_BYTES, 0)) > 0) {
                send(clientSocket, buffer, bytes, 0);
            }
            close(remoteSock);
            free(full_response);
            free(cache_key);
            return 1;
        }
        full_response = temp;
        
        memcpy(full_response + response_size, buffer, bytes);
        response_size += bytes;
        full_response[response_size] = '\0';
    }

    close(remoteSock);

    if(bytes < 0) {
        printf("[HTTP] Error receiving data from remote server\n");
        free(full_response);
        free(cache_key);
        return -1;
    }

    // Cache the response if it's not too large
    if(response_size > 0 && response_size < MAX_RESPONSE_SIZE) {
        cache_add(full_response, response_size, cache_key);
    }

    free(full_response);
    free(cache_key);
    printf("[HTTP] GET request completed (%d bytes)\n", response_size);
    return 1;
}

// Basic POST handler: forwards to server without caching
int handle_post(int clientSocket, struct ParsedRequest* request, char* raw_request){
    if(!request || !request->host || !request->path) {
        send_error_response(clientSocket, 400, "Invalid request");
        return -1;
    }
    
    printf("[HTTP] Handling POST request: %s%s\n", request->host, request->path);

    int port = request->port ? atoi(request->port) : 80;
    int remoteSock = connect_remote_server(request->host, port);
    if(remoteSock < 0) {
        send_error_response(clientSocket, 502, "Failed to connect to remote server");
        return -1;
    }

    // Forward the original request
    if(send(remoteSock, raw_request, strlen(raw_request), 0) < 0) {
        printf("[HTTP] Failed to send POST request to remote server\n");
        send_error_response(clientSocket, 502, "Failed to send request to remote server");
        close(remoteSock);
        return -1;
    }

    // Forward response back to client
    char buffer[MAX_BYTES];
    int bytes;
    int total_bytes = 0;

    while((bytes = recv(remoteSock, buffer, MAX_BYTES, 0)) > 0){
        if(send(clientSocket, buffer, bytes, 0) < 0) {
            printf("[HTTP] Failed to send POST response to client\n");
            break;
        }
        total_bytes += bytes;
    }
    
    close(remoteSock);
    printf("[HTTP] POST request completed (%d bytes)\n", total_bytes);
    return 1;
}

int handle_put(int clientSocket, struct ParsedRequest* request, char* raw_request) {
    char filepath[1024];

    // Remove /find/ prefix if present
    const char* relative_path = request->path;
    if (strncmp(request->path, "/find/", 6) == 0) {
        relative_path = request->path + 6;
    }

    // Construct local file path
    snprintf(filepath, sizeof(filepath), "./find/%s", relative_path);

    // Open file for writing (create if it doesn't exist)
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0){
        perror("[PUT] Failed to open file");
        char resp[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\n\r\n";
        send(clientSocket, resp, strlen(resp), 0);
        return -1;
    }

    // Calculate body start (skip HTTP headers)
    char* body = strstr(raw_request, "\r\n\r\n");
    if(!body){
        close(fd);
        char resp[] = "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n";
        send(clientSocket, resp, strlen(resp), 0);
        return -1;
    }
    body += 4; // skip "\r\n\r\n"

    // Write body to file
    write(fd, body, strlen(body));
    close(fd);

    // Send success response
    char resp[] = "HTTP/1.1 201 Created\r\nContent-Length:0\r\n\r\n";
    send(clientSocket, resp, strlen(resp), 0);

    printf("[PUT] File saved: %s\n", filepath);
    return 0;
}


int handle_find(int clientSocket, struct ParsedRequest* request, char* raw_request) {
    char filepath[512];
    struct stat st;

    // Remove /find/ prefix for local file path
    const char* relative_path = request->path;
    if (strncmp(request->path, "/find/", 6) == 0) {
        relative_path = request->path + 6;
    }

    // Construct local file path
    snprintf(filepath, sizeof(filepath), "./find/%s", relative_path);

    // Check if file exists
    if (stat(filepath, &st) != 0) {
        const char* not_found = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/plain\r\n"
                                "Connection: close\r\n\r\n"
                                "File not found.\n";
        send(clientSocket, not_found, strlen(not_found), 0);
        return -1;
    }

    // Send HTTP header
    const char* header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n";
    send(clientSocket, header, strlen(header), 0);

    // Send file content
    FILE* f = fopen(filepath, "rb");
    if (!f) return -1;

    char buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        send(clientSocket, buffer, n, 0);
    }
    fclose(f);

    return 0;
}

// File upload handler
int handle_file_upload(int clientSocket, struct ParsedRequest* request, char* body, int body_len) {
    if (!request || !request->path || !body) {
        send_error_response(clientSocket, 400, "Invalid upload request");
        return -1;
    }

    printf("[UPLOAD] File upload requested: %s\n", request->path);

    // Ensure upload directory exists
    mkdir(UPLOAD_DIR, 0755);

    // Parse filename from path
    char* filename = strrchr(request->path, '/');
    if (!filename) filename = request->path;
    else filename++; // Skip '/'

    if (strlen(filename) == 0) {
        send_error_response(clientSocket, 400, "No filename specified");
        return -1;
    }

    // Save file
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename);
    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        send_error_response(clientSocket, 500, "Failed to save file");
        return -1;
    }

    int write_size = body_len;
    if (write_size > MAX_FILE_SIZE) write_size = MAX_FILE_SIZE;
    fwrite(body, 1, write_size, fp);
    fclose(fp);

    // Respond to client
    char response[512];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>File uploaded successfully: %s</h1></body></html>",
        strlen(filename) + 44, filename);

    send(clientSocket, response, len, 0);
    printf("[UPLOAD] File saved as %s\n", filepath);

    return 1;
}

// File download handler
int handle_file_download(int clientSocket, struct ParsedRequest* request) {


    if (!request || !request->path) {
        send_error_response(clientSocket, 400, "Invalid download request");
        return -1;
    }

    printf("[DOWNLOAD] File download requested: %s\n", request->path);

    // Check if it's a local file request
    if (strncmp(request->path, "/files/", 7) == 0) {
        char* filename = request->path + 7; // skip "/files/"
        char* file_data;
        int file_size;

        if (read_file(filename, &file_data, &file_size) == 0) {
            // Send file with headers
            char headers[1024];
            int header_len = snprintf(headers, sizeof(headers),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Content-Disposition: attachment; filename=\"%s\"\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n", filename, file_size);

            send(clientSocket, headers, header_len, 0);
            send(clientSocket, file_data, file_size, 0);
            free(file_data);
            return 1;
        } else {
            send_error_response(clientSocket, 404, "File not found");
            return -1;
        }
    }

    // Otherwise, fallback to GET handler
    char dummy_request[] = "";
    return handle_get(clientSocket, request, dummy_request);
}


