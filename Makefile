# CC = g++
# CFLAGS = -g -Wall -pthread

# # Object files
# OBJS = proxy_parse.o proxy_server_with_cache.o file_share.o

# # Final target
# all: proxy

# proxy: $(OBJS)
# 	$(CC) $(CFLAGS) -o proxy $(OBJS)

# proxy_parse.o: proxy_parse.c proxy_parse.h
# 	$(CC) $(CFLAGS) -c proxy_parse.c

# proxy_server_with_cache.o: proxy_server_with_cache.c proxy_parse.h file_share.h
# 	$(CC) $(CFLAGS) -c proxy_server_with_cache.c

# file_share.o: file_share.c file_share.h
# 	$(CC) $(CFLAGS) -c file_share.c

# clean:
# 	rm -f proxy *.o

# tar:
# 	tar -cvzf ass1.tgz proxy_server_with_cache.c proxy_parse.c proxy_parse.h \
# 	file_share.c file_share.h README Makefile

# Makefile for Proxy Server

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE -Isrc
LDFLAGS = -pthread
TARGET = proxy_server
SRCDIR = src

# Source files (in src directory)
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/proxy_parse.c $(SRCDIR)/cache.c $(SRCDIR)/http_handler.c $(SRCDIR)/file_share.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = $(SRCDIR)/proxy_parse.h $(SRCDIR)/cache.h $(SRCDIR)/http_handler.h $(SRCDIR)/file_share.h

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build completed: $(TARGET)"

# Individual object file rules
$(SRCDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/proxy_parse.h $(SRCDIR)/cache.h $(SRCDIR)/http_handler.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/main.c -o $(SRCDIR)/main.o

$(SRCDIR)/proxy_parse.o: $(SRCDIR)/proxy_parse.c $(SRCDIR)/proxy_parse.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/proxy_parse.c -o $(SRCDIR)/proxy_parse.o

$(SRCDIR)/cache.o: $(SRCDIR)/cache.c $(SRCDIR)/cache.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/cache.c -o $(SRCDIR)/cache.o

$(SRCDIR)/http_handler.o: $(SRCDIR)/http_handler.c $(SRCDIR)/http_handler.h $(SRCDIR)/proxy_parse.h $(SRCDIR)/cache.h $(SRCDIR)/file_share.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/http_handler.c -o $(SRCDIR)/http_handler.o

$(SRCDIR)/file_share.o: $(SRCDIR)/file_share.c $(SRCDIR)/file_share.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/file_share.c -o $(SRCDIR)/file_share.o

# Clean build files
clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
	@echo "Clean completed"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/"

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstalled from /usr/local/bin/"

# Run the server
run: $(TARGET)
	./$(TARGET)

# Run with custom port
run-port: $(TARGET)
	./$(TARGET) 8888

# Test compilation
test-compile:
	@echo "Testing compilation of each file..."
	$(CC) $(CFLAGS) -c $(SRCDIR)/main.c -o $(SRCDIR)/main.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/proxy_parse.c -o $(SRCDIR)/proxy_parse.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/cache.c -o $(SRCDIR)/cache.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/http_handler.c -o $(SRCDIR)/http_handler.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/file_share.c -o $(SRCDIR)/file_share.o
	@echo "All files compiled successfully!"

# Check what files exist
check-files:
	@echo "Checking source files in $(SRCDIR)/:"
	@ls -la $(SRCDIR)/

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build the proxy server (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized release version"
	@echo "  clean        - Remove build files"
	@echo "  install      - Install to /usr/local/bin"
	@echo "  uninstall    - Remove from /usr/local/bin"
	@echo "  run          - Build and run server on port 8080"
	@echo "  run-port     - Build and run server on port 8888"
	@echo "  test-compile - Test compilation of each source file"
	@echo "  check-files  - List files in src directory"
	@echo "  help         - Show this help message"

.PHONY: all clean debug release install uninstall run run-port test-compile check-files help
