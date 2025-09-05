// /*
//   proxy_parse.c -- a HTTP Request Parsing Library.
//   COS 461  
// */

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

// #include "proxy_parse.h"

// #define DEFAULT_NHDRS 8
// #define MAX_REQ_LEN 65535
// #define MIN_REQ_LEN 4

// static const char *root_abs_path = "/";

// /* private function declartions */
// int ParsedRequest_printRequestLine(struct ParsedRequest *pr, 
// 				   char * buf, size_t buflen,
// 				   size_t *tmp);
// size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr);

// /*
//  * debug() prints out debugging info if DEBUG is set to 1
//  *
//  * parameter format: same as printf 
//  *
//  */
// void debug(const char * format, ...) {
//      va_list args;
//      if (DEBUG) {
// 	  va_start(args, format);
// 	  vfprintf(stderr, format, args);
// 	  va_end(args);
//      }
// }


// /*
//  *  ParsedHeader Public Methods
//  */

// /* Set a header with key and value */
// int ParsedHeader_set(struct ParsedRequest *pr, 
// 		     const char * key, const char * value)
// {
//      struct ParsedHeader *ph;
//      ParsedHeader_remove (pr, key);

//      if (pr->headerslen <= pr->headersused+1) {
// 	  pr->headerslen = pr->headerslen * 2;
// 	  pr->headers = 
// 	       (struct ParsedHeader *)realloc(pr->headers, 
// 		pr->headerslen * sizeof(struct ParsedHeader));
// 	  if (!pr->headers)
// 	       return -1;
//      }

//      ph = pr->headers + pr->headersused;
//      pr->headersused += 1;
     
//      ph->key = (char *)malloc(strlen(key)+1);
//      memcpy(ph->key, key, strlen(key));
//      ph->key[strlen(key)] = '\0';

//      ph->value = (char *)malloc(strlen(value)+1);
//      memcpy(ph->value, value, strlen(value));
//      ph->value[strlen(value)] = '\0';

//      ph->keylen = strlen(key)+1;
//      ph->valuelen = strlen(value)+1;
//      return 0;
// }


// /* get the parsedHeader with the specified key or NULL */
// struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, 
// 				      const char * key)
// {
//      size_t i = 0;
//      struct ParsedHeader * tmp;
//      while(pr->headersused > i)
//      {
// 	  tmp = pr->headers + i;
// 	  if(tmp->key && key && strcmp(tmp->key, key) == 0)
// 	  {
// 	       return tmp;
// 	  }
// 	  i++;
//      }
//      return NULL;
// }

// /* remove the specified key from parsedHeader */
// int ParsedHeader_remove(struct ParsedRequest *pr, const char *key)
// {
//      struct ParsedHeader *tmp;
//      tmp = ParsedHeader_get(pr, key);
//      if(tmp == NULL)
// 	  return -1;

//      free(tmp->key);
//      free(tmp->value);
//      tmp->key = NULL;
//      return 0;
// }


// /* modify the header with given key, giving it a new value
//  * return 1 on success and 0 if no such header found
//  * 
// int ParsedHeader_modify(struct ParsedRequest *pr, const char * key, 
// 			const char *newValue)
// {
//      struct ParsedHeader *tmp;
//      tmp = ParsedHeader_get(pr, key);
//      if(tmp != NULL)
//      {
// 	  if(tmp->valuelen < strlen(newValue)+1)
// 	  {
// 	       tmp->valuelen = strlen(newValue)+1;
// 	       tmp->value = (char *) realloc(tmp->value, 
// 					     tmp->valuelen * sizeof(char));
// 	  } 
// 	  strcpy(tmp->value, newValue);
// 	  return 1;
//      }
//      return 0;
// }
// */

// /*
//   ParsedHeader Private Methods
// */

// void ParsedHeader_create(struct ParsedRequest *pr)
// {
//      pr->headers = 
//      (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader)*DEFAULT_NHDRS);
//      pr->headerslen = DEFAULT_NHDRS;
//      pr->headersused = 0;
// } 


// size_t ParsedHeader_lineLen(struct ParsedHeader * ph)
// {
//      if(ph->key != NULL)
//      {
// 	  return strlen(ph->key)+strlen(ph->value)+4;
//      }
//      return 0; 
// }

