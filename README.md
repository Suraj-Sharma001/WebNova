# 🚀 Getting Started

### Prerequisites

* A C compiler (like `gcc`)
* `make`

### Installation & Running

1.  **Clone the repository** (if applicable)
    ```bash
    git clone <your-repo-url>
    cd <your-repo-directory>
    ```

2.  **Compile the server**
    ```bash
    make
    ```

3.  **Run the proxy server** on a specified port (e.g., `8080`)
    ```bash
    ./proxy_server 8080
    ```
    The server is now running and listening for connections on `localhost:8080`.

---

## ⚙️ API Endpoints & Testing Guide

Here’s how to test the various functionalities using `curl`. The `-v` flag is used for verbose output to see request/response headers.

### A. Serve Local File (with Cache)

* **Endpoint**: `GET /find/<filename>`
* **Description**: Retrieves a file from the local `./find/` directory. The first request caches the file in memory.

#### 1. First Request (Cache Miss)
This request reads the file from disk and caches it.

* **Command:**
    ```bash
    curl -v http://localhost:8080/find/test.txt
    ```
* **Expected Server Logs:**
    ```log
    [THREAD] Handling GET request for /find/test.txt
    [FIND] Cached full response for /find/test.txt (XXX bytes)
    ```

#### 2. Second Request (Cache Hit)
This request serves the file directly from the in-memory cache.

* **Command:**
    ```bash
    curl -v http://localhost:8080/find/test.txt
    ```
* **Expected Server Logs:**
    ```log
    [FIND] Serving /find/test.txt from cache
    ```

#### 3. File Not Found

* **Command:**
    ```bash
    curl -v http://localhost:8080/find/missing.txt
    ```
* **Expected Output:** `File not found.`
* **Expected Server Logs:**
    ```log
    [FIND] File ./find/missing.txt not found.
    ```

---

### B. Create or Update a File

* **Endpoint**: `PUT /find/<filename>`
* **Description**: Creates a new file or overwrites an existing file in `./find/` with the raw request body.

* **Command:**
    ```bash
    curl -X PUT http://localhost:8080/find/new.txt -d "This is a new file"
    ```
* **Expected Output:**
    ```http
    HTTP/1.1 201 Created
    ```
* **Result**: A file named `new.txt` containing "This is a new file" is created in the `./find/` directory.
* **Expected Server Logs:**
    ```log
    [PUT] File saved: ./find/new.txt
    ```

---


### C. Upload a File

* **Endpoint**: `POST /upload/<filename>`
* **Description**: Uploads a binary or text file to the `./uploads/` directory on the server.

1.  **Create a test file:**
    ```bash
    echo "Upload test file" > sample.txt
    ```
2.  **Upload the file:**
    ```bash
    curl -X POST -T sample.txt http://localhost:8080/upload/sample.txt
    ```
* **Expected Output (HTML):**
    ```html
    <html><body><h1>File uploaded successfully: sample.txt</h1></body></html>
    ```
* **Expected Server Logs:**
    ```log
    [UPLOAD] File upload requested: /upload/sample.txt
    [UPLOAD] File saved as ./uploads/sample.txt
    ```

---

## D. Demonstration of Threading

**Scenario:**
The server handles multiple client connections simultaneously.

**Example Commands (Open multiple terminals):**

```bash
curl -X GET http://localhost:8080/find/test1.txt
curl -X GET http://localhost:8080/find/test2.txt
curl -X GET http://localhost:8080/find/test3.txt
```

**Example Output:**

```
[THREAD] Handling GET request for /find/test1.txt
[THREAD] Handling GET request for /find/test2.txt
[THREAD] Handling GET request for /find/test3.txt
```

**Explanation:**
Multiple requests are handled in separate threads, allowing simultaneous processing without blocking other clients.

---

### E. Download a File

* **Endpoint**: `GET /files/<filename>`
* **Description**: Downloads a specified file. The server adds a `Content-Disposition` header to suggest a filename to the client.

* **Command:**
    The `-OJ` flags tell `curl` to use the server-suggested filename.
    ```bash
    curl -OJ http://localhost:8080/files/sample.txt
    ```
* **Behavior**: The file `sample.txt` will be downloaded to your current local directory.
* **Expected Server Logs:**
    ```log
    [DOWNLOAD] File download requested: /files/sample.txt
    [FILE] Read file: sample.txt, size: 19 bytes
    ```