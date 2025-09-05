#include "proxy_parse.h"

struct ParsedRequest* ParsedRequest_create() {
    struct ParsedRequest* pr = (struct ParsedRequest*)malloc(sizeof(struct ParsedRequest));
    if(!pr) return NULL;
    
    memset(pr, 0, sizeof(struct ParsedRequest));
    return pr;
}

void ParsedRequest_destroy(struct ParsedRequest* pr) {
    if(!pr) return;
    
    free(pr->method);
    free(pr->protocol);
    free(pr->host);
    free(pr->port);
    free(pr->path);
    free(pr->version);
    free(pr->body);
    
    for(int i = 0; i < pr->header_count; i++) {
        free(pr->headers[i]);
    }
    
    free(pr);
}

static char* duplicate_string(const char* src, int len) {
    if(!src || len <= 0) return NULL;
    
    char* dst = malloc(len + 1);
    if(!dst) return NULL;
    
    strncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

static char* trim_whitespace(char* str) {
    if(!str) return NULL;
    
    // Trim leading whitespace
    while(isspace(*str)) str++;
    
    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;
    *(end + 1) = '\0';
    
    return str;
}

int ParsedRequest_parse(struct ParsedRequest* pr, const char* buffer, int buf_len) {
    if(!pr || !buffer || buf_len <= 0) return -1;
    
    char* buf_copy = malloc(buf_len + 1);
    if(!buf_copy) return -1;
    
    strncpy(buf_copy, buffer, buf_len);
    buf_copy[buf_len] = '\0';
    
    // Find the end of headers (double CRLF or double LF)
    char* header_end = strstr(buf_copy, "\r\n\r\n");
    if(header_end) {
        *header_end = '\0';
        pr->body = duplicate_string(header_end + 4, strlen(header_end + 4));
        pr->body_length = strlen(header_end + 4);
    } else {
        header_end = strstr(buf_copy, "\n\n");
        if(header_end) {
            *header_end = '\0';
            pr->body = duplicate_string(header_end + 2, strlen(header_end + 2));
            pr->body_length = strlen(header_end + 2);
        }
    }
    
    // Parse request line
    char* line = strtok(buf_copy, "\r\n");
    if(!line) {
        free(buf_copy);
        return -1;
    }
    
    char* method = strtok(line, " ");
    char* url = strtok(NULL, " ");
    char* version = strtok(NULL, " ");
    
    if(!method || !url || !version) {
        free(buf_copy);
        return -1;
    }
    
    pr->method = duplicate_string(method, strlen(method));
    pr->version = duplicate_string(version, strlen(version));
    
    // Parse URL
    char* url_copy = duplicate_string(url, strlen(url));
    if(!url_copy) {
        free(buf_copy);
        return -1;
    }
    
    // Check if URL has protocol
    if(strncmp(url_copy, "http://", 7) == 0) {
        pr->protocol = duplicate_string("http", 4);
        char* host_start = url_copy + 7;
        char* path_start = strchr(host_start, '/');
        
        if(path_start) {
            pr->path = duplicate_string(path_start, strlen(path_start));
            *path_start = '\0';
        } else {
            pr->path = duplicate_string("/", 1);
        }
        
        // Parse host and port
        char* port_start = strchr(host_start, ':');
        if(port_start) {
            pr->port = duplicate_string(port_start + 1, strlen(port_start + 1));
            *port_start = '\0';
        } else {
            pr->port = duplicate_string("80", 2);
        }
        
        pr->host = duplicate_string(host_start, strlen(host_start));
    } else {
        // Relative URL - extract from Host header later
        pr->path = duplicate_string(url_copy, strlen(url_copy));
        pr->protocol = duplicate_string("http", 4);
    }
    
    free(url_copy);
    
    // Parse headers
    char* header_line;
    while((header_line = strtok(NULL, "\r\n")) && pr->header_count < MAX_HEADERS) {
        if(strlen(header_line) == 0) break;
        
        pr->headers[pr->header_count] = duplicate_string(header_line, strlen(header_line));
        
        // Extract host if not already set
        if(!pr->host && strncasecmp(header_line, "Host:", 5) == 0) {
            char* host_value = header_line + 5;
            host_value = trim_whitespace(host_value);
            
            char* port_sep = strchr(host_value, ':');
            if(port_sep) {
                pr->host = duplicate_string(host_value, port_sep - host_value);
                pr->port = duplicate_string(port_sep + 1, strlen(port_sep + 1));
            } else {
                pr->host = duplicate_string(host_value, strlen(host_value));
                if(!pr->port) pr->port = duplicate_string("80", 2);
            }
        }
        
        pr->header_count++;
    }
    
    free(buf_copy);

    // Provide defaults if missing
    if(!pr->host) {
        pr->host = duplicate_string("localhost", 9);
    }
    if(!pr->port) {
        pr->port = duplicate_string("80", 2);
    }
    
    // Validate required fields
    if(!pr->method || !pr->host || !pr->path) {
        return -1;
    }
    
    return 0;
}

int ParsedRequest_unparse(struct ParsedRequest* pr, char* buffer, int buf_len) {
    if(!pr || !buffer || buf_len <= 0) return -1;
    
    int written = 0;
    
    // Request line
    written += snprintf(buffer + written, buf_len - written, 
                       "%s %s %s\r\n", 
                       pr->method, pr->path, pr->version);
    
    // Headers
    for(int i = 0; i < pr->header_count && written < buf_len - 1; i++) {
        written += snprintf(buffer + written, buf_len - written,
                           "%s\r\n", pr->headers[i]);
    }
    
    // End of headers
    if(written < buf_len - 2) {
        written += snprintf(buffer + written, buf_len - written, "\r\n");
    }
    
    // Body
    if(pr->body && pr->body_length > 0 && written + pr->body_length < buf_len) {
        memcpy(buffer + written, pr->body, pr->body_length);
        written += pr->body_length;
    }
    
    return written;
}

void ParsedRequest_print(struct ParsedRequest* pr) {
    if(!pr) return;
    
    printf("Method: %s\n", pr->method ? pr->method : "NULL");
    printf("Protocol: %s\n", pr->protocol ? pr->protocol : "NULL");
    printf("Host: %s\n", pr->host ? pr->host : "NULL");
    printf("Port: %s\n", pr->port ? pr->port : "NULL");
    printf("Path: %s\n", pr->path ? pr->path : "NULL");
    printf("Version: %s\n", pr->version ? pr->version : "NULL");
    printf("Headers (%d):\n", pr->header_count);
    
    for(int i = 0; i < pr->header_count; i++) {
        printf("  %s\n", pr->headers[i]);
    }
    
    if(pr->body) {
        printf("Body length: %d\n", pr->body_length);
    }
}