// size_t ParsedHeader_headersLen(struct ParsedRequest *pr) 
// {
//      if (!pr || !pr->buf)
// 	  return 0;

//      size_t i = 0;
//      int len = 0;
//      while(pr->headersused > i)
//      {
// 	  len += ParsedHeader_lineLen(pr->headers + i);
// 	  i++;
//      }
//      len += 2;
//      return len;
// }

// int ParsedHeader_printHeaders(struct ParsedRequest * pr, char * buf, 
// 			      size_t len)
// {
//      char * current = buf;
//      struct ParsedHeader * ph;
//      size_t i = 0;

//      if(len < ParsedHeader_headersLen(pr))
//      {
// 	  debug("buffer for printing headers too small\n");
// 	  return -1;
//      }
  
//      while(pr->headersused > i)
//      {
// 	  ph = pr->headers+i;
// 	  if (ph->key) {
// 	       memcpy(current, ph->key, strlen(ph->key));
// 	       memcpy(current+strlen(ph->key), ": ", 2);
// 	       memcpy(current+strlen(ph->key) +2 , ph->value, 
// 		      strlen(ph->value));
// 	       memcpy(current+strlen(ph->key) +2+strlen(ph->value) , 
// 		      "\r\n", 2);
// 	       current += strlen(ph->key)+strlen(ph->value)+4;
// 	  }
// 	  i++;
//      }
//      memcpy(current, "\r\n",2);
//      return 0;
// }


// void ParsedHeader_destroyOne(struct ParsedHeader * ph)
// {
//      if(ph->key != NULL)
//      {
// 	  free(ph->key);
// 	  ph->key = NULL;
// 	  free(ph->value);
// 	  ph->value = NULL;
// 	  ph->keylen = 0;
// 	  ph->valuelen = 0;
//      }
// }

// void ParsedHeader_destroy(struct ParsedRequest * pr)
// {
//      size_t i = 0;
//      while(pr->headersused > i)
//      {
// 	  ParsedHeader_destroyOne(pr->headers + i);
// 	  i++;
//      }
//      pr->headersused = 0;

//      free(pr->headers);
//      pr->headerslen = 0;
// }


// int ParsedHeader_parse(struct ParsedRequest * pr, char * line)
// {
//      char * key;
//      char * value;
//      char * index1;
//      char * index2;

//      index1 = index(line, ':');
//      if(index1 == NULL)
//      {
// 	  debug("No colon found\n");
// 	  return -1;
//      }
//      key = (char *)malloc((index1-line+1)*sizeof(char));
//      memcpy(key, line, index1-line);
//      key[index1-line]='\0';

//      index1 += 2;
//      index2 = strstr(index1, "\r\n");
//      value = (char *) malloc((index2-index1+1)*sizeof(char));
//      memcpy(value, index1, (index2-index1));
//      value[index2-index1] = '\0';

//      ParsedHeader_set(pr, key, value);
//      free(key);
//      free(value);
//      return 0;
// }

// /*
//   ParsedRequest Public Methods
// */

// void ParsedRequest_destroy(struct ParsedRequest *pr)
// {
//      if(pr->buf != NULL)
//      {
// 	  free(pr->buf);
//      }
//      if (pr->path != NULL) {
// 	  free(pr->path);
//      }
//      if(pr->headerslen > 0)
//      {
// 	  ParsedHeader_destroy(pr);
//      }
//      free(pr);
// }

// struct ParsedRequest* ParsedRequest_create()
// {
//      struct ParsedRequest *pr;
//      pr = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
//      if (pr != NULL)
//      {
// 	  ParsedHeader_create(pr);
// 	  pr->buf = NULL;
// 	  pr->method = NULL;
// 	  pr->protocol = NULL;
// 	  pr->host = NULL;
// 	  pr->path = NULL;
// 	  pr->version = NULL;
// 	  pr->buf = NULL;
// 	  pr->buflen = 0;
//      }
//      return pr;
// }

// /* 
//    Recreate the entire buffer from a parsed request object.
//    buf must be allocated
// */
// int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf, 
// 			  size_t buflen)
// {
//      if (!pr || !pr->buf)
// 	  return -1;

