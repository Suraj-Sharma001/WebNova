### cache issue

# Proxy Server Commands and Demonstrations

This document explains the main commands of the proxy server, along with examples and demonstrations of caching and threading.

---

## 1. GET Request

**Description:**
Retrieve a file from the server.

**Command:**

```bash
curl -X GET http://localhost:8080/find/test.txt
```

**Example Output:**

```
This is the content of test.txt
```

**Explanation:**
The server fetches the requested file and returns its content. If the file is not found, it returns a 404 Not Found.

---

## 2. PUT Request

**Description:**
Upload a file to the server.

**Command:**

```bash
curl -X PUT --data-binary @os.txt http://localhost:8080/find/love.txt
```

**Explanation:**

* `--data-binary @os.txt` reads the local file `os.txt`.
* Uploads it to the server at `/find/love.txt`.

**Example Output:**

```
File saved successfully.
```

---

## 3. FIND Request

**Description:**
Search for a file on the server.

**Command:**

```bash
curl -X FIND http://localhost:8080/find/test.txt
```

**Example Output:**

```
FIND handler working! You searched for: /find/test.txt
```

**Explanation:**
The server searches for the requested file path and responds if found.

---

## 4. Demonstration of Cache

**Scenario:**
The server caches previously requested files to improve speed.

**Example Commands:**

```bash
curl -X GET http://localhost:8080/find/test.txt
curl -X GET http://localhost:8080/find/test.txt
```

**Example Output:**

```
[CACHE] URL not found in cache: localhost:80/find/test.txt
[CACHE] Caching file: /find/test.txt
[CACHE] Returning cached file: /find/test.txt
```

**Explanation:**

* First request: File is not in cache, server fetches it.
* Second request: File served from cache, faster response.

---

## 5. Demonstration of Threading

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

You can use this Markdown file to demonstrate all the key functionalities of your proxy server to your faculty.
