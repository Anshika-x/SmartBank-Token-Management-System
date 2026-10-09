#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCKET closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSESOCKET close
#endif

#define MAX 100
#define HISTORY_MAX 10000
#define REQUEST_SIZE 32768

typedef struct {
    int token;
    char name[50];
    int service;
    int served;
} Customer;

Customer queue[MAX];
Customer history[HISTORY_MAX];
int front = -1, rear = -1;
int nextToken = 101;
int totalRegistered = 0, totalServed = 0, historyCount = 0;

const char *getService(int service) {
    switch (service) {
        case 1: return "Cash Deposit";
        case 2: return "Cash Withdrawal";
        case 3: return "Account Opening";
        default: return "Unknown";
    }
}
int isEmpty(void) { return front == -1; }
int isFull(void) { return (rear + 1) % MAX == front; }
int waitingCount(void) {
    if (isEmpty()) return 0;
    if (rear >= front) return rear - front + 1;
    return MAX - front + rear + 1;
}
void enqueue(Customer c) {
    if (isEmpty()) front = rear = 0;
    else rear = (rear + 1) % MAX;
    queue[rear] = c;
}
Customer dequeueCustomer(void) {
    Customer c = queue[front];
    if (front == rear) front = rear = -1;
    else front = (front + 1) % MAX;
    return c;
}
void saveHistory(Customer c) {
    if (historyCount < HISTORY_MAX) history[historyCount++] = c;
    else {
        memmove(history, history + 1, sizeof(Customer) * (HISTORY_MAX - 1));
        history[HISTORY_MAX - 1] = c;
    }
}
void jsonEscape(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < cap; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '"' || ch == '\\') { dst[j++] = '\\'; dst[j++] = (char)ch; }
        else if (ch == '\n' || ch == '\r') { dst[j++] = ' '; }
        else if (ch >= 32) dst[j++] = (char)ch;
    }
    dst[j] = '\0';
}
void sendResponse(socket_t client, int status, const char *type, const char *body) {
    char header[512];
    const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" :
                         status == 404 ? "Not Found" : status == 409 ? "Conflict" : "Internal Server Error";
    int len = (int)strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Connection: close\r\nAccess-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n\r\n",
        status, reason, type, len);
    send(client, header, (int)strlen(header), 0);
    send(client, body, len, 0);
}
void sendJson(socket_t client, int status, const char *body) {
    sendResponse(client, status, "application/json; charset=utf-8", body);
}
void sendFile(socket_t client, const char *path, const char *type) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        sendResponse(client, 404, "text/plain; charset=utf-8",
                     "File not found. Run the server from the SmartBank_Website folder.");
        return;
    }
    char body[20000];
    size_t n = fread(body, 1, sizeof(body) - 1, f);
    fclose(f);
    body[n] = '\0';
    sendResponse(client, 200, type, body);
}
char *jsonStringValue(const char *body, const char *key, char *out, size_t outSize) {
    char pattern[80];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return NULL;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return NULL;
    p++;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < outSize) {
        if (*p == '\\' && p[1]) p++;
        out[j++] = *p++;
    }
    out[j] = '\0';
    return out;
}
int jsonIntValue(const char *body, const char *key, int *value) {
    char pattern[80];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return 0;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!isdigit((unsigned char)*p) && *p != '-') return 0;
    *value = atoi(p);
    return 1;
}
void buildQueueJson(char *out, size_t cap) {
    size_t used = 0;
    used += snprintf(out + used, cap - used, "{\"customers\":[");
    int count = waitingCount(), i = front;
    for (int p = 0; p < count; p++) {
        Customer c = queue[i];
        char name[120];
        jsonEscape(c.name, name, sizeof(name));
        used += snprintf(out + used, cap - used,
            "%s{\"token\":%d,\"name\":\"%s\",\"service\":\"%s\",\"status\":\"Waiting\",\"position\":%d}",
            p ? "," : "", c.token, name, getService(c.service), p + 1);
        i = (i + 1) % MAX;
        if (used >= cap - 256) break;
    }
    snprintf(out + used, cap - used, "]}");
}
void handleRequest(socket_t client, const char *method, const char *path, const char *body) {
    char response[24000];
    if (strcmp(method, "OPTIONS") == 0) {
        sendJson(client, 200, "{}"); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        sendFile(client, "index.html", "text/html; charset=utf-8"); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/index.html") == 0) { sendFile(client, "index.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/register.html") == 0) { sendFile(client, "register.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/queue.html") == 0) { sendFile(client, "queue.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/service.html") == 0) { sendFile(client, "service.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/customers.html") == 0) { sendFile(client, "customers.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/analytics.html") == 0) { sendFile(client, "analytics.html", "text/html; charset=utf-8"); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/style.css") == 0) {
        sendFile(client, "style.css", "text/css; charset=utf-8"); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/app.js") == 0) {
        sendFile(client, "app.js", "application/javascript; charset=utf-8"); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/queue") == 0) {
        buildQueueJson(response, sizeof(response)); sendJson(client, 200, response); return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/stats") == 0) {
        snprintf(response, sizeof(response),
            "{\"totalRegistered\":%d,\"waiting\":%d,\"served\":%d,\"nextToken\":%d}",
            totalRegistered, waitingCount(), totalServed, nextToken);
        sendJson(client, 200, response); return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/register") == 0) {
        if (isFull()) { sendJson(client, 409, "{\"error\":\"The waiting queue is full. Please serve a customer first.\"}"); return; }
        Customer c; memset(&c, 0, sizeof(c));
        if (!jsonStringValue(body, "name", c.name, sizeof(c.name)) || strlen(c.name) == 0) {
            sendJson(client, 400, "{\"error\":\"Please enter a customer name.\"}"); return;
        }
        int service = 0;
        if (!jsonIntValue(body, "service", &service) || service < 1 || service > 3) {
            sendJson(client, 400, "{\"error\":\"Please select a valid banking service.\"}"); return;
        }
        c.token = nextToken++;
        c.service = service;
        c.served = 0;
        enqueue(c);
        saveHistory(c);
        totalRegistered++;
        snprintf(response, sizeof(response),
            "{\"message\":\"Token generated successfully.\",\"customer\":{\"token\":%d,\"name\":\"%s\",\"service\":\"%s\",\"status\":\"Waiting\"}}",
            c.token, c.name, getService(c.service));
        sendJson(client, 200, response); return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/serve") == 0) {
        if (isEmpty()) { sendJson(client, 409, "{\"error\":\"No customers are waiting.\"}"); return; }
        Customer c = dequeueCustomer();
        totalServed++;
        for (int h = historyCount - 1; h >= 0; h--) {
            if (history[h].token == c.token) { history[h].served = 1; break; }
        }
        snprintf(response, sizeof(response),
            "{\"message\":\"Customer served.\",\"customer\":{\"token\":%d,\"name\":\"%s\",\"service\":\"%s\",\"status\":\"Served\"}}",
            c.token, c.name, getService(c.service));
        sendJson(client, 200, response); return;
    }
    if (strcmp(method, "GET") == 0 && strncmp(path, "/api/search?token=", 18) == 0) {
        int token = atoi(path + 18);
        for (int h = historyCount - 1; h >= 0; h--) {
            if (history[h].token == token) {
                Customer c = history[h];
                char name[120]; jsonEscape(c.name, name, sizeof(name));
                snprintf(response, sizeof(response),
                    "{\"found\":true,\"customer\":{\"token\":%d,\"name\":\"%s\",\"service\":\"%s\",\"status\":\"%s\"}}",
                    c.token, name, getService(c.service), c.served ? "Served" : "Waiting");
                sendJson(client, 200, response); return;
            }
        }
        sendJson(client, 404, "{\"found\":false,\"error\":\"Token not found. Check the token number.\"}"); return;
    }
    sendJson(client, 404, "{\"error\":\"Endpoint not found.\"}");
}
int main(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock could not start.\n"); return 1;
    }
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) { printf("Could not create server socket.\n"); return 1; }
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Could not use port 8080. Close any other app using it and try again.\n");
        CLOSESOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (listen(server, 10) == SOCKET_ERROR) {
        printf("Could not start listening.\n"); CLOSESOCKET(server); return 1;
    }
    printf("\nSmartBank is running at http://localhost:8080\n");
    printf("Keep this terminal open while using the website.\nPress Ctrl+C to stop.\n\n");
    while (1) {
        socket_t client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        char request[REQUEST_SIZE];
        int received = 0, n;
        while (received < (int)sizeof(request) - 1) {
            n = recv(client, request + received, (int)sizeof(request) - 1 - received, 0);
            if (n <= 0) break;
            received += n;
            request[received] = '\0';
            char *headerEnd = strstr(request, "\r\n\r\n");
            if (headerEnd) {
                int contentLength = 0;
                char *cl = strstr(request, "Content-Length:");
                if (cl && cl < headerEnd) contentLength = atoi(cl + 15);
                int headerLen = (int)(headerEnd + 4 - request);
                if (received >= headerLen + contentLength) break;
            } else if (received > 0 && strstr(request, "\r\n")) {
                /* Continue receiving until headers are complete. */
            }
        }
        request[received] = '\0';
        char method[12] = "", path[256] = "";
        sscanf(request, "%11s %255s", method, path);
        char *body = strstr(request, "\r\n\r\n");
        if (body) body += 4; else body = "";
        char *query = strchr(path, '?');
        if (query && strncmp(path, "/api/search?", 12) != 0) *query = '\0';
        handleRequest(client, method, path, body);
        CLOSESOCKET(client);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