//      size_t tmp;
//      if (ParsedRequest_printRequestLine(pr, buf, buflen, &tmp) < 0)
// 	  return -1;
//      if (ParsedHeader_printHeaders(pr, buf+tmp, buflen-tmp) < 0)
// 	  return -1;
//      return 0;
// }

// /* 
//    Recreate the headers from a parsed request object.
//    buf must be allocated
// */
// int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, 
// 				  size_t buflen)
// {
//      if (!pr || !pr->buf)
// 	  return -1;

//      if (ParsedHeader_printHeaders(pr, buf, buflen) < 0)
// 	  return -1;
//      return 0;
// }


// /* Size of the headers if unparsed into a string */
// size_t ParsedRequest_totalLen(struct ParsedRequest *pr)
// {
//      if (!pr || !pr->buf)
// 	  return 0;
//      return ParsedRequest_requestLineLen(pr)+ParsedHeader_headersLen(pr);
// }


// /* 
//    Parse request buffer
 
//    Parameters: 
//    parse: ptr to a newly created ParsedRequest object
//    buf: ptr to the buffer containing the request (need not be NUL terminated)
//    and the trailing \r\n\r\n
//    buflen: length of the buffer including the trailing \r\n\r\n
   
//    Return values:
//    -1: failure
//    0: success
// */
// int 
// ParsedRequest_parse(struct ParsedRequest * parse, const char *buf, 
// 		    int buflen)
// {
//      char *full_addr;
//      char *saveptr;
//      char *index;
//      char *currentHeader;

//      if (parse->buf != NULL) {
// 	  debug("parse object already assigned to a request\n");
// 	  return -1;
//      }
   
//      if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN) {
// 	  debug("invalid buflen %d", buflen);
// 	  return -1;
//      }
   
//      /* Create NUL terminated tmp buffer */
//      char *tmp_buf = (char *)malloc(buflen + 1); /* including NUL */
//      memcpy(tmp_buf, buf, buflen);
//      tmp_buf[buflen] = '\0';
   
//      index = strstr(tmp_buf, "\r\n\r\n");
//      if (index == NULL) {
// 	  debug("invalid request line, no end of header\n");
// 	  free(tmp_buf);
// 	  return -1;
//      }
   
//      /* Copy request line into parse->buf */
//      index = strstr(tmp_buf, "\r\n");
//      if (parse->buf == NULL) {
// 	  parse->buf = (char *) malloc((index-tmp_buf)+1);
// 	  parse->buflen = (index-tmp_buf)+1;
//      }
//      memcpy(parse->buf, tmp_buf, index-tmp_buf);
//      parse->buf[index-tmp_buf] = '\0';

//      /* Parse request line */
//      parse->method = strtok_r(parse->buf, " ", &saveptr);
//      if (parse->method == NULL) {
// 	  debug( "invalid request line, no whitespace\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }
//      if (strcmp (parse->method, "GET")) {
// 	  debug( "invalid request line, method not 'GET': %s\n", 
// 		 parse->method);
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }

//      full_addr = strtok_r(NULL, " ", &saveptr);

//      if (full_addr == NULL) {
// 	  debug( "invalid request line, no full address\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }

//      parse->version = full_addr + strlen(full_addr) + 1;

//      if (parse->version == NULL) {
// 	  debug( "invalid request line, missing version\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }
//      if (strncmp (parse->version, "HTTP/", 5)) {
// 	  debug( "invalid request line, unsupported version %s\n", 
// 		 parse->version);
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }


//      parse->protocol = strtok_r(full_addr, "://", &saveptr);
//      if (parse->protocol == NULL) {
// 	  debug( "invalid request line, missing host\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }
     
//      const char *rem = full_addr + strlen(parse->protocol) + strlen("://");
//      size_t abs_uri_len = strlen(rem);

//      parse->host = strtok_r(NULL, "/", &saveptr);
//      if (parse->host == NULL) {
// 	  debug( "invalid request line, missing host\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }
     
//      if (strlen(parse->host) == abs_uri_len) {
// 	  debug("invalid request line, missing absolute path\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  return -1;
//      }

