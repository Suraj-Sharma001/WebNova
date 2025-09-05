// /*
//  * proxy_parse.h -- a HTTP Request Parsing Library.
//  *
//  * Written by: Matvey Arye
//  * For: COS 518 
//  * 
//  */

#ifndef PROXY_PARSE_H
#define PROXY_PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_HEADERS 50
#define MAX_HEADER_LEN 1024
#define MAX_URL_LEN 2048

struct ParsedRequest {
    char *method;
    char *protocol;
    char *host;
    char *port;
    char *path;
    char *version;
    char *headers[MAX_HEADERS];
    int header_count;
    char *body;
    int body_length;
};

// Function declarations
struct ParsedRequest* ParsedRequest_create();
void ParsedRequest_destroy(struct ParsedRequest* pr);
int ParsedRequest_parse(struct ParsedRequest* pr, const char* buffer, int buf_len);
int ParsedRequest_unparse(struct ParsedRequest* pr, char* buffer, int buf_len);
void ParsedRequest_print(struct ParsedRequest* pr);

#endif
