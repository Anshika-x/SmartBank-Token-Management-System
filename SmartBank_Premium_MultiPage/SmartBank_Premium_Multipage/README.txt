SMARTBANK PREMIUM MULTI-PAGE
============================

Pages: Overview (index.html), Registration (register.html), Live Queue (queue.html), Service Desk (service.html), Customer Directory (customers.html), Analytics (analytics.html).

RUN IN VS CODE (Windows + MSYS2 UCRT64)
1. Extract this ZIP. Open the extracted folder in VS Code.
2. Open the MSYS2 UCRT64 terminal and navigate to this folder.
3. Compile: gcc server.c -o server.exe -lws2_32
4. Start: ./server.exe
5. Open http://localhost:8080 in Chrome or Edge.

Use the left sidebar to move between separate pages. The C program implements the queue operations and exposes API endpoints.

Note: this is an educational demo. Data is kept in memory and resets when the server stops. It is not a real banking system.
