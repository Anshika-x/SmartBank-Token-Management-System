# SmartBank — Token Management System

## About the Project

SmartBank is a banking token management system developed as a Data Structures and Algorithms (DSA) project. It uses a circular queue implemented in C to manage customer tokens efficiently.

## Features

* Customer registration and token generation
* Separate dashboards for registration, queue monitoring, service desk, customer directory, and analytics
* Call and serve the next customer
* Search customers by token number
* Track waiting and served customers
* User-friendly web interface

## Technologies Used

* **C:** Circular queue and server-side logic
* **HTML:** Website structure
* **CSS:** Styling and layout
* **JavaScript:** Interactive features

## Data Structure

The project uses a **circular queue** and follows the First-In, First-Out (FIFO) principle to serve customers in order.

## How to Run

1. Install GCC and ensure the Windows networking library is available.
2. Open the project folder in the terminal.
3. Compile the C server:
   `gcc server.c -o server.exe -lws2_32`
4. Run the server:
   `./server.exe`
5. Open `http://localhost:8080` in your browser.

## Purpose

This project demonstrates how data structures can be applied to a practical banking queue-management scenario.
