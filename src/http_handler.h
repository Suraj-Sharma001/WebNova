#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include "proxy_parse.h"   // Ensure this defines struct ParsedRequest

// HTTP request handlers
int handle_get(int clientSocket, struct ParsedRequest* request, char* raw_request);
int handle_post(int clientSocket, struct ParsedRequest* request, char* raw_request);
int handle_file_upload(int clientSocket, struct ParsedRequest* request);
int handle_file_download(int clientSocket, struct ParsedRequest* request);

#endif