//      parse->path = strtok_r(NULL, " ", &saveptr);
//      if (parse->path == NULL) {          // replace empty abs_path with "/"
// 	  int rlen = strlen(root_abs_path);
// 	  parse->path = (char *)malloc(rlen + 1);
// 	  strncpy(parse->path, root_abs_path, rlen + 1);
//      } else if (strncmp(parse->path, root_abs_path, strlen(root_abs_path)) == 0) {
// 	  debug("invalid request line, path cannot begin "
// 		"with two slash characters\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  parse->buf = NULL;
// 	  parse->path = NULL;
// 	  return -1;
//      } else {
// 	  // copy parse->path, prefix with a slash
// 	  char *tmp_path = parse->path;
// 	  int rlen = strlen(root_abs_path);
// 	  int plen = strlen(parse->path);
// 	  parse->path = (char *)malloc(rlen + plen + 1);
// 	  strncpy(parse->path, root_abs_path, rlen);
// 	  strncpy(parse->path + rlen, tmp_path, plen + 1);
//      }

//      parse->host = strtok_r(parse->host, ":", &saveptr);
//      parse->port = strtok_r(NULL, "/", &saveptr);

//      if (parse->host == NULL) {
// 	  debug( "invalid request line, missing host\n");
// 	  free(tmp_buf);
// 	  free(parse->buf);
// 	  free(parse->path);
// 	  parse->buf = NULL;
// 	  parse->path = NULL;
// 	  return -1;
//      }

//      if (parse->port != NULL) {
// 	  int port = strtol (parse->port, (char **)NULL, 10);
// 	  if (port == 0 && errno == EINVAL) {
// 	       debug("invalid request line, bad port: %s\n", parse->port);
// 	       free(tmp_buf);
// 	       free(parse->buf);
// 	       free(parse->path);
// 	       parse->buf = NULL;
// 	       parse->path = NULL;
// 	       return -1;
// 	  }
//      }

   
//      /* Parse headers */
//      int ret = 0;
//      currentHeader = strstr(tmp_buf, "\r\n")+2;
//      while (currentHeader[0] != '\0' && 
// 	    !(currentHeader[0] == '\r' && currentHeader[1] == '\n')) {
	  
// 	  //debug("line %s %s", parse->version, currentHeader);

// 	  if (ParsedHeader_parse(parse, currentHeader)) {
// 	       ret = -1;
// 	       break;
// 	  }

// 	  currentHeader = strstr(currentHeader, "\r\n");
// 	  if (currentHeader == NULL || strlen (currentHeader) < 2)
// 	       break;

// 	  currentHeader += 2;
//      }
//      free(tmp_buf);
//      return ret;
// }

// /* 
//    ParsedRequest Private Methods
// */

// size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr)
// {
//      if (!pr || !pr->buf)
// 	  return 0;

//      size_t len =  
// 	  strlen(pr->method) + 1 + strlen(pr->protocol) + 3 + 
// 	  strlen(pr->host) + 1 + strlen(pr->version) + 2;
//      if(pr->port != NULL)
//      {
// 	  len += strlen(pr->port)+1;
//      }
//      /* path is at least a slash */
//      len += strlen(pr->path);
//      return len;
// }

// int ParsedRequest_printRequestLine(struct ParsedRequest *pr, 
// 				   char * buf, size_t buflen,
// 				   size_t *tmp)
// {
//      char * current = buf;

//      if(buflen <  ParsedRequest_requestLineLen(pr))
//      {
// 	  debug("not enough memory for first line\n");
// 	  return -1; 
//      }
//      memcpy(current, pr->method, strlen(pr->method));
//      current += strlen(pr->method);
//      current[0]  = ' ';
//      current += 1;

//      memcpy(current, pr->protocol, strlen(pr->protocol));
//      current += strlen(pr->protocol);
//      memcpy(current, "://", 3);
//      current += 3;
//      memcpy(current, pr->host, strlen(pr->host));
//      current += strlen(pr->host);
//      if(pr->port != NULL)
//      {
// 	  current[0] = ':';
// 	  current += 1;
// 	  memcpy(current, pr->port, strlen(pr->port));
// 	  current += strlen(pr->port);
//      }
//      /* path is at least a slash */
//      memcpy(current, pr->path, strlen(pr->path));
//      current += strlen(pr->path);

//      current[0] = ' ';
//      current += 1;

//      memcpy(current, pr->version, strlen(pr->version));
//      current += strlen(pr->version);
//      memcpy(current, "\r\n", 2);
//      current +=2;
//      *tmp = current-buf;
//      return 0;
// }
