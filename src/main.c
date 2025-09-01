#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "proxy_parse.h"
#include "cache.h"
#include "http_handler.h"

#define MAX_CLIENTS 400

sem_t semaphore;
pthread_t tid[MAX_CLIENTS];
int thread_count = 0;
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_fn(void* arg){
    int clientSocket = *(int*)arg;
    free(arg);  // free memory allocated for client socket

    sem_wait(&semaphore);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if(bytes <= 0){
        printf("[THREAD] Client disconnected or error\n");
        close(clientSocket);
        sem_post(&semaphore);
        return NULL;
    }

    buffer[bytes] = '\0'; // Ensure null termination

    struct ParsedRequest* req = ParsedRequest_create();
    if(!req) {
        printf("[THREAD] Failed to create ParsedRequest\n");
        close(clientSocket);
        sem_post(&semaphore);
        return NULL;
    }

    if(ParsedRequest_parse(req, buffer, bytes) < 0){
        printf("[THREAD] Failed to parse request\n");
        close(clientSocket);
        ParsedRequest_destroy(req);
        sem_post(&semaphore);
        return NULL;
    }

    if(strcmp(req->method, "GET") == 0){
        printf("[THREAD] Handling GET request for %s\n", req->path);
        handle_get(clientSocket, req, buffer);
    } else if(strcmp(req->method, "POST") == 0){
        printf("[THREAD] Handling POST request for %s\n", req->path);
        handle_post(clientSocket, req, buffer);
    } else {
        printf("[THREAD] Unsupported method: %s\n", req->method);
        // Send 405 Method Not Allowed
        char response[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(clientSocket, response, strlen(response), 0);
    }

    close(clientSocket);
    ParsedRequest_destroy(req);
    sem_post(&semaphore);
    return NULL;
}

int main(int argc, char** argv){
    int port = 8080;
    if(argc == 2) {
        port = atoi(argv[1]);
        if(port <= 0 || port > 65535) {
            printf("[MAIN] Invalid port number. Using default port 8080\n");
            port = 8080;
        }
    }
    printf("[MAIN] Starting proxy server on port %d\n", port);

    sem_init(&semaphore, 0, MAX_CLIENTS);

    // Create server socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0){
        perror("[MAIN] Socket creation failed");
        exit(1);
    }

    // Set socket options to reuse address
    int opt = 1;
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[MAIN] Setsockopt failed");
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if(bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
        perror("[MAIN] Bind failed");
        close(serverSocket);
        exit(1);
    }

    if(listen(serverSocket, MAX_CLIENTS) < 0){
        perror("[MAIN] Listen failed");
        close(serverSocket);
        exit(1);
    }

    printf("[MAIN] Proxy server listening...\n");

    while(1){
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int* clientSocket = malloc(sizeof(int));
        if(!clientSocket){
            perror("[MAIN] Memory allocation failed");
            continue;
        }

        *clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if(*clientSocket < 0){
            perror("[MAIN] Accept failed");
            free(clientSocket);
            continue;
        }

        printf("[MAIN] Connection accepted from %s:%d\n",
               inet_ntoa(clientAddr.sin_addr),
               ntohs(clientAddr.sin_port));

        pthread_mutex_lock(&thread_mutex);
        int current_thread = thread_count % MAX_CLIENTS;
        thread_count++;
        pthread_mutex_unlock(&thread_mutex);

        if(pthread_create(&tid[current_thread], NULL, thread_fn, clientSocket) != 0) {
            perror("[MAIN] Thread creation failed");
            close(*clientSocket);
            free(clientSocket);
            continue;
        }
        pthread_detach(tid[current_thread]); // detach to automatically reclaim resources
    }

    close(serverSocket);
    sem_destroy(&semaphore);
    pthread_mutex_destroy(&thread_mutex);
    return 0;
}