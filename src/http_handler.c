#include "http_handler.h"
#include "cache.h"
#include "file_share.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_BYTES 4096
#define MAX_RESPONSE_SIZE (50 * 1024 * 1024) // 50MB max response size

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

// File upload handler: basic implementation
int handle_file_upload(int clientSocket, struct ParsedRequest* request){
    if(!request || !request->path) {
        send_error_response(clientSocket, 400, "Invalid upload request");
        return -1;
    }
    
    printf("[HTTP] File upload requested: %s\n", request->path);
    
    // Extract filename from path
    char* filename = strrchr(request->path, '/');
    if(!filename) filename = request->path;
    else filename++; // Skip the '/'
    
    if(strlen(filename) == 0) {
        send_error_response(clientSocket, 400, "No filename specified");
        return -1;
    }
    
    // For now, just return a simple response
    // In a real implementation, you would parse multipart/form-data
    char response[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 84\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>File Upload</h1><p>Upload functionality not implemented</p></body></html>";
    
    send(clientSocket, response, strlen(response), 0);
    return 1;
}

// File download handler: uses cache and GET logic
int handle_file_download(int clientSocket, struct ParsedRequest* request){
    if(!request || !request->path) {
        send_error_response(clientSocket, 400, "Invalid download request");
        return -1;
    }
    
    printf("[HTTP] File download requested: %s\n", request->path);
    
    // Check if it's a local file request
    if(strncmp(request->path, "/files/", 7) == 0) {
        char* filename = request->path + 7; // Skip "/files/"
        char* file_data;
        int file_size;
        
        if(read_file(filename, &file_data, &file_size) == 0) {
            // Send file with appropriate headers
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
    
    // Otherwise, treat as regular GET request
    char dummy_request[] = "";
    return handle_get(clientSocket, request, dummy_request);
